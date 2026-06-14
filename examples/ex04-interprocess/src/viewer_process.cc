#include "viewer_process.hh"

#include <windowsx.h>
#include <conio.h>
#include <iostream>
#include <optional>
#include <xutl/debug/logger.hh>
#include <xutl/utility/bit.hh>

namespace surface_proto = triengine_interop::surface::proto;

viewer_process::viewer_process(
    const SIZE initial_frame_size, 
    const DXGI_FORMAT target_frame_format, 
    const bool enable_vsync)
{
    XUTL_INFO("DX viewer process spawned (PID: {})", ::GetCurrentProcessId());
    this->_initialize(
        initial_frame_size, 
        target_frame_format, 
        enable_vsync
    );
}

viewer_process::~viewer_process()
{
    XUTL_DEBUG("Cleaning up viewer process resources...");

    // Release the viewer-owned present target before the consumer's device goes away.
    if (_consumer.get_dx11_context()) {
        _consumer.get_dx11_context()->ClearState();
        _consumer.get_dx11_context()->Flush();
    }

    // Cleanup D3D resources
    _dx11_rtv.Reset();
    _dx11_swapchain1.Reset();
    _viewer_hwnd.reset();

    // Destroy the surface interop and disconnect from the renderer.
    _consumer.disconnect();

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
    const DXGI_FORMAT target_frame_format,
    const bool enable_vsync)
{
    _fl_vsync_enabled = enable_vsync;
    XUTL_DEBUG("VSync: {}", enable_vsync ? "enabled" : "disabled");

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

    _consumer.set_disconnect_callback(
        [this]()
    {
        XUTL_WARN("Renderer process disconnected! closing viewer window...");
        ::PostMessageA(_viewer_hwnd.get(), WM_CLOSE, 0, 0);
    });

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    XUTL_DEBUG("Connecting and creating surface consumer...");

    // The consumer connects, performs the init handshake, picks the renderer's adapter,
    // opens the shared surface and builds the blit pipeline. No manual color conversion
    // is needed: the GPU swizzles RGBA<->BGRA on load/store, so only a Y-flip is
    // requested here (the default `surface_render_options`).
    if (!_consumer.connect(
        Config::RENDERER_SERVER_NAME,
        initial_frame_size))
    {
        throw std::runtime_error{ "failed to connect / create surface consumer" };
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
    XUTL_DEBUG("Creating swap chain on the consumer's device...");

    // The viewer owns the present target. It is created on the consumer's device so the
    // consumer can blit its copy texture directly onto the back buffer.

    // NOTE: 반드시 CreateDXGIFactory2 함수를 사용해서 DXGI 1.2 버전 이상의 DXGI 팩토리(`IDXGIFactory`)를 생성해줘야 함.
    // (`ID3D11Device::CreateTexture2D: D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX is only available for devices created off of Dxgi1.1 factories or later.` D3D11 오류 방지)
    ComPtr<IDXGIFactory2> dxgiFactory2;
    ASSERT_HR(::CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory2)));

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
        // Tearing is only permitted when VSync is off; the flag must match the sync
        // interval used in Present() (see _render_frame).
        swapchainDesc1.Flags = enable_vsync ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        ASSERT_HR(dxgiFactory2->CreateSwapChainForHwnd(
            _consumer.get_dx11_device(),
            _viewer_hwnd.get(),
            &swapchainDesc1,
            nullptr, // Do not use fullscreen
            nullptr, // No restrictions to output
            &_dx11_swapchain1
        ));
    }

    // Get the back buffer of the swap chain
    ComPtr<ID3D11Texture2D> backBuffer;
    ASSERT_HR(_dx11_swapchain1->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));

    // Create Render Target View (RTV)
    ASSERT_HR(_consumer.get_dx11_device()->CreateRenderTargetView(
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

    // Detach and release the back-buffer RTV before resizing the swap chain.
    _consumer.get_dx11_context()->OMSetRenderTargets(0, nullptr, nullptr);
    _dx11_rtv.Reset();
    _consumer.get_dx11_context()->Flush();

    // Resize the swap chain (viewer-owned present target)
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc1{};
    _dx11_swapchain1->GetDesc1(&swapchainDesc1);
    ASSERT_HR(_dx11_swapchain1->ResizeBuffers(
        swapchainDesc1.BufferCount,
        new_frame_size.cx,
        new_frame_size.cy,
        swapchainDesc1.Format,
        swapchainDesc1.Flags
    ));

    // Resize the shared-surface side (the consumer requests the renderer resize and
    // recreates the shared texture / copy / SRV).
    if (!_consumer.resize_frame(new_frame_size))
    {
        throw std::runtime_error("frame resize request failed.");
    }

    // 리사이즈된 스왑체인의 백버퍼에 대한 RTV 재생성
    ComPtr<ID3D11Texture2D> backBuffer;
    ASSERT_HR(_dx11_swapchain1->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
    ASSERT_HR(_consumer.get_dx11_device()->CreateRenderTargetView(
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
        // Sync the latest renderer frame into the consumer's copy, then blit it
        // onto our back buffer. A false return means the shared surface became
        // inconsistent (renderer likely crashed); skip the frame.
        if (!_consumer.sync_latest_frame(1)) {
            return;
        }
        _consumer.blit_to_render_target(_dx11_rtv.Get(), _viewport);
    }
    else
    {
        const std::array<float, 4> clearColor{ 0.5f, 0.5f, 0.5f, 1.0f };
        _consumer.get_dx11_context()->ClearRenderTargetView(_dx11_rtv.Get(), clearColor.data());
    }

    // VSync on: sync interval 1, no tearing. VSync off: sync interval 0 with tearing
    // allowed (requires the swap chain's ALLOW_TEARING flag, set in _initialize).
    if (_fl_vsync_enabled) {
        _dx11_swapchain1->Present(1, 0);
    } else {
        _dx11_swapchain1->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }

    // log average of frame time and fps every 1 second
    thread_local int frameCount = 0;
    ++frameCount;
    thread_local auto lastTime = std::chrono::steady_clock::now();
    const auto currentTime = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime) >= 1s) {
        XUTL_DEBUG("Viewer FPS: {}", frameCount);
        lastTime = currentTime;
        frameCount = 0;
    }
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

        auto key = surface_proto::translate_vkcode(vkcode);
        if (key != surface_proto::KEY_UNKNOWN)
        {
            auto action = is_key_released ? surface_proto::ACTION_RELEASE : (was_key_down && repeat_count ? surface_proto::ACTION_REPEAT : surface_proto::ACTION_PRESS);
            surface_proto::modifier_button_type mods{};
            if (GetKeyState(VK_SHIFT) & 0x8000) { mods |= surface_proto::MOD_KEY_SHIFT; }
            if (GetKeyState(VK_CONTROL) & 0x8000) { mods |= surface_proto::MOD_KEY_CTRL; }
            if (GetKeyState(VK_MENU) & 0x8000) { mods |= surface_proto::MOD_KEY_ALT; }
            if (GetKeyState(VK_CAPITAL) & 0x0001) { mods |= surface_proto::MOD_KEY_CAPSLOCK; }
            if (GetKeyState(VK_NUMLOCK) & 0x0001) { mods |= surface_proto::MOD_KEY_NUMLOCK; }

            //XUTL_TRACE("key event -> key: {}, scancode: {}, action: {}, mods: 0x{:X}"
            //    , static_cast<int>(key)
            //    , scan_code
            //    , static_cast<int>(action)
            //    , static_cast<std::underlying_type_t<surface_proto::modifier_button_type>>(mods)
            //);

            // Forward the key event to the renderer.
            XUTL_ASSERT(std::errc{} == _consumer.send_key_event(key, action, mods));

            if (action == surface_proto::ACTION_RELEASE)
            {
                switch (key) {
                case surface_proto::KEY_F1:
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

        surface_proto::mouse_button_type btn = [msg]() -> std::optional<surface_proto::mouse_button_type> {
            switch (msg) {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                return surface_proto::MOUSE_L;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                return surface_proto::MOUSE_R;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return surface_proto::MOUSE_M;
            default:
                return std::nullopt;
            }
        }().value();

        surface_proto::button_action_type action = [msg]() -> std::optional<surface_proto::button_action_type> {
            switch (msg) {
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
                return surface_proto::ACTION_PRESS;
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
                return surface_proto::ACTION_RELEASE;
            default:
                return std::nullopt;
            }
        }().value();

        surface_proto::modifier_button_type mods{};
        if (wParam & MK_CONTROL) { mods |= surface_proto::MOD_KEY_CTRL; }
        if (wParam & MK_SHIFT) { mods |= surface_proto::MOD_KEY_SHIFT; }
        if (wParam & MK_LBUTTON) { mods |= surface_proto::MOD_MOUSE_L; }
        if (wParam & MK_RBUTTON) { mods |= surface_proto::MOD_MOUSE_R; }
        if (wParam & MK_MBUTTON) { mods |= surface_proto::MOD_MOUSE_M; }

        //XUTL_TRACE("mouse event -> btn: {}, action: {}, mods: 0x{:X}"
        //    , static_cast<int>(btn)
        //    , static_cast<int>(action)
        //    , static_cast<std::underlying_type_t<surface_proto::modifier_button_type>>(mods)
        //);

        XUTL_ASSERT(std::errc{} == _consumer.send_mouse_button_event(POINT{ x, y }, btn, action, mods));

        return 0;
    }
    case WM_MOUSEMOVE:
    {
        const int32_t x = GET_X_LPARAM(lParam);
        const int32_t y = GET_Y_LPARAM(lParam);

        surface_proto::modifier_button_type mods{};
        if (wParam & MK_CONTROL) { mods |= surface_proto::MOD_KEY_CTRL; }
        if (wParam & MK_SHIFT) { mods |= surface_proto::MOD_KEY_SHIFT; }
        if (wParam & MK_LBUTTON) { mods |= surface_proto::MOD_MOUSE_L; }
        if (wParam & MK_RBUTTON) { mods |= surface_proto::MOD_MOUSE_R; }
        if (wParam & MK_MBUTTON) { mods |= surface_proto::MOD_MOUSE_M; }

        //XUTL_TRACE("mouse move event -> pos: ({}, {}), mods: 0x{:X}"
        //    , x, y
        //    , static_cast<std::underlying_type_t<surface_proto::modifier_button_type>>(mods)
        //);

        XUTL_ASSERT(std::errc{} == _consumer.send_mouse_move_event(POINT{ x, y }, mods));

        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        const int32_t raw_scroll_delta = GET_WHEEL_DELTA_WPARAM(wParam);

        // Same scroll value as GLFW: normalize one wheel notch to 1.0 / -1.0.
        const float yoffset = static_cast<float>(raw_scroll_delta) / static_cast<float>(WHEEL_DELTA);

        //XUTL_TRACE("mouse scroll event -> yoffset: {}", yoffset);

        XUTL_ASSERT(std::errc{} == _consumer.send_mouse_scroll_event(yoffset));

        return 0;
    }
    } // switch

    return ::DefWindowProcA(hWnd, msg, wParam, lParam);
}