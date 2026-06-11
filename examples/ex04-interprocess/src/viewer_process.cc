#include "viewer_process.hh"

#include <windowsx.h>
#include <conio.h>
#include <iostream>
#include <xutl/debug/logger.hh>

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
            XUTL_ERROR("Failed to duplicate shared texture handle. (error: {})", ::GetLastError());
            return nullptr;
        }

        utils::unique_handle duplicated_handle_guard{ duplicated_handle }; // 핸들의 자동 해제를 위한 RAII 핸들 래퍼
        if (HRESULT hr = dx11_device->OpenSharedResource1(
            duplicated_handle_guard.get(),
            IID_PPV_ARGS(&dx11_shared_texture));
            FAILED(hr))
        {
            XUTL_ERROR("Failed to open shared texture resource. (HRESULT: {:08X})", static_cast<uint32_t>(hr));
            return nullptr;
        }

        return dx11_shared_texture;
    }

} // namespace

viewer_process::viewer_process(const SIZE initial_frame_size, const DXGI_FORMAT target_frame_format)
{
    XUTL_INFO("DX viewer process spawned (PID: {})", ::GetCurrentProcessId());
    this->_initialize(initial_frame_size, target_frame_format);
}

viewer_process::~viewer_process()
{
    XUTL_DEBUG("Cleaning up viewer process resources...");
    
    // Clean up D3D11 device context state
    if (_dx11_device_context2) {
        _dx11_device_context2->ClearState();
        _dx11_device_context2->Flush();
    }

    // Clean up D3D11 resources (COM objects will auto-release)
    _dx11_rtv.Reset();
    _dx11_srv.Reset();
    _dx11_pixel_shader.Reset();
    _dx11_vertex_shader.Reset();
    _dx11_sampler_state.Reset();
    _dx11_shared_texture_copy.Reset();
    _dxgi_keyed_mutex.Reset();
    _dx11_shared_texture.Reset();
    _dx11_swapchain1.Reset();
    _dx11_device_context2.Reset();
    _dx11_device2.Reset();
    _dxgi_adapter.Reset();

    // Clean up window handle
    _viewer_hwnd.reset();

    // Clean up IPC client
    if (_ipc_cli) {
        _ipc_cli->disconnect();
        _ipc_cli.reset();
    }

    // Close renderer process handle
    _renderer_process_handle.reset();

    XUTL_DEBUG("Viewer process cleanup complete.");
}

