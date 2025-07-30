#include "viewer_process.hh"

#include <windowsx.h>
#include <cxlib/utils/logger.hh>

namespace
{
    ComPtr<ID3D11Texture2D> open_shared_texture_from_native_handle(
        ComPtr<ID3D11Device2> dx11_device,
        HANDLE target_shared_texture_handle,
        HANDLE owner_process_handle)
    {
        ComPtr<ID3D11Texture2D> dx11_shared_texture;

        // OpenSharedResource1(혹은 OpenSharedResourceByName)를 사용하여 client측에서 생성한 NT 핸들 획득 & 공유 텍스처 생성
        // (OpenSharedResource1 함수를 사용하는 경우, DuplicateHandle을 사용하여 전달받은 공유 텍스처 핸들을 현재 프로세스에서 유효한 핸들로 복제해야 함)
        HANDLE duplicated_handle{};
        if (!::DuplicateHandle(
            owner_process_handle, // 원본 핸들을 소유하고 있는 소스 프로세스 핸들
            target_shared_texture_handle, // IPC로 수신한 원본 핸들 값
            ::GetCurrentProcess(), // 핸들을 복제해 올 타겟 프로세스 핸들 (현재 프로세스)
            &duplicated_handle, // 복제된 핸들을 저장할 포인터
            0, // 접근 권한 (0은 원본과 동일한 권한임을 의미)
            FALSE, // 핸들 상속 여부
            DUPLICATE_SAME_ACCESS // 원본과 동일한 접근 권한으로 복제
        )) {
            CXLIB_ERROR("Failed to duplicate shared texture handle. (error: {})", ::GetLastError());
            return nullptr;
        }

        utils::unique_handle duplicated_handle_guard{ duplicated_handle }; // 핸들의 자동 해제를 위한 RAII 핸들 래퍼
        if (HRESULT hr = dx11_device->OpenSharedResource1(
            duplicated_handle_guard.get(),
            IID_PPV_ARGS(&dx11_shared_texture));
            FAILED(hr))
        {
            CXLIB_ERROR("Failed to open shared texture resource. (HRESULT: {:08X})", static_cast<uint32_t>(hr));
            return nullptr;
        }

        return dx11_shared_texture;
    }

} // namespace

viewer_process::viewer_process(int32_t frame_width, int32_t frame_height, DXGI_FORMAT frame_format)
{
    CXLIB_INFO("DX viewer process spawned (PID: {})", ::GetCurrentProcessId());
    this->_initialize(frame_width, frame_height, frame_format);
}

viewer_process::~viewer_process()
{
    CXLIB_DEBUG("Cleaning up DX viewer resources...");
    // RAII handles most cleanup...
}

void viewer_process::run()
{
    CXLIB_INFO("Entering render loop...");

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (::PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageA(&msg);
        }
        else
        {
            this->_render_frame();
        }
    } // while
}

void viewer_process::_initialize(
    const int32_t frame_width,
    const int32_t frame_height,
    const DXGI_FORMAT frame_format)
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    CXLIB_DEBUG("Connecting to IPC server...");

    if (std::shared_ptr<void> hEvent{
            ::OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, Config::RENDERER_PROCESS_INST_NAME),
            ::CloseHandle
        }; !hEvent)
    {
        CXLIB_DEBUG("No existing renderer process found. Launching new renderer...");

        // 자식 프로세스(GL 렌더러)를 별도의 콘솔 창으로 실행
        std::array<char, MAX_PATH + 20> currExePathBuff{};
        ::GetModuleFileNameA(NULL, currExePathBuff.data(), static_cast<DWORD>(currExePathBuff.size()));
        std::string cmdLine = fmt::format("\"{}\" --renderer-mode", currExePathBuff.data());

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        CXLIB_ASSERT(::CreateProcessA(
            NULL,
            cmdLine.data(),
            NULL,
            NULL,
            FALSE, // 핸들 상속 여부
            CREATE_NEW_CONSOLE,
            NULL,
            NULL,
            &si, &pi
        ));

        _renderer_process_handle.reset(pi.hProcess);
        ::CloseHandle(pi.hThread); // Thread handle not needed

        // 서버 프로세스 초기화 대기
        std::this_thread::sleep_for(2000ms);

        CXLIB_DEBUG("Launched renderer process.");
    }
    else
    {
        CXLIB_DEBUG("Found existing renderer process!");
    }

    _ipc_cli = std::make_shared<ipc_client>();
    _ipc_cli->set_disconnect_callback(
        [this]()
    {
        CXLIB_WARN("Renderer process disconnected! closing viewer window...");
        ::PostMessageA(_viewer_hwnd.get(), WM_CLOSE, 0, 0);
    });

    if (!_ipc_cli->connect(Config::RENDERER_SERVER_NAME)) {
        throw std::runtime_error{ "connect failed" };
    }

    CXLIB_DEBUG("Connected! sending init request...");

    // 초기화 요청 전송

    packet_builder<ipc_proto::packets::init_request_t> init_req{ ipc_proto::packet_type::init_request };
    init_req.body()->frame_width = frame_width;
    init_req.body()->frame_height = frame_height;
    init_req.body()->frame_format = frame_format;

    std::vector<uint8_t> init_rep_bytes;
    if (std::errc{} != _ipc_cli->send_request_sync(
        init_req.data(),
        init_req.size(),
        init_rep_bytes))
    {
        throw std::runtime_error{ "send init request failed" };
    }

    packet_view init_rep_pck_view{ init_rep_bytes.data(), init_rep_bytes.size() };
    const auto init_rep = init_rep_pck_view.body<ipc_proto::packets::init_response_t>();
    CXLIB_DEBUG("Received init response. (pid: {}, adapter: {:x}-{:x}, resource handle: {})"
        , init_rep->renderer_process_id
        , init_rep->target_adapter_luid.HighPart
        , init_rep->target_adapter_luid.LowPart
        , init_rep->shared_texture_handle
    );

    if (!_renderer_process_handle) {
        CXLIB_DEBUG("Opening renderer process handle...");
        _renderer_process_handle.reset(::OpenProcess(
            PROCESS_DUP_HANDLE | SYNCHRONIZE,
            FALSE,
            init_rep->renderer_process_id
        ));
        CXLIB_ASSERT(_renderer_process_handle.get());
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    CXLIB_DEBUG("Creating d3d window...");

    static const auto pfnNativeWndProc = +[](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT
    {
        viewer_process* pThis{ nullptr };
        if (message == WM_NCCREATE) {
            CREATESTRUCT* pCreate = _CXLIB utils::bit_cast<CREATESTRUCT*>(lParam);
            pThis = _CXLIB utils::bit_cast<viewer_process*>(pCreate->lpCreateParams);
            ::SetWindowLongPtrA(hWnd, GWLP_USERDATA, _CXLIB utils::bit_cast<LONG_PTR>(pThis));
        }
        else {
            pThis = _CXLIB utils::bit_cast<viewer_process*>(::GetWindowLongPtrA(hWnd, GWLP_USERDATA));
        }

        return (pThis && pThis->_initialized)
            ? pThis->_wnd_proc(hWnd, message, wParam, lParam)
            : ::DefWindowProcA(hWnd, message, wParam, lParam);
    };

    static_assert(std::is_same_v<std::remove_const_t<decltype(pfnNativeWndProc)>, WNDPROC>, "WndProc type mismatch!");

    WNDCLASSA wc = { 0, };
    wc.lpfnWndProc = pfnNativeWndProc;
    wc.lpszClassName = "MyDX11ViewerWndClass";
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    ::RegisterClassA(&wc);

    RECT windowRect = { 0, 0, frame_width, frame_height };
    const DWORD windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    ::AdjustWindowRect(&windowRect, windowStyle, FALSE);

    _viewer_hwnd.reset(::CreateWindowExA(
        0,
        wc.lpszClassName,
        fmt::format("DX Viewer (PID: {})", ::GetCurrentProcessId()).c_str(),
        windowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL,
        NULL,
        NULL,
        this // Pass 'this' to WndProc
    ));
    CXLIB_ASSERT(_viewer_hwnd.get());

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    CXLIB_DEBUG("Initializing D3D resources...");

    // NOTE: 반드시 CreateDXGIFactory2 함수를 사용해서 DXGI 1.2 버전 이상의 DXGI 팩토리(`IDXGIFactory`)를 생성해줘야 함.
    // (`ID3D11Device::CreateTexture2D: D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX is only available for devices created off of Dxgi1.1 factories or later.` D3D11 오류 방지)
    ComPtr<IDXGIFactory2> dxgiFactory2;
    ASSERT_HR(::CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory2)));

    ComPtr<IDXGIAdapter> dxgiAdapter0;
    for (UINT i = 0; dxgiFactory2->EnumAdapters(i, &dxgiAdapter0) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC desc;
        dxgiAdapter0->GetDesc(&desc);
        if (std::memcmp(&desc.AdapterLuid, &init_rep->target_adapter_luid, sizeof(LUID)) == 0) {
            _dxgi_adapter = dxgiAdapter0;
            break;
        }
    }

    if (!_dxgi_adapter) {
        throw std::runtime_error{ "Matching adapter not found" };
    }

    UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    ComPtr<ID3D11Device> dx11Device0;
    ComPtr<ID3D11DeviceContext> dx11DeviceContext0;

    ASSERT_HR(::D3D11CreateDevice(
        _dxgi_adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        NULL,
        deviceFlags,
        NULL,
        0,
        D3D11_SDK_VERSION,
        &dx11Device0,
        NULL,
        &dx11DeviceContext0
    ));

    // Convert `ID3D11Device` -> `ID3D11Device2` (Higher version object)
    ASSERT_HR(dx11Device0.As(&_dx11_device2));

    // Convert `ID3D11DeviceContext` -> `ID3D11DeviceContext2` (Higher version object)
    ASSERT_HR(dx11DeviceContext0.As(&_dx11_device_context2));

    _dx11_shared_texture = open_shared_texture_from_native_handle(
        _dx11_device2,
        init_rep->shared_texture_handle,
        _renderer_process_handle.get()
    );
    if (_dx11_shared_texture) {
        CXLIB_DEBUG("Successfully opened shared texture from native handle. (handle: {})", init_rep->shared_texture_handle);
    } else {
        throw std::runtime_error{ "Failed to open shared texture from native handle" };
    }

    // 획득한 공유 텍스처에서 KeyedMutex 인터페이스 획득
    ASSERT_HR(_dx11_shared_texture.As(&_dxgi_keyed_mutex));

    // 화면 렌더링용 screen 텍스처 생성
    D3D11_TEXTURE2D_DESC screenTexDesc{};
    screenTexDesc.Width = static_cast<UINT>(frame_width);
    screenTexDesc.Height = static_cast<UINT>(frame_height);
    screenTexDesc.MipLevels = 1;
    screenTexDesc.ArraySize = 1;
    screenTexDesc.Format = frame_format;
    screenTexDesc.SampleDesc.Count = 1;
    screenTexDesc.SampleDesc.Quality = 0;
    screenTexDesc.Usage = D3D11_USAGE_DEFAULT;
    screenTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    screenTexDesc.CPUAccessFlags = 0;
    screenTexDesc.MiscFlags = 0;
    ASSERT_HR(_dx11_device2->CreateTexture2D(
        &screenTexDesc,
        nullptr,
        &_dx11_screen_texture
    ));

    // DXGI 1.2 스왑 체인 디스크립션(DXGI_SWAP_CHAIN_DESC1) 설정
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc1{};
    swapchainDesc1.Width = swapchainDesc1.Height = 0; // NOTE: 0으로 설정 시 창 크기로 자동 맞춤
    swapchainDesc1.Format = frame_format;
    swapchainDesc1.Stereo = FALSE;
    swapchainDesc1.SampleDesc.Count = 1;
    swapchainDesc1.SampleDesc.Quality = 0;
    swapchainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc1.BufferCount = 2; // 더블 버퍼링 사용
    swapchainDesc1.Scaling = DXGI_SCALING_STRETCH;
    swapchainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // NOTE: 현대적인 플립 모델 사용
    swapchainDesc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchainDesc1.Flags = 0;

    // 스왑 체인 생성 (CreateSwapChainForHwnd 사용)
    ASSERT_HR(dxgiFactory2->CreateSwapChainForHwnd(
        _dx11_device2.Get(),
        _viewer_hwnd.get(),
        &swapchainDesc1,
        nullptr, // 전체화면 디스크립션 (사용 X)
        nullptr, // 출력 제한 없음
        &_dx11_swapchain1
    ));

    // 스왑 체인의 백 버퍼를 렌더 타겟 뷰로 설정
    ComPtr<ID3D11Texture2D> backBuffer;
    ASSERT_HR(_dx11_swapchain1->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
    ASSERT_HR(_dx11_device2->CreateRenderTargetView(backBuffer.Get(), NULL, &_dx11_rtv));

    // Full-screen Quad 렌더링을 위한 셰이더 및 리소스 생성
    // OpenGL의 공유 텍스처를 y축 기준으로 뒤집어서 렌더링하기 위한 screen quad 셰이더 코드
    static const std::string shaderCode = R"hlsl(
        Texture2D tx : register(t0);
        SamplerState smp : register(s0);

        struct VS_OUT {
            float4 pos : SV_POSITION;
            float2 uv : TEXCOORD;
        };

        VS_OUT VS(uint id : SV_VertexID) {
            VS_OUT output;
            // Full-screen triangle UVs: (0,0), (2,0), (0,2)
            output.uv = float2((id << 1) & 2, id & 2); 
            // Full-screen triangle positions: (-1,1), (3,1), (-1,-3)
            output.pos = float4(output.uv * 2.0f - 1.0f, 0.0f, 1.0f);
            // Flip Y for correct rendering
            output.pos.y = -output.pos.y;
            return output;
        }

        float4 PS(VS_OUT input) : SV_TARGET {
            return tx.Sample(smp, float2(input.uv.x, 1.0 - input.uv.y)); // flip Y-axis
            // return tx.Sample(smp, input.uv);
        }
    )hlsl";

    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    HRESULT hr = ::D3DCompile(
        shaderCode.c_str(), shaderCode.size(),
        nullptr, nullptr, nullptr,
        "VS",
        "vs_5_0",
        0, 0,
        &vsBlob, &errBlob
    );
    if (FAILED(hr)) {
        throw std::runtime_error{ fmt::format(
            "Failed to compile vertex shader({}): {}"
            , hr
            , static_cast<char*>(errBlob->GetBufferPointer())
        ) };
    }

    hr = ::D3DCompile(
        shaderCode.c_str(), shaderCode.size(),
        nullptr, nullptr, nullptr,
        "PS",
        "ps_5_0",
        0, 0,
        &psBlob,
        &errBlob
    );
    if (FAILED(hr)) {
        throw std::runtime_error{ fmt::format(
            "Failed to compile pixel shader({}): {}"
            , hr
            , static_cast<char*>(errBlob->GetBufferPointer())
        ) };
    }

    ASSERT_HR(_dx11_device2->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &_dx11_vertex_shader));
    ASSERT_HR(_dx11_device2->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &_dx11_pixel_shader));
    ASSERT_HR(_dx11_device2->CreateShaderResourceView(_dx11_screen_texture.Get(), NULL, &_dx11_srv)); // Equivalent of: `glBindTexture`+ `sampler2D`

    D3D11_SAMPLER_DESC sampDesc{};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    ASSERT_HR(_dx11_device2->CreateSamplerState(&sampDesc, &_dx11_sampler_state));

    _initialized = true;
    CXLIB_INFO("Initialization complete.");
}

