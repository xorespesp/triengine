#include "viewer_process.hh"

#include <windowsx.h>
#include <cxlib/utils/logger.hh>

viewer_process::viewer_process()
{
    CXLIB_INFO("DX viewer process spawned (PID: {})", ::GetCurrentProcessId());

    this->_connect_to_renderer_process();
    this->_init_d3d_window();
    this->_init_d3d_resources();
    _initialized = true;

    CXLIB_INFO("Initialization complete.");
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

void viewer_process::_connect_to_renderer_process()
{
    if (std::shared_ptr<void> hEvent{
            ::OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, Config::RENDERER_PROCESS_INST_NAME),
            ::CloseHandle
        }; !hEvent)
    {
        CXLIB_DEBUG("No existing renderer process found. Launching new renderer...");

        // 자식 프로세스(GL 렌더러)를 별도의 콘솔 창으로 실행
        std::array<char, MAX_PATH + 20> currExePathBuff{};
        ::GetModuleFileNameA(NULL, currExePathBuff.data(), currExePathBuff.size());
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
        ), "Failed to launch renderer process");

        _renderer_process_handle.reset(pi.hProcess);
        ::CloseHandle(pi.hThread); // Thread handle not needed

        // 서버 프로세스 초기화 대기
        std::this_thread::sleep_for(1000ms);

        CXLIB_DEBUG("Launched renderer process.");
    }
    else
    {
        CXLIB_DEBUG("Found existing renderer process!");
    }

    SPDLOG_DEBUG("Connecting to IPC server...");
    _ipc_cli = std::make_shared<ipc_client>();
    _ipc_cli->set_notify_callback(
        [this](
            [[maybe_unused]] const uint32_t id, 
            const std::string_view data)
        {
            packet_view pck{ data.data(), data.size() };
            if (!pck.is_valid()) {
                CXLIB_WARN("Got invalid packet");
                return;
            }

            switch (pck.type()) {
            case ipc_proto::packet_type::shared_render_context:
            {
                {
                    std::scoped_lock lk{ _shared_info_mtx };
                    _shared_info = *pck.body<ipc_proto::packets::shared_render_context_t>();
                }
                _shared_info_cv.notify_all();
                break;
            }
            default:
                break;
            }
        });

    _ipc_cli->set_disconnect_callback(
        [this]()
        {
            CXLIB_WARN("Renderer process disconnected! closing viewer window...");
            ::PostMessageA(_viewer_hwnd.get(), WM_CLOSE, 0, 0);
        });

    if (!_ipc_cli->connect(Config::RENDERER_SERVER_NAME)) {
        throw std::runtime_error{ "connect failed" };
    }
    CXLIB_DEBUG("Connected!");

    // 공유 정보 수신 대기
    CXLIB_INFO("Waiting for data...");
    std::unique_lock lk{ _shared_info_mtx };
    _shared_info_cv.wait(lk, [this] { return _shared_info.has_value(); });
    CXLIB_DEBUG("Received data. (pid: {}, adapter: {:x}-{:x}, resource handle: {}, initial size: {}x{})"
        , _shared_info->renderer_process_id
        , _shared_info->target_adapter_luid.HighPart
        , _shared_info->target_adapter_luid.LowPart
        , _shared_info->shared_texture_handle
        , _shared_info->shared_texture_width
        , _shared_info->shared_texture_height
    );

    if (!_renderer_process_handle) {
        CXLIB_DEBUG("Opening renderer process handle...");
        _renderer_process_handle.reset(::OpenProcess(
            PROCESS_DUP_HANDLE | SYNCHRONIZE,
            FALSE,
            _shared_info->renderer_process_id
        ));
        CXLIB_ASSERT(_renderer_process_handle.get());
    }
}

void viewer_process::_init_d3d_window()
{
    //CXLIB_TRACE("{} ENTER", __func__);

    static const auto pfnNativeWndProc = +[](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT
    {
        viewer_process* pThis{ nullptr };
        if (message == WM_NCCREATE) {
            CREATESTRUCT* pCreate = _CXLIB utils::bit_cast<CREATESTRUCT*>(lParam);
            pThis = _CXLIB utils::bit_cast<viewer_process*>(pCreate->lpCreateParams);
            ::SetWindowLongPtrA(hWnd, GWLP_USERDATA, _CXLIB utils::bit_cast<LONG_PTR>(pThis));
        } else {
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

    RECT windowRect = { 0, 0, _shared_info->shared_texture_width, _shared_info->shared_texture_height };
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
}

void viewer_process::_init_d3d_resources()
{
    //CXLIB_TRACE("{} ENTER", __func__);

    // IMPORTANT NOTE: 반드시 CreateDXGIFactory2 함수를 사용해서 DXGI 1.2 버전 이상의 DXGI 팩토리(`IDXGIFactory`)를 생성해줘야 함.
    // (안 그러면 `D3D11 ERROR: ID3D11Device::OpenSharedResource1: Returning E_INVALIDARG, meaning invalid parameters were passed` 오류 발생)
    ComPtr<IDXGIFactory2> dxgiFactory2;
    ASSERT_HR(::CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory2)));

    ComPtr<IDXGIAdapter> dxgiAdapter0;
    for (UINT i = 0; dxgiFactory2->EnumAdapters(i, &dxgiAdapter0) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC desc;
        dxgiAdapter0->GetDesc(&desc);
        if (std::memcmp(&desc.AdapterLuid, &_shared_info->target_adapter_luid, sizeof(LUID)) == 0) {
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

    // ID3D11Device -> ID3D11Device2 (상위 버전 오브젝트)로 변환
    ASSERT_HR(dx11Device0.As(&_dx11_device2));

    // ID3D11DeviceContext -> ID3D11DeviceContext2 (상위 버전 오브젝트)로 변환
    ASSERT_HR(dx11DeviceContext0.As(&_dx11_device_context2));

    // OpenSharedResource1 함수(혹은 OpenSharedResourceByName 함수)를 사용하여 client측에서 생성한 NT 핸들 획득 & 공유 텍스처 생성
    // OpenSharedResource1 함수를 사용하는 경우, DuplicateHandle을 사용하여 전달받은 공유 텍스처 핸들을 현재 프로세스에서 유효한 핸들로 복제해야 함
    HANDLE duplicatedSharedTextureHandle{};
    if (!::DuplicateHandle(
        _renderer_process_handle.get(), // 원본 핸들이 속한 프로세스 (자식/렌더러)
        _shared_info->shared_texture_handle, // 원본 핸들 값 (IPC로 수신)
        ::GetCurrentProcess(), // 핸들을 복제해 올 대상 프로세스 (부모/뷰어)
        &duplicatedSharedTextureHandle, // 복제된 핸들을 저장할 변수
        0, // 접근 권한 (0은 원본과 동일한 권한)
        FALSE, // 핸들 상속 여부
        DUPLICATE_SAME_ACCESS // 옵션 (원본과 동일한 접근 권한으로 복제)
    )) {
        throw std::runtime_error{ "Failed to duplicate client resource handle" };
    }
    ASSERT_HR(_dx11_device2->OpenSharedResource1(
        duplicatedSharedTextureHandle,
        IID_PPV_ARGS(&_dx11_shared_texture)
    ));
    CXLIB_DEBUG("Successfully acquired client resource. (handle: {} -> {})"
        , _shared_info->shared_texture_handle
        , duplicatedSharedTextureHandle
    );
    // 리소스를 여는 데 성공했다면, 복제된 핸들은 더이상 필요 없으므로 정리
    ::CloseHandle(duplicatedSharedTextureHandle);

    // 획득한 공유 텍스처에서 KeyedMutex 인터페이스 획득
    ASSERT_HR(_dx11_shared_texture.As(&_dxgi_keyed_mutex));

    // 화면 렌더링용 screen 텍스처 생성
    D3D11_TEXTURE2D_DESC screenTexDesc{};
    screenTexDesc.Width = _shared_info->shared_texture_width;
    screenTexDesc.Height = _shared_info->shared_texture_height;
    screenTexDesc.MipLevels = 1;
    screenTexDesc.ArraySize = 1;
    screenTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    screenTexDesc.SampleDesc.Count = 1;
    screenTexDesc.SampleDesc.Quality = 0;
    screenTexDesc.Usage = D3D11_USAGE_DEFAULT;
    screenTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    screenTexDesc.CPUAccessFlags = 0;
    screenTexDesc.MiscFlags = 0;
    ASSERT_HR(_dx11_device2->CreateTexture2D(&screenTexDesc, NULL, &_dx11_screen_texture));

    // DXGI 1.2 스왑 체인 디스크립션(DXGI_SWAP_CHAIN_DESC1) 설정
    DXGI_SWAP_CHAIN_DESC1 scd1{};
    scd1.Width = 0; // 0으로 설정 시 창 크기에 자동으로 맞춰짐
    scd1.Height = 0;
    scd1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd1.Stereo = FALSE;
    scd1.SampleDesc.Count = 1;
    scd1.SampleDesc.Quality = 0;
    scd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd1.BufferCount = 2; // 더블 버퍼링
    scd1.Scaling = DXGI_SCALING_STRETCH;
    scd1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 현대적인 플립 모델 사용
    scd1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    scd1.Flags = 0;

    // CreateSwapChainForHwnd 함수를 사용하여 스왑 체인 생성
    ASSERT_HR(dxgiFactory2->CreateSwapChainForHwnd(
        _dx11_device2.Get(),
        _viewer_hwnd.get(),
        &scd1,
        nullptr, // 전체화면 디스크립션은 사용하지 않음
        nullptr, // 출력 제한 없음
        &_dx11_swapchain1
    ));

    ComPtr<ID3D11Texture2D> backBuffer;
    ASSERT_HR(_dx11_swapchain1->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
    ASSERT_HR(_dx11_device2->CreateRenderTargetView(backBuffer.Get(), NULL, &_dx11_rtv));

    // Full-screen Quad 렌더링을 위한 셰이더 및 리소스 생성
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
            return tx.Sample(smp, float2(input.uv.x, 1.0 - input.uv.y)); // return tx.Sample(smp, input.uv);
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
    ASSERT_HR(_dx11_device2->CreateShaderResourceView(_dx11_screen_texture.Get(), NULL, &_dx11_srv));

    D3D11_SAMPLER_DESC sampDesc{};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    ASSERT_HR(_dx11_device2->CreateSamplerState(&sampDesc, &_dx11_sampler_state));
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

        packet_buffer<ipc_proto::packets::frame_resize_request_t> req{ ipc_proto::packet_type::frame_resize_request };
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
        const auto rep = pck_view.body<ipc_proto::packets::frame_resize_response_t>();
        CXLIB_TRACE("frame resize response -> resource handle: {}", rep->shared_texture_handle);

        // 렌더링 리소스 해제 (해제 순서가 매우 중요함)
        //    - 렌더 타겟을 파이프라인에서 분리해야 스왑체인 리사이즈 가능
        _dx11_device_context2->OMSetRenderTargets(0, nullptr, nullptr);
        _dx11_rtv.Reset(); // 백버퍼에 대한 RTV 해제
        _dx11_srv.Reset(); // Screen 텍스처에 대한 SRV 해제
        _dxgi_keyed_mutex.Reset(); // 공유 텍스처에 대한 Mutex 해제
        _dx11_shared_texture.Reset(); // 공유 텍스처 자체를 해제
        _dx11_screen_texture.Reset(); // 복사 대상이었던 로컬 Screen 텍스처 해제

        // 스왑체인 리사이즈
        //    - 모든 버퍼 참조(RTV 등)가 해제된 후 호출해야 함
        HRESULT hr = _dx11_swapchain1->ResizeBuffers(
            2, // 버퍼 개수 (기존과 동일)
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM, // 포맷 (기존과 동일)
            0
        );
        ASSERT_HR(hr);

        // 새로운 핸들로 공유 텍스처 다시 초기화
        //    - DuplicateHandle은 클라이언트 프로세스와 동일한 어댑터에서 실행되므로 필수
        HANDLE duplicatedSharedTextureHandle{};
        if (!::DuplicateHandle(
            _renderer_process_handle.get(),
            rep->shared_texture_handle, // IPC로 받은 새로운 핸들
            ::GetCurrentProcess(),
            &duplicatedSharedTextureHandle,
            0, FALSE, DUPLICATE_SAME_ACCESS
        )) {
            throw std::runtime_error{ "Failed to duplicate new client resource handle" };
        }

        ASSERT_HR(_dx11_device2->OpenSharedResource1(
            duplicatedSharedTextureHandle,
            IID_PPV_ARGS(&_dx11_shared_texture) // _dx11SharedTexture를 새로운 리소스로 교체
        ));
        ::CloseHandle(duplicatedSharedTextureHandle); // 리소스를 열었으면 복제된 핸들은 바로 닫기

        // 새로운 크기로 렌더링 리소스 재생성
        ASSERT_HR(_dx11_shared_texture.As(&_dxgi_keyed_mutex)); // 새로운 Mutex 획득

        // 로컬 Screen 텍스처 재생성
        D3D11_TEXTURE2D_DESC screenTexDesc{};
        screenTexDesc.Width = width;
        screenTexDesc.Height = height;
        screenTexDesc.MipLevels = 1;
        screenTexDesc.ArraySize = 1;
        screenTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        screenTexDesc.SampleDesc.Count = 1;
        screenTexDesc.SampleDesc.Quality = 0;
        screenTexDesc.Usage = D3D11_USAGE_DEFAULT;
        screenTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        ASSERT_HR(_dx11_device2->CreateTexture2D(&screenTexDesc, NULL, &_dx11_screen_texture));

        // 새로운 Screen 텍스처에 대한 SRV 재생성
        ASSERT_HR(_dx11_device2->CreateShaderResourceView(_dx11_screen_texture.Get(), NULL, &_dx11_srv));

        // 리사이즈된 스왑체인의 백버퍼에 대한 RTV 재생성
        ComPtr<ID3D11Texture2D> backBuffer;
        ASSERT_HR(_dx11_swapchain1->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
        ASSERT_HR(_dx11_device2->CreateRenderTargetView(backBuffer.Get(), NULL, &_dx11_rtv));

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
            ipc_proto::modkey_button_type mods{};
            if (GetKeyState(VK_SHIFT) & 0x8000) { mods |= ipc_proto::MODKEY_SHIFT; }
            if (GetKeyState(VK_CONTROL) & 0x8000) { mods |= ipc_proto::MODKEY_CTRL; }
            if (GetKeyState(VK_MENU) & 0x8000) { mods |= ipc_proto::MODKEY_ALT; }
            if (GetKeyState(VK_CAPITAL) & 0x0001) { mods |= ipc_proto::MODKEY_CAPSLOCK; }
            if (GetKeyState(VK_NUMLOCK) & 0x0001) { mods |= ipc_proto::MODKEY_NUMLOCK; }

            CXLIB_TRACE("key event: key: {}, scancode: {}, action: {}, mods: {}"
                , static_cast<int>(key)
                , scan_code
                , static_cast<int>(action)
                , static_cast<int>(mods)
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
                return ipc_proto::MOUSEBTN_L;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                return ipc_proto::MOUSEBTN_R;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return ipc_proto::MOUSEBTN_M;
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

        ipc_proto::modkey_button_type mods{};
        if (wParam & MK_CONTROL) { mods |= ipc_proto::MODKEY_CTRL; }
        if (wParam & MK_SHIFT) { mods |= ipc_proto::MODKEY_SHIFT; }

        //CXLIB_TRACE("mouse btn event: btn: {}, action: {}, mods: {}"
        //    , static_cast<int>(btn)
        //    , static_cast<int>(action)
        //    , static_cast<int>(mods)
        //);

        packet_buffer<ipc_proto::packets::mouse_button_event_t> pck{ ipc_proto::packet_type::mouse_button_event };
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

        //CXLIB_TRACE("mouse move event: ({}, {})", x, y);

        packet_buffer<ipc_proto::packets::mouse_move_event_t> pck{ ipc_proto::packet_type::mouse_move_event };
        pck.body()->x = x;
        pck.body()->y = y;
        pck.body()->mods.ctrl_pressed = wParam & MK_CONTROL;
        pck.body()->mods.shift_pressed = wParam & MK_SHIFT;
        pck.body()->mods.l_btn_pressed = wParam & MK_LBUTTON;
        pck.body()->mods.r_btn_pressed = wParam & MK_RBUTTON;
        pck.body()->mods.m_btn_pressed = wParam & MK_MBUTTON;
        CXLIB_ASSERT(std::errc{} == _ipc_cli->send_notify(pck.data(), pck.size()));

        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        const int32_t raw_delta = GET_WHEEL_DELTA_WPARAM(wParam);

        //CXLIB_TRACE("mouse scroll event: {}", raw_delta);

        // GLFW와 동일한 스크롤 값 계산
        // GLFW는 스크롤 한 단위를 1.0 또는 -1.0으로 정규화하므로,
        // raw_delta 값을 WHEEL_DELTA로 나누어준다. (Normalization)
        const float yoffset = static_cast<float>(raw_delta) / static_cast<float>(WHEEL_DELTA);

        packet_buffer<ipc_proto::packets::mouse_scroll_event_t> pck{ ipc_proto::packet_type::mouse_scroll_event };
        pck.body()->yoffset = yoffset;
        CXLIB_ASSERT(std::errc{} == _ipc_cli->send_notify(pck.data(), pck.size()));

        return 0;
    }
    } // switch

    return ::DefWindowProcA(hWnd, msg, wParam, lParam);
}

void viewer_process::_render_frame()
{
    // 렌더링 전, 획득해둔 KeyedMutex를 사용하여 GL 렌더러가 텍스처 쓰기를 완료할 때까지 대기
    // (뮤텍스를 즉시 얻지 못한 경우, GL 렌더러 측에서 아직 작업 중이거나 렌더링된 프레임이 없음을 의미)
    // 
    // https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgikeyedmutex-acquiresync
    // AcquireSync은 다음과 같은 DWORD 상수들을 반환할 수 있음.
    // (단순 성공 여부 판단을 위해 SUCCEEDED 매크로만 사용할 경우, WAIT_OBJECT_0 반환값을 제외한 나머지 반환 상태값을 제대로 감지하지 못할 수 있음에 유의)
    //     - WAIT_OBJECT_0  : keyed mutex를 성공적으로 획득했음. 이 경우, 렌더링 작업을 계속 진행할 수 있음. (S_OK 와 동일한 값)
    //     - WAIT_TIMEOUT   : 지정된 키가 해제되기 전에 타임아웃 간격이 경과했음을 의미.
    //     - WAIT_ABANDONED : SharedSurface와 KeyedMutex가 더 이상 일관된 상태가 아님. 이 경우, KeyedMutex와 SharedSurface 둘 다 해제한 후 재생성해야 함.
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

    _dx11_device_context2->Draw(3, 0); // 화면을 덮는 하나의 삼각형을 그림

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