void viewer_process::run()
{
    XUTL_INFO("Entering render loop...");

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (::PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
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
    const SIZE initial_frame_size,
    const DXGI_FORMAT target_frame_format)
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    XUTL_DEBUG("Connecting to IPC server...");

    if (std::shared_ptr<void> hEvent{
            ::OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, Config::RENDERER_PROCESS_INST_NAME),
            ::CloseHandle
        }; !hEvent)
    {
        XUTL_DEBUG("No existing renderer process found. Launching new renderer...");

        std::array<char, MAX_PATH + 20> currExePathBuff{};
        ::GetModuleFileNameA(nullptr, currExePathBuff.data(), static_cast<DWORD>(currExePathBuff.size()));
        std::string cmdLine = fmt::format("\"{}\" --renderer-mode", currExePathBuff.data());

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        XUTL_ASSERT(::CreateProcessA(
            nullptr,
            cmdLine.data(),
            nullptr,
            nullptr,
            FALSE, // Handle inheritance
            CREATE_NEW_CONSOLE,
            nullptr,
            nullptr,
            &si, &pi
        ));

        _renderer_process_handle.reset(pi.hProcess);
        ::CloseHandle(pi.hThread); // Thread handle not needed

        // Wait for the renderer process to be ready
        std::this_thread::sleep_for(1000ms);
        std::cout << "\nPress any key to connect..." << std::endl;
        static_cast<void>(::_getch());

        XUTL_DEBUG("Launched renderer process.");
    }
    else
    {
        XUTL_DEBUG("Found existing renderer process!");
    }

    _ipc_cli = std::make_shared<ipc_client>();
    _ipc_cli->set_disconnect_callback(
        [this]()
    {
        XUTL_WARN("Renderer process disconnected! closing viewer window...");
        ::PostMessageA(_viewer_hwnd.get(), WM_CLOSE, 0, 0);
    });

    if (!_ipc_cli->connect(Config::RENDERER_SERVER_NAME)) {
        throw std::runtime_error{ "connect failed" };
    }

    XUTL_DEBUG("Connected! sending init request...");

    // 초기화 요청 전송

    packet_builder<ipc_proto::packets::init_request_t> init_req{ ipc_proto::packet_type::init_request };
    init_req.body()->frame_width = static_cast<int32_t>(initial_frame_size.cx);
    init_req.body()->frame_height = static_cast<int32_t>(initial_frame_size.cy);

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
    XUTL_DEBUG("Received init response. (pid: {}, adapter: {:x}-{:x}, surface handle: {})"
        , init_rep->renderer_process_id
        , init_rep->target_adapter_luid.HighPart
        , init_rep->target_adapter_luid.LowPart
        , init_rep->surface_handle
    );

    if (!_renderer_process_handle) {
        XUTL_DEBUG("Opening renderer process handle...");
        _renderer_process_handle.reset(::OpenProcess(
            PROCESS_DUP_HANDLE | SYNCHRONIZE,
            FALSE,
            init_rep->renderer_process_id
        ));
        XUTL_ASSERT(_renderer_process_handle.get());
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    XUTL_DEBUG("Creating d3d window...");

    static const auto pfnNativeWndProc = +[](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT
    {
        viewer_process* pThis{ nullptr };
        if (message == WM_NCCREATE) {
            CREATESTRUCT* pCreate = _XUTL utility::bit_cast<CREATESTRUCT*>(lParam);
            pThis = _XUTL utility::bit_cast<viewer_process*>(pCreate->lpCreateParams);
            ::SetWindowLongPtrA(hWnd, GWLP_USERDATA, _XUTL utility::bit_cast<LONG_PTR>(pThis));
        } else {
            pThis = _XUTL utility::bit_cast<viewer_process*>(::GetWindowLongPtrA(hWnd, GWLP_USERDATA));
        }

        return (pThis && pThis->_fl_initialized)
            ? pThis->_wnd_proc(hWnd, message, wParam, lParam)
            : ::DefWindowProcA(hWnd, message, wParam, lParam);
    };

    static_assert(std::is_same_v<std::remove_const_t<decltype(pfnNativeWndProc)>, WNDPROC>, "WndProc type mismatch!");

    WNDCLASSA wc = { 0, };
    wc.lpfnWndProc = pfnNativeWndProc;
    wc.lpszClassName = "MyDX11ViewerWndClass";
    wc.hCursor = ::LoadCursorA(nullptr, IDC_ARROW);
    ::RegisterClassA(&wc);

    RECT windowRect = { 0, 0, initial_frame_size.cx, initial_frame_size.cy };
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
        nullptr,
        nullptr,
        nullptr,
        this // Pass 'this' to WndProc
    ));
    XUTL_ASSERT(_viewer_hwnd.get());

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    XUTL_DEBUG("Initializing D3D resources...");

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
            XUTL_DEBUG(L"Found matching adapter: {:x}-{:x} ({})", desc.AdapterLuid.HighPart, desc.AdapterLuid.LowPart, desc.Description);
            break;
        }
    }

    if (!_dxgi_adapter) {
        throw std::runtime_error{ "Matching adapter not found" };
    }

    // Create D3D11 device and device context
    {
        UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
        deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        ComPtr<ID3D11Device> dx11Device0;
        ComPtr<ID3D11DeviceContext> dx11DeviceContext0;

        ASSERT_HR(::D3D11CreateDevice(
            _dxgi_adapter.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            deviceFlags,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &dx11Device0,
            nullptr,
            &dx11DeviceContext0
        ));

        // Convert `ID3D11Device` -> `ID3D11Device2` (Higher version object)
        ASSERT_HR(dx11Device0.As(&_dx11_device2));

        // Convert `ID3D11DeviceContext` -> `ID3D11DeviceContext2` (Higher version object)
        ASSERT_HR(dx11DeviceContext0.As(&_dx11_device_context2));
    }

    // Open shared interop texture from native handle
    {
        _dx11_shared_texture = open_shared_texture_from_native_handle(
            _dx11_device2,
            init_rep->surface_handle,
            _renderer_process_handle.get()
        );
        if (_dx11_shared_texture) {
            XUTL_DEBUG("Successfully opened surface handle: {})", init_rep->surface_handle);
        } else {
            throw std::runtime_error{ fmt::format(
                "Failed to open surface handle: {}"
                , init_rep->surface_handle
            )};
        }

        // Get the KeyedMutex interface from the shared texture
        ASSERT_HR(_dx11_shared_texture.As(&_dxgi_keyed_mutex));
    }

    // Create copy of the shared texture (non-shared)
    // (This texture will be sampled to present to the swap chain)
    {
        D3D11_TEXTURE2D_DESC sharedTexCopyDesc{};
        _dx11_shared_texture->GetDesc(&sharedTexCopyDesc); // frame size & format will be same as shared texture
        sharedTexCopyDesc.MipLevels = 1;
        sharedTexCopyDesc.ArraySize = 1;
        sharedTexCopyDesc.SampleDesc.Count = 1;
        sharedTexCopyDesc.SampleDesc.Quality = 0;
        sharedTexCopyDesc.Usage = D3D11_USAGE_DEFAULT;
        sharedTexCopyDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        sharedTexCopyDesc.CPUAccessFlags = 0;
        sharedTexCopyDesc.MiscFlags = 0;
        ASSERT_HR(_dx11_device2->CreateTexture2D(
            &sharedTexCopyDesc,
            nullptr,
            &_dx11_shared_texture_copy
        ));
    }

    // Create Sampler State
    {
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

    // Create the swap chain for the viewer window (Use CreateSwapChainForHwnd instead)
    {
        DXGI_SWAP_CHAIN_DESC1 swapchainDesc1{};
        swapchainDesc1.Width = swapchainDesc1.Height = 0; // NOTE: 0 means automatically set to the window's size
        swapchainDesc1.Format = target_frame_format;
        swapchainDesc1.Stereo = FALSE; // Disable stereo (3D) mode
        swapchainDesc1.SampleDesc.Count = 1; // No multi-sampling (MSAA off)
        swapchainDesc1.SampleDesc.Quality = 0; // Default quality
        swapchainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchainDesc1.BufferCount = 2; // Double buffering
        swapchainDesc1.Scaling = DXGI_SCALING_STRETCH;
        swapchainDesc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapchainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Use Modern flip model `DXGI_SWAP_EFFECT_FLIP_DISCARD` -> faster than blt
        swapchainDesc1.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING; // Allow tearing for VSync off
        ASSERT_HR(dxgiFactory2->CreateSwapChainForHwnd(
            _dx11_device2.Get(),
            _viewer_hwnd.get(),
            &swapchainDesc1,
            nullptr, // Do not use fullscreen
            nullptr, // No restrictions to output
            &_dx11_swapchain1
        ));
    }

    // Base full-screen quad rendering shader template with preprocessor conditionals
    static const std::string shaderTemplate = R"hlsl(
        Texture2D g_texture : register(t0);
        SamplerState g_sampler : register(s0);

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
            float2 uv = input.uv;
            
        #ifdef FLIP_Y_AXIS
            // Flip Y-axis (if needed)
            uv.y = 1.0 - uv.y;
        #endif
            
            float4 color = g_texture.Sample(g_sampler, uv);
            
        #ifdef CONVERT_RGBA_TO_BGRA
            color = float4(color.b, color.g, color.r, color.a); // Swap R and B channels
        #endif
            
            return color;
        }
    )hlsl";

    // Helper to compile shader with specific defines
    constexpr auto compileShader =
        [](const std::string& shader_template,
            const std::string& entry_point,
            const std::string& target,
            const D3D_SHADER_MACRO* defines = nullptr) -> ComPtr<ID3DBlob>
        {
            ComPtr<ID3DBlob> psBlob, errBlob;
            HRESULT hr = ::D3DCompile(
                shader_template.c_str(), shader_template.size(),
                nullptr, defines, nullptr,
                entry_point.c_str(), target.c_str(),
                0, 0,
                &psBlob, &errBlob
            );
            if (FAILED(hr)) {
                throw std::runtime_error{ fmt::format(
                    "Failed to compile {} shader({:08X}): {}"
                    , target.c_str()
                    , hr
                    , static_cast<const char*>(errBlob->GetBufferPointer())
                ) };
            }
            return psBlob;
        };

    // Compile vertex shader
    {
        auto vsBlob = compileShader(shaderTemplate, "VS", "vs_5_0");
        ASSERT_HR(_dx11_device2->CreateVertexShader(
            vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(),
            nullptr,
            &_dx11_vertex_shader
        ));
    }

    // Compile pixel shader
    {
        std::vector<D3D_SHADER_MACRO> defines;

        // Need Y-flip because OpenGL uses bottom-left origin while DirectX uses top-left
        defines.push_back(D3D_SHADER_MACRO{ "FLIP_Y_AXIS", "1" });

        // NOTE: No manual color-conversion needed when rendering RGBA texture (OpenGL) to BGRA render target (Flutter),
        // GPU handles the conversion automatically.
        // when you load the texture, it gets 'swizzled' if needed to the standard Red, Green, and Blue channels
        // and when you write to the render target the same thing happens depending on the format.
        // so manual pixel shader conversion would cause double-swapping and corrupt colors.
        // Ref: https://stackoverflow.com/a/46369577/3865427
        // if (target_frame_format == DXGI_FORMAT_B8G8R8A8_UNORM) {
        //     defines.push_back(D3D_SHADER_MACRO{ "CONVERT_RGBA_TO_BGRA", "1" }); 
        // }

        // Add a null terminator to the defines array
        if (!defines.empty()) {
            defines.push_back(D3D_SHADER_MACRO{ nullptr, nullptr });
        }

        auto psBlob = compileShader(shaderTemplate, "PS", "ps_5_0", defines.data());
        ASSERT_HR(_dx11_device2->CreatePixelShader(
            psBlob->GetBufferPointer(),
            psBlob->GetBufferSize(),
            nullptr,
            &_dx11_pixel_shader
        ));
    }

    // Get the back buffer of the swap chain
    ComPtr<ID3D11Texture2D> backBuffer;
    ASSERT_HR(_dx11_swapchain1->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));

    // Create Shader Resource View (SRV)
    // Equivalent of: `glBindTexture`+ `sampler2D`
    ASSERT_HR(_dx11_device2->CreateShaderResourceView(
        _dx11_shared_texture_copy.Get(),
        nullptr,
        &_dx11_srv
    ));

    // Create Render Target View (RTV)
    ASSERT_HR(_dx11_device2->CreateRenderTargetView(
        backBuffer.Get(),
        nullptr,
        &_dx11_rtv
    ));

    // Setup viewport
    {
        _viewport.TopLeftX = 0.0f;
        _viewport.TopLeftY = 0.0f;
        _viewport.Width = static_cast<float>(initial_frame_size.cx);
        _viewport.Height = static_cast<float>(initial_frame_size.cy);
        _viewport.MinDepth = 0.0f;
        _viewport.MaxDepth = 1.0f;
    }

    _fl_initialized = true;
    XUTL_INFO("Initialization complete.");
}