LRESULT viewer_process::_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CLOSE: {
        CXLIB_DEBUG("Closing viewer window...");
        break;
    }
    case WM_DESTROY: {
        ::PostQuitMessage(0);
        return 0;
    }
    case WM_ERASEBKGND: {
        return 1; // Prevent flickering
    }
    case WM_SIZE:
    {
        const int32_t
            width = static_cast<int32_t>(LOWORD(lParam)),
            height = static_cast<int32_t>(HIWORD(lParam));

        if (width <= 0 || height <= 0) {
            break;
        }

        CXLIB_TRACE("frame resize request: {}x{}", width, height);

        packet_builder<ipc_proto::packets::frame_resize_request_t> req{ ipc_proto::packet_type::frame_resize_request };
        req.body()->width = width;
        req.body()->height = height;

        std::vector<uint8_t> rep_bytes;
        if (std::errc{} != _ipc_cli->send_request_sync(
            req.data(),
            req.size(),
            rep_bytes))
        {
            CXLIB_ERROR("frame resize request failed.");
            return 0;
        }

        packet_view pck_view{ rep_bytes.data(), rep_bytes.size() };
        const auto resize_rep = pck_view.body<ipc_proto::packets::frame_resize_response_t>();
        CXLIB_TRACE("frame resize response -> resource handle: {}", resize_rep->shared_texture_handle);

        D3D11_TEXTURE2D_DESC screenTexDesc{};
        _dx11_screen_texture->GetDesc(&screenTexDesc);
        
        // 렌더 타겟을 파이프라인에서 분리해야 스왑체인 리사이즈 가능
        _dx11_device_context2->OMSetRenderTargets(0, nullptr, nullptr);

        //
        // 렌더링 리소스 해제 (해제 순서에 유의)
        //

        _dx11_rtv.Reset();
        _dx11_srv.Reset();
        _dxgi_keyed_mutex.Reset();
        _dx11_shared_texture.Reset();
        _dx11_screen_texture.Reset();

        //
        // 렌더링 리소스 리사이즈(재생성) 시작
        // (모든 버퍼 참조(RTV 등)가 해제된 후 수행되어야 함)
        //

        // 스왑체인 리사이즈
        DXGI_SWAP_CHAIN_DESC1 swapchainDesc1{};
        _dx11_swapchain1->GetDesc1(&swapchainDesc1);
        HRESULT hr = _dx11_swapchain1->ResizeBuffers(
            swapchainDesc1.BufferCount,
            width,
            height,
            swapchainDesc1.Format,
            swapchainDesc1.Flags
        );
        ASSERT_HR(hr);

        // 공유 텍스처 재생성
        _dx11_shared_texture = open_shared_texture_from_native_handle(
            _dx11_device2,
            resize_rep->shared_texture_handle, // IPC로 받은 새로운 핸들
            _renderer_process_handle.get()
        );
        if (_dx11_shared_texture) {
            CXLIB_DEBUG("Successfully opened shared texture from native handle. (handle: {})", resize_rep->shared_texture_handle);
        } else {
            throw std::runtime_error{ "Failed to open shared texture from native handle" };
        }

        // KeyedMutex 재생성
        ASSERT_HR(_dx11_shared_texture.As(&_dxgi_keyed_mutex));

        // 로컬 Screen 텍스처 재생성
        screenTexDesc.Width = static_cast<UINT>(width);
        screenTexDesc.Height = static_cast<UINT>(height);
        ASSERT_HR(_dx11_device2->CreateTexture2D(
            &screenTexDesc, 
            nullptr, 
            &_dx11_screen_texture
        ));

        // 새로운 Screen 텍스처에 대한 SRV 재생성
        ASSERT_HR(_dx11_device2->CreateShaderResourceView(
            _dx11_screen_texture.Get(), 
            nullptr, 
            &_dx11_srv
        ));

        // 리사이즈된 스왑체인의 백버퍼에 대한 RTV 재생성
        ComPtr<ID3D11Texture2D> backBuffer;
        ASSERT_HR(_dx11_swapchain1->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
        ASSERT_HR(_dx11_device2->CreateRenderTargetView(
            backBuffer.Get(), 
            nullptr, 
            &_dx11_rtv
        ));

        CXLIB_DEBUG("All DX resources have been successfully synchronized.");
        return 0;
    }
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        const WORD vkcode = LOWORD(wParam); // vk code
        const WORD key_flags = HIWORD(lParam); // key flags
        const BOOL is_ext_key = (key_flags & KF_EXTENDED) == KF_EXTENDED; // extended-key flag, 1 if scancode has 0xE0 prefix
        const BOOL is_key_released = (key_flags & KF_UP) == KF_UP; // transition-state flag, 1 on keyup
        const WORD scan_code = MAKEWORD(LOBYTE(key_flags), (is_ext_key) ? 0xE0 : 0x00); // scan code
        const BOOL was_key_down = (key_flags & KF_REPEAT) == KF_REPEAT; // previous key-state flag, 1 on autorepeat
        const WORD repeat_count = LOWORD(lParam); // repeat count, > 0 if several keydown messages was combined into one message

        auto key = ipc_proto::translate_vkcode(vkcode);
        if (key != ipc_proto::KEY_UNKNOWN)
        {
            auto action = is_key_released ? ipc_proto::ACTION_RELEASE : (was_key_down && repeat_count ? ipc_proto::ACTION_REPEAT : ipc_proto::ACTION_PRESS);
            ipc_proto::modifier_button_type mods{};
            if (GetKeyState(VK_SHIFT) & 0x8000) { mods |= ipc_proto::MOD_KEY_SHIFT; }
            if (GetKeyState(VK_CONTROL) & 0x8000) { mods |= ipc_proto::MOD_KEY_CTRL; }
            if (GetKeyState(VK_MENU) & 0x8000) { mods |= ipc_proto::MOD_KEY_ALT; }
            if (GetKeyState(VK_CAPITAL) & 0x0001) { mods |= ipc_proto::MOD_KEY_CAPSLOCK; }
            if (GetKeyState(VK_NUMLOCK) & 0x0001) { mods |= ipc_proto::MOD_KEY_NUMLOCK; }

            CXLIB_TRACE("key event -> key: {}, scancode: {}, action: {}, mods: 0x{:X}"
                , static_cast<int>(key)
                , scan_code
                , static_cast<int>(action)
                , static_cast<std::underlying_type_t<ipc_proto::modifier_button_type>>(mods)
            );
        }

        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    {
        const int32_t x = GET_X_LPARAM(lParam);
        const int32_t y = GET_Y_LPARAM(lParam);

        ipc_proto::mouse_button_type btn = [msg]() -> std::optional<ipc_proto::mouse_button_type> {
            switch (msg) {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                return ipc_proto::MOUSE_L;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                return ipc_proto::MOUSE_R;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return ipc_proto::MOUSE_M;
            default:
                return std::nullopt;
            }
        }().value();

        ipc_proto::button_action_type action = [msg]() -> std::optional<ipc_proto::button_action_type> {
            switch (msg) {
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
                return ipc_proto::ACTION_PRESS;
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
                return ipc_proto::ACTION_RELEASE;
            default:
                return std::nullopt;
            }
        }().value();

        ipc_proto::modifier_button_type mods{};
        if (wParam & MK_CONTROL) { mods |= ipc_proto::MOD_KEY_CTRL; }
        if (wParam & MK_SHIFT) { mods |= ipc_proto::MOD_KEY_SHIFT; }
        if (wParam & MK_LBUTTON) { mods |= ipc_proto::MOD_MOUSE_L; }
        if (wParam & MK_RBUTTON) { mods |= ipc_proto::MOD_MOUSE_R; }
        if (wParam & MK_MBUTTON) { mods |= ipc_proto::MOD_MOUSE_M; }

        CXLIB_TRACE("mouse event -> btn: {}, action: {}, mods: 0x{:X}"
            , static_cast<int>(btn)
            , static_cast<int>(action)
            , static_cast<std::underlying_type_t<ipc_proto::modifier_button_type>>(mods)
        );

        packet_builder<ipc_proto::packets::mouse_button_event_t> pck{ ipc_proto::packet_type::mouse_button_event };
        pck.body()->x = x;
        pck.body()->y = y;
        pck.body()->button = btn;
        pck.body()->action = action;
        pck.body()->mods = mods;
        CXLIB_ASSERT(std::errc{} == _ipc_cli->send_notify(pck.data(), pck.size()));

        return 0;
    }
    case WM_MOUSEMOVE:
    {
        const int32_t x = GET_X_LPARAM(lParam);
        const int32_t y = GET_Y_LPARAM(lParam);

        ipc_proto::modifier_button_type mods{};
        if (wParam & MK_CONTROL) { mods |= ipc_proto::MOD_KEY_CTRL; }
        if (wParam & MK_SHIFT) { mods |= ipc_proto::MOD_KEY_SHIFT; }
        if (wParam & MK_LBUTTON) { mods |= ipc_proto::MOD_MOUSE_L; }
        if (wParam & MK_RBUTTON) { mods |= ipc_proto::MOD_MOUSE_R; }
        if (wParam & MK_MBUTTON) { mods |= ipc_proto::MOD_MOUSE_M; }

        CXLIB_TRACE("mouse move event -> pos: ({}, {}), mods: 0x{:X}"
            , x, y
            , static_cast<std::underlying_type_t<ipc_proto::modifier_button_type>>(mods)
        );

        packet_builder<ipc_proto::packets::mouse_move_event_t> pck{ ipc_proto::packet_type::mouse_move_event };
        pck.body()->x = x;
        pck.body()->y = y;
        pck.body()->mods = mods;
        CXLIB_ASSERT(std::errc{} == _ipc_cli->send_notify(pck.data(), pck.size()));

        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        const int32_t raw_scroll_delta = GET_WHEEL_DELTA_WPARAM(wParam);

        // GLFW와 동일한 스크롤 값 계산
        // GLFW는 스크롤 한 단위를 1.0 또는 -1.0으로 정규화하므로,
        // raw_delta 값을 WHEEL_DELTA로 나누어준다. (Normalization)
        const float yoffset = static_cast<float>(raw_scroll_delta) / static_cast<float>(WHEEL_DELTA);

        CXLIB_TRACE("mouse scroll event -> yoffset: {}", yoffset);

        packet_builder<ipc_proto::packets::mouse_scroll_event_t> pck{ ipc_proto::packet_type::mouse_scroll_event };
        pck.body()->yoffset = yoffset;
        CXLIB_ASSERT(std::errc{} == _ipc_cli->send_notify(pck.data(), pck.size()));

        return 0;
    }
    } // switch

    return ::DefWindowProcA(hWnd, msg, wParam, lParam);
}

void viewer_process::_render_frame()
{
    // 획득해둔 KeyedMutex를 사용하여 렌더링 타이밍 동기화 수행
    // (매 프레임 렌더링 전, GL 렌더러 측이 텍스처 쓰기를 완료할 때까지 대기)
    // -> 뮤텍스를 즉시 얻지 못한 경우, GL 렌더러 측에서 아직 작업 중이거나 렌더링된 프레임이 없음을 의미
    // 
    // NOTE: AcquireSync 함수 사용 시 단순 성공 여부 판단을 위해 SUCCEEDED 매크로만 사용할 경우, 
    //       WAIT_OBJECT_0 반환값을 제외한 나머지 반환 상태값을 제대로 감지하지 못할 수 있음에 유의.
    //       AcquireSync 함수는 다음과 같은 DWORD 상수들을 반환할 수 있다:
    //         - WAIT_OBJECT_0  : KeyedMutex를 성공적으로 획득 -> 렌더링 작업을 계속 진행할 수 있음. (S_OK와 동일한 값)
    //         - WAIT_TIMEOUT   : 지정된 키가 해제되기 전에 타임아웃 간격이 경과했음을 의미.
    //         - WAIT_ABANDONED : SharedSurface와 KeyedMutex가 더 이상 일관된 상태가 아님. 
    //                            이 경우, KeyedMutex와 SharedSurface 둘 다 해제한 후 재생성해야 함.
    //       Ref: https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgikeyedmutex-acquiresync
    switch (
        const HRESULT sync_hr = _dxgi_keyed_mutex->AcquireSync(0/* Key */, 10/* Wait Timeout */);
        sync_hr
    ) {
    case WAIT_OBJECT_0: // KeyedMutex를 성공적으로 획득했으므로 렌더링 작업 진행 가능
        // Shared 텍스처를 Screen 텍스처로 복사 (락 점유 시간을 최소화하기 위해 별도 텍스처로 데이터를 복사한 뒤 렌더링 수행)
        _dx11_device_context2->CopyResource(_dx11_screen_texture.Get(), _dx11_shared_texture.Get());
        _dxgi_keyed_mutex->ReleaseSync(0/* Key */); // 텍스처 사용이 끝났으므로 KeyedMutex 잠금 해제
        break;
    case WAIT_TIMEOUT: // KeyedMutex를 획득하지 못했으므로 렌더링 작업을 건너뜀
        return;
    case WAIT_ABANDONED: // KeyedMutex가 더 이상 일관된 상태가 아님. 이 경우, KeyedMutex와 SharedSurface 둘 다 해제한 후 재생성해야 함.
        throw std::runtime_error{ "Failed to acquire keyed mutex. (keyed mutex is no longer in a consistent state)" };
        return;
    }

    RECT clientRect;
    ::GetClientRect(_viewer_hwnd.get(), &clientRect);
    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(clientRect.right), static_cast<float>(clientRect.bottom), 0.0f, 1.0f };

    const std::array<float, 4> clearColor{ 0.5f, 0.5f, 0.5f, 1.0f };
    _dx11_device_context2->ClearRenderTargetView(_dx11_rtv.Get(), clearColor.data());

    _dx11_device_context2->OMSetRenderTargets(1, _dx11_rtv.GetAddressOf(), NULL);
    _dx11_device_context2->RSSetViewports(1, &viewport);

    _dx11_device_context2->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    _dx11_device_context2->IASetInputLayout(NULL); // No input buffer needed for this VS trick

    _dx11_device_context2->VSSetShader(_dx11_vertex_shader.Get(), NULL, 0);
    _dx11_device_context2->PSSetShader(_dx11_pixel_shader.Get(), NULL, 0);
    _dx11_device_context2->PSSetShaderResources(0, 1, _dx11_srv.GetAddressOf());
    _dx11_device_context2->PSSetSamplers(0, 1, _dx11_sampler_state.GetAddressOf());

    _dx11_device_context2->Draw(3, 0); // full-screen quad 렌더링 시작 (3 vertices)

    // Alternative: Use IDXGISwapChain1::Present1
    _dx11_swapchain1->Present(1, 0); // VSync On

    // log fps
    //static int frameCount = 0;
    //++frameCount;
    //static auto lastTime = std::chrono::steady_clock::now();
    //const auto currentTime = std::chrono::steady_clock::now();
    //if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime) >= 1s)
    //{
    //    CXLIB_DEBUG("Render FPS: {}", frameCount);
    //    lastTime = currentTime;
    //    frameCount = 0;
    //}
}