void viewer_process::_resize_frame(const SIZE new_frame_size)
{
    XUTL_TRACE("frame resize request: {}x{}", new_frame_size.cx, new_frame_size.cy);

    packet_builder<ipc_proto::packets::frame_resize_request_t> req{ ipc_proto::packet_type::frame_resize_request };
    req.body()->width = static_cast<int32_t>(new_frame_size.cx);
    req.body()->height = static_cast<int32_t>(new_frame_size.cy);

    std::vector<uint8_t> rep_bytes;
    if (std::errc{} != _ipc_cli->send_request_sync(
        req.data(),
        req.size(),
        rep_bytes))
    {
        throw std::runtime_error("frame resize request failed.");
    }

    packet_view pck_view{ rep_bytes.data(), rep_bytes.size() };
    const auto resize_rep = pck_view.body<ipc_proto::packets::frame_resize_response_t>();
    XUTL_TRACE("frame resize response -> surface handle: {}"
        , resize_rep->surface_handle
    );

    D3D11_TEXTURE2D_DESC sharedTexCopyDesc{};
    _dx11_shared_texture_copy->GetDesc(&sharedTexCopyDesc);

    // First, need to clear the render target and shader resource views
    // before resizing the swap chain and shared texture.
    _dx11_device_context2->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    _dx11_device_context2->PSSetShaderResources(0, 1, &nullSRV);
    _dx11_device_context2->Flush(); // Wait for GPU to finish processing

    //
    // 렌더링 리소스 해제 (해제 순서에 유의; 뷰 -> 텍스처 순서)
    //

    _dx11_rtv.Reset();
    _dx11_srv.Reset();
    _dxgi_keyed_mutex.Reset();
    _dx11_shared_texture.Reset();
    _dx11_shared_texture_copy.Reset();

    //
    // 렌더링 리소스 리사이즈(재생성) 시작
    // (모든 버퍼 참조(RTV 등)가 해제된 후 수행되어야 함)
    //

    // 스왑체인 리사이즈
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc1{};
    _dx11_swapchain1->GetDesc1(&swapchainDesc1);
    HRESULT hr = _dx11_swapchain1->ResizeBuffers(
        swapchainDesc1.BufferCount,
        new_frame_size.cx,
        new_frame_size.cy,
        swapchainDesc1.Format,
        swapchainDesc1.Flags
    );
    ASSERT_HR(hr);

    // 공유 텍스처 재생성
    _dx11_shared_texture = open_shared_texture_from_native_handle(
        _dx11_device2,
        resize_rep->surface_handle, // IPC로 받은 새로운 핸들
        _renderer_process_handle.get()
    );
    if (_dx11_shared_texture) {
        XUTL_DEBUG("Successfully opened surface handle: {}"
            , resize_rep->surface_handle
        );
    } else {
        throw std::runtime_error{ "Failed to open surface handle" };
    }

    // KeyedMutex 재생성
    ASSERT_HR(_dx11_shared_texture.As(&_dxgi_keyed_mutex));

    // 로컬 Screen 텍스처 재생성
    sharedTexCopyDesc.Width = static_cast<UINT>(new_frame_size.cx);
    sharedTexCopyDesc.Height = static_cast<UINT>(new_frame_size.cy);
    ASSERT_HR(_dx11_device2->CreateTexture2D(
        &sharedTexCopyDesc,
        nullptr,
        &_dx11_shared_texture_copy
    ));

    // 새로운 Screen 텍스처에 대한 SRV 재생성
    ASSERT_HR(_dx11_device2->CreateShaderResourceView(
        _dx11_shared_texture_copy.Get(),
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

    // Update viewport to new frame size
    _viewport.Width = static_cast<float>(new_frame_size.cx);
    _viewport.Height = static_cast<float>(new_frame_size.cy);

    XUTL_DEBUG("All DX resources have been successfully synchronized.");
}

void viewer_process::_render_frame()
{
    if (_fl_render_interop_texture)
    {
        // 렌더링 전, 획득해둔 KeyedMutex를 사용하여 GL 렌더러가 텍스처 쓰기를 완료할 때까지 대기
        // (뮤텍스를 즉시 얻지 못한 경우, GL 렌더러 측에서 아직 작업 중이거나 렌더링된 프레임이 없음을 의미)
        // 
        // AcquireSync은 다음과 같은 DWORD 상수들을 반환할 수 있음.
        // (단순 성공 여부 판단을 위해 SUCCEEDED 매크로만 사용할 경우, WAIT_OBJECT_0 반환값을 제외한 나머지 반환 상태값을 제대로 감지하지 못할 수 있음에 유의)
        //     - WAIT_OBJECT_0  : keyed mutex를 성공적으로 획득했음. 이 경우, 렌더링 작업을 계속 진행할 수 있음. (S_OK 와 동일한 값)
        //     - WAIT_TIMEOUT   : 지정된 키가 해제되기 전에 타임아웃 간격이 경과했음을 의미.
        //     - WAIT_ABANDONED : SharedSurface와 KeyedMutex가 더 이상 일관된 상태가 아님. 이 경우, KeyedMutex와 SharedSurface 둘 다 해제한 후 재생성해야 함.
        // https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgikeyedmutex-acquiresync

        constexpr uint64_t kMutexKey = 0;
        constexpr uint32_t kMaxWaitTimeoutInMs = 1; // Use a small timeout to prevent blocking the viewer while allowing smooth frame updates
        switch (
            const HRESULT sync_hr = _dxgi_keyed_mutex->AcquireSync(kMutexKey, kMaxWaitTimeoutInMs);
        sync_hr
            ) {
        case WAIT_OBJECT_0: // KeyedMutex를 성공적으로 획득했으므로 렌더링 작업 진행 가능
            // Shared 텍스처를 복사본 텍스처로 복사 (락 점유 시간을 최소화하기 위해 별도 텍스처로 데이터를 복사한 뒤 렌더링 수행)
            _dx11_device_context2->CopyResource(_dx11_shared_texture_copy.Get(), _dx11_shared_texture.Get());
            _dxgi_keyed_mutex->ReleaseSync(kMutexKey); // 텍스처 사용이 끝났으므로 KeyedMutex 잠금 해제
            break;
        case WAIT_TIMEOUT: // KeyedMutex를 획득하지 못했으므로 렌더링 작업을 건너뜀
            break; // Continue rendering with the previous frame to maintain smooth presentation
        case WAIT_ABANDONED: // KeyedMutex가 더 이상 일관된 상태가 아님. 이 경우, KeyedMutex와 SharedSurface 둘 다 해제한 후 재생성해야 함.
            XUTL_ERROR("Keyed mutex abandoned - renderer process may have crashed");
            return;
        default:
            XUTL_ERROR("Unexpected AcquireSync result: 0x{:X}", sync_hr);
            return;
        }

        _dx11_device_context2->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        _dx11_device_context2->IASetInputLayout(nullptr); // No input buffer needed for this VS trick

        _dx11_device_context2->VSSetShader(_dx11_vertex_shader.Get(), nullptr, 0);
        _dx11_device_context2->PSSetShader(_dx11_pixel_shader.Get(), nullptr, 0);
        _dx11_device_context2->PSSetShaderResources(0, 1, _dx11_srv.GetAddressOf());
        _dx11_device_context2->PSSetSamplers(0, 1, _dx11_sampler_state.GetAddressOf());

        _dx11_device_context2->RSSetViewports(1, &_viewport);
        _dx11_device_context2->OMSetRenderTargets(1, _dx11_rtv.GetAddressOf(), nullptr);

        _dx11_device_context2->Draw(3, 0); // 화면을 덮는 하나의 삼각형을 그림
    }
    else
    {
        const std::array<float, 4> clearColor{ 0.5f, 0.5f, 0.5f, 1.0f };
        _dx11_device_context2->ClearRenderTargetView(_dx11_rtv.Get(), clearColor.data());
    }

    _dx11_swapchain1->Present(0, DXGI_PRESENT_ALLOW_TEARING); // Present(0, DXGI_PRESENT_ALLOW_TEARING) : VSync Off

    // log fps
    //static int frameCount = 0;
    //++frameCount;
    //static auto lastTime = std::chrono::steady_clock::now();
    //const auto currentTime = std::chrono::steady_clock::now();
    //if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime) >= 1s)
    //{
    //    XUTL_DEBUG("Render FPS: {}", frameCount);
    //    lastTime = currentTime;
    //    frameCount = 0;
    //}
}

LRESULT viewer_process::_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CLOSE:
        XUTL_DEBUG("Closing viewer window...");
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1; // Prevent flickering
    case WM_SIZE:
    {
        const SIZE new_frame_size{
            static_cast<LONG>(LOWORD(lParam)),
            static_cast<LONG>(HIWORD(lParam))
        };

        if (new_frame_size.cx > 0 && new_frame_size.cy > 0) {
            this->_resize_frame(new_frame_size);
        }

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

            XUTL_TRACE("key event -> key: {}, scancode: {}, action: {}, mods: 0x{:X}"
                , static_cast<int>(key)
                , scan_code
                , static_cast<int>(action)
                , static_cast<std::underlying_type_t<ipc_proto::modifier_button_type>>(mods)
            );

            if (action == ipc_proto::ACTION_RELEASE)
            {
                switch (key) {
                case ipc_proto::KEY_F1:
                    _fl_render_interop_texture = !_fl_render_interop_texture;
                    XUTL_DEBUG("Toggled interop texture rendering: {}", _fl_render_interop_texture ? "enabled" : "disabled");
                    break;
                default:
                    break;
                }
            }
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

        XUTL_TRACE("mouse event -> btn: {}, action: {}, mods: 0x{:X}"
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
        XUTL_ASSERT(std::errc{} == _ipc_cli->send_notify(pck.data(), pck.size()));

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

        XUTL_TRACE("mouse move event -> pos: ({}, {}), mods: 0x{:X}"
            , x, y
            , static_cast<std::underlying_type_t<ipc_proto::modifier_button_type>>(mods)
        );

        packet_builder<ipc_proto::packets::mouse_move_event_t> pck{ ipc_proto::packet_type::mouse_move_event };
        pck.body()->x = x;
        pck.body()->y = y;
        pck.body()->mods = mods;
        XUTL_ASSERT(std::errc{} == _ipc_cli->send_notify(pck.data(), pck.size()));

        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        const int32_t raw_scroll_delta = GET_WHEEL_DELTA_WPARAM(wParam);

        // GLFW와 동일한 스크롤 값 계산
        // GLFW는 스크롤 한 단위를 1.0 또는 -1.0으로 정규화하므로,
        // raw_delta 값을 WHEEL_DELTA로 나누어준다. (Normalization)
        const float yoffset = static_cast<float>(raw_scroll_delta) / static_cast<float>(WHEEL_DELTA);

        XUTL_TRACE("mouse scroll event -> yoffset: {}", yoffset);

        packet_builder<ipc_proto::packets::mouse_scroll_event_t> pck{ ipc_proto::packet_type::mouse_scroll_event };
        pck.body()->yoffset = yoffset;
        XUTL_ASSERT(std::errc{} == _ipc_cli->send_notify(pck.data(), pck.size()));

        return 0;
    }
    } // switch

    return ::DefWindowProcA(hWnd, msg, wParam, lParam);
}