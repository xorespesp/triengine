#include "tiny_image_viewer.hh" // Changed include extension

#include <algorithm> 
#include <vector>
#include <stdexcept>
#include <cstdint>      
#include <thread>       
#include <chrono>       

#ifdef TINY_VIEWER_PLATFORM_WINDOWS
#  include <windows.h>
#  include <windowsx.h> 
#  include <mutex> 
#endif

namespace tiny_viewer {

    // --- platform_window Interface (Internal) ---
    class platform_window {
    public:
        virtual ~platform_window() = default;
        // Changed title to std::string_view for ANSI compatibility
        virtual bool create_os_window(std::string_view title, int32_t w, int32_t h, tiny_image_viewer* owner) = 0;
        virtual void show_os_window() = 0;
        virtual void hide_os_window() = 0;
        virtual bool is_os_window_active() const = 0;
        virtual void process_os_events() = 0;
        virtual void close_os_window() = 0;
        virtual void request_os_redraw() = 0;
        virtual void update_os_display_surface(const std::vector<uint8_t>* bgra_data, int32_t img_w, int32_t img_h) = 0;
    protected:
        tiny_image_viewer* _owner_viewer = nullptr;
    };

#ifdef TINY_VIEWER_PLATFORM_WINDOWS
    // --- Helper function to map Windows VK codes to special_key (remains the same) ---
    special_key map_vk_to_special_key(::WPARAM vk_code) {
        switch (vk_code) {
        case VK_ESCAPE: return special_key::escape;
        case VK_RETURN: return special_key::enter;
        case VK_TAB:    return special_key::tab;
        case VK_BACK:   return special_key::backspace;
        case VK_INSERT: return special_key::insert;
        case VK_DELETE: return special_key::del;
        case VK_RIGHT:  return special_key::arrow_right;
        case VK_LEFT:   return special_key::arrow_left;
        case VK_DOWN:   return special_key::arrow_down;
        case VK_UP:     return special_key::arrow_up;
        case VK_PRIOR:  return special_key::page_up;
        case VK_NEXT:   return special_key::page_down;
        case VK_HOME:   return special_key::home;
        case VK_END:    return special_key::end;
        case VK_F1:     return special_key::f1; case VK_F2:  return special_key::f2; case VK_F3:  return special_key::f3;
        case VK_F4:     return special_key::f4; case VK_F5:  return special_key::f5; case VK_F6:  return special_key::f6;
        case VK_F7:     return special_key::f7; case VK_F8:  return special_key::f8; case VK_F9:  return special_key::f9;
        case VK_F10:    return special_key::f10; case VK_F11: return special_key::f11; case VK_F12: return special_key::f12;
        case VK_LSHIFT:   return special_key::left_shift;
        case VK_RSHIFT:   return special_key::right_shift;
        case VK_LCONTROL: return special_key::left_control;
        case VK_RCONTROL: return special_key::right_control;
        case VK_LMENU:    return special_key::left_alt;
        case VK_RMENU:    return special_key::right_alt;
        // VK_SHIFT, VK_CONTROL, VK_MENU are virtual keys that don't distinguish left/right.
        // GetKeyState or GetAsyncKeyState is better for modifier state.
        // map_vk_to_special_key focuses on specific key presses.
        default:        return special_key::unknown;
        }
    }

    // --- windows_platform_window Implementation (Internal) ---
    class windows_platform_window : public platform_window {
    public:
        windows_platform_window() : _hinstance(::GetModuleHandleA(nullptr)) {
            // Using std::to_string for uintptr_t to create a unique ANSI class name
            _window_class_name_ansi = "TinyImageViewerWindowClass_A_" + std::to_string(reinterpret_cast<uintptr_t>(this));
            ::ZeroMemory(&_bitmap_info, sizeof(::BITMAPINFO));
            _bitmap_info.bmiHeader.biSize = sizeof(::BITMAPINFOHEADER);
            _bitmap_info.bmiHeader.biPlanes = 1;
            _bitmap_info.bmiHeader.biBitCount = 32;
            _bitmap_info.bmiHeader.biCompression = BI_RGB;
        }

        ~windows_platform_window() override {
            close_os_window();
        }

        // Changed title to std::string_view
        bool create_os_window(std::string_view title, int32_t w, int32_t h, tiny_image_viewer* owner) override {
            if (_hwnd) {
                return true;
            }
            _owner_viewer = owner;

            ::WNDCLASSEXA wcex = {}; // Use ANSI version
            wcex.cbSize = sizeof(::WNDCLASSEXA); // Use ANSI version
            wcex.style = CS_HREDRAW | CS_VREDRAW;
            wcex.lpfnWndProc = windows_platform_window::_wnd_proc_router;
            wcex.hInstance = _hinstance;
            wcex.hIcon = ::LoadIconA(nullptr, IDI_APPLICATION);
            wcex.hCursor = ::LoadCursorA(nullptr, IDC_ARROW);
            wcex.hbrBackground = nullptr;
            wcex.lpszClassName = _window_class_name_ansi.c_str(); // Use ANSI class name

            if (!::RegisterClassExA(&wcex)) { // Use ANSI version
                if (::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
                    return false;
                }
            }
            _is_class_registered = true;

            ::RECT window_rect = { 0, 0, static_cast<::LONG>(w), static_cast<::LONG>(h) };
            ::AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

            // Convert title from std::string_view to std::string for .c_str() if needed,
            // or ensure title is from a null-terminated source if passing title.data().
            // For safety with string_view, creating a temporary std::string is robust.
            std::string temp_title(title);
            _hwnd = ::CreateWindowExA( // Use ANSI version
                0,
                _window_class_name_ansi.c_str(),
                temp_title.c_str(), // Use ANSI title
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT,
                window_rect.right - window_rect.left, window_rect.bottom - window_rect.top,
                nullptr, nullptr, _hinstance, this);

            if (_hwnd) {
                _is_event_loop_active = true;
            }
            return _hwnd != nullptr;
        }

        void show_os_window() override {
            if (_hwnd) {
                ::ShowWindow(_hwnd, SW_SHOW);
                ::UpdateWindow(_hwnd);
                _is_event_loop_active = true;
            }
        }

        void hide_os_window() override {
            if (_hwnd) {
                ::ShowWindow(_hwnd, SW_HIDE);
            }
        }

        bool is_os_window_active() const override {
            return _hwnd != nullptr && _is_event_loop_active;
        }

        void process_os_events() override {
            if (!_hwnd || !_is_event_loop_active) return;

            ::MSG msg = {};
            while (::PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) { // Use ANSI version
                if (msg.message == WM_QUIT) {
                    _is_event_loop_active = false;
                    return;
                }
                ::TranslateMessage(&msg);
                ::DispatchMessageA(&msg);  // Use ANSI version
            }
        }

        void close_os_window() override {
            if (_hwnd) {
                ::DestroyWindow(_hwnd);
            }
            if (_is_class_registered) {
                ::UnregisterClassA(_window_class_name_ansi.c_str(), _hinstance); // Use ANSI version
                _is_class_registered = false;
            }
            _hwnd = nullptr;
            _is_event_loop_active = false;
        }

        void request_os_redraw() override {
            if (_hwnd) {
                ::InvalidateRect(_hwnd, nullptr, FALSE);
            }
        }

        void update_os_display_surface(const std::vector<uint8_t>* bgra_data, int32_t img_w, int32_t img_h) override {
            // This method remains the same as pixel data is not character data
            std::lock_guard<std::mutex> lock(_buffer_mutex);
            if (!bgra_data || bgra_data->empty() || img_w <= 0 || img_h <= 0) {
                _current_image_data_ptr = nullptr;
                _active_image_width = 0;
                _active_image_height = 0;
            } else {
                _current_image_data_ptr = bgra_data;
                _active_image_width = img_w;
                _active_image_height = img_h;
                _bitmap_info.bmiHeader.biWidth = static_cast<::LONG>(img_w);
                _bitmap_info.bmiHeader.biHeight = -static_cast<::LONG>(img_h);
            }
        }

    private:
        static ::LRESULT CALLBACK _wnd_proc_router(::HWND hwnd, ::UINT msg, ::WPARAM w_param, ::LPARAM l_param) {
            windows_platform_window* self = nullptr;
            if (msg == WM_NCCREATE) {
                ::CREATESTRUCTA* create_struct = reinterpret_cast<::CREATESTRUCTA*>(l_param); // Use ANSI version
                self = reinterpret_cast<windows_platform_window*>(create_struct->lpCreateParams);
                ::SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<::LONG_PTR>(self)); // Use ANSI version
                self->_hwnd = hwnd;
            }
            else {
                self = reinterpret_cast<windows_platform_window*>(::GetWindowLongPtrA(hwnd, GWLP_USERDATA)); // Use ANSI version
            }

            if (self) {
                return self->_wnd_proc(hwnd, msg, w_param, l_param);
            }
            return ::DefWindowProcA(hwnd, msg, w_param, l_param); // Use ANSI version
        }

        ::LRESULT _wnd_proc(::HWND hwnd, ::UINT msg, ::WPARAM w_param, ::LPARAM l_param) {
            if (!_owner_viewer) {
                return ::DefWindowProcA(hwnd, msg, w_param, l_param); // Use ANSI version
            }

            key_modifiers mods;
            mods.shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
            mods.ctrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
            mods.alt = (::GetKeyState(VK_MENU) & 0x8000) != 0;

            switch (msg) {
            case WM_PAINT:
                _on_paint(hwnd);
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_SIZE:
            {
                int32_t new_width = static_cast<int32_t>(LOWORD(l_param));
                int32_t new_height = static_cast<int32_t>(HIWORD(l_param));
                _owner_viewer->_invoke_resize_callback(new_width, new_height);
            }
            return 0;
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            {
                special_key sk = map_vk_to_special_key(w_param);
                mods.alt = (msg == WM_SYSKEYDOWN) || ((::GetKeyState(VK_MENU) & 0x8000) != 0);
                _owner_viewer->_invoke_key_callback(key_action::press, sk, 0, mods);

                // For WM_SYSKEYDOWN, if not handled by app (e.g. Alt-F4), should pass to DefWindowProc.
                // Our callback doesn't return consumed status, so it will always pass.
                // If a key like F10 (menu activation) is handled, should return 0.
                if (msg == WM_SYSKEYDOWN && (w_param == VK_F10 || w_param == VK_MENU)) {
                    // Let system handle menu activation by Alt or F10
                } else if (sk != special_key::unknown && sk != special_key::none) {
                    // If we mapped it and want to consider it "handled" to suppress further processing like WM_CHAR for this vk.
                    // This depends on desired behavior. For now, let it fall through for TranslateMessage.
                }
            }
            break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                special_key sk = map_vk_to_special_key(w_param);
                mods.alt = (msg == WM_SYSKEYUP) || ((::GetKeyState(VK_MENU) & 0x8000) != 0);
                _owner_viewer->_invoke_key_callback(key_action::release, sk, 0, mods);
            }
            break;
            case WM_CHAR:
            {
                // wParam is the ANSI character code when not UNICODE
                _owner_viewer->_invoke_key_callback(key_action::press, special_key::none, static_cast<uint32_t>(static_cast<unsigned char>(w_param)), mods);
            }
            return 0;
            case WM_CLOSE:
                ::DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
            {
                std::lock_guard<std::mutex> lock(_buffer_mutex);
                _current_image_data_ptr = nullptr;
            }
            ::PostQuitMessage(0);
            _is_event_loop_active = false;
            _hwnd = nullptr;
            return 0;
            }
            return ::DefWindowProcA(hwnd, msg, w_param, l_param); // Use ANSI version
        }

        void _on_paint(::HWND hwnd_paint) {
            ::PAINTSTRUCT paint_struct;
            ::HDC hdc = ::BeginPaint(hwnd_paint, &paint_struct);
            ::RECT client_rect;
            ::GetClientRect(hwnd_paint, &client_rect);
            int32_t window_width = static_cast<int32_t>(client_rect.right - client_rect.left);
            int32_t window_height = static_cast<int32_t>(client_rect.bottom - client_rect.top);

            ::HDC mem_dc = ::CreateCompatibleDC(hdc);
            ::HBITMAP mem_bitmap = ::CreateCompatibleBitmap(hdc, window_width, window_height);
            ::HBITMAP old_bitmap = (::HBITMAP)::SelectObject(mem_dc, mem_bitmap);

            ::HBRUSH bg_brush = ::CreateSolidBrush(RGB(30, 30, 30));
            ::FillRect(mem_dc, &client_rect, bg_brush);
            ::DeleteObject(bg_brush);

            int32_t local_image_width = 0;
            int32_t local_image_height = 0;
            const std::vector<uint8_t>* data_ptr = nullptr;
            ::BITMAPINFO current_bitmap_info;
            {
                std::lock_guard<std::mutex> lock(_buffer_mutex);
                if (!_current_image_data_ptr || _current_image_data_ptr->empty() ||
                    _active_image_width <= 0 || _active_image_height <= 0 ||
                    window_width <= 0 || window_height <= 0) {
                    ::BitBlt(hdc, 0, 0, window_width, window_height, mem_dc, 0, 0, SRCCOPY);
                    ::SelectObject(mem_dc, old_bitmap);
                    ::DeleteObject(mem_bitmap);
                    ::DeleteDC(mem_dc);
                    ::EndPaint(hwnd_paint, &paint_struct);
                    return;
                }
                data_ptr = _current_image_data_ptr;
                local_image_width = _active_image_width;
                local_image_height = _active_image_height;
                current_bitmap_info = _bitmap_info;
            }

            if (!data_ptr) {
                ::BitBlt(hdc, 0, 0, window_width, window_height, mem_dc, 0, 0, SRCCOPY);
                ::SelectObject(mem_dc, old_bitmap);
                ::DeleteObject(mem_bitmap);
                ::DeleteDC(mem_dc);
                ::EndPaint(hwnd_paint, &paint_struct);
                return;
            }

            scale_mode current_scale_mode = _owner_viewer ? _owner_viewer->get_scale_mode() : scale_mode::fit_window;
            int32_t target_x = 0, target_y = 0, target_width = 0, target_height = 0;
            if (current_scale_mode == scale_mode::stretch_to_fill) {
                target_width = window_width;
                target_height = window_height;
            }
            else if (current_scale_mode == scale_mode::original_size) {
                target_width = local_image_width;
                target_height = local_image_height;
                target_x = (window_width - target_width) / 2;
                target_y = (window_height - target_height) / 2;
            }
            else {
                float img_aspect_ratio = static_cast<float>(local_image_width) / local_image_height;
                if (local_image_height == 0) img_aspect_ratio = 1.0f; // Avoid division by zero
                float win_aspect_ratio = static_cast<float>(window_width) / window_height;
                if (window_height == 0) win_aspect_ratio = 1.0f; // Avoid division by zero

                if (img_aspect_ratio > win_aspect_ratio) {
                    target_width = window_width;
                    target_height = (local_image_height == 0) ? 0 : static_cast<int32_t>(static_cast<float>(window_width) / img_aspect_ratio);
                }
                else {
                    target_height = window_height;
                    target_width = (img_aspect_ratio == 0) ? 0 : static_cast<int32_t>(static_cast<float>(window_height) * img_aspect_ratio);
                }
                target_x = (window_width - target_width) / 2;
                target_y = (window_height - target_height) / 2;
            }

            ::SetStretchBltMode(mem_dc, COLORONCOLOR);
            if (target_width > 0 && target_height > 0 && local_image_width > 0 && local_image_height > 0) {
                ::StretchDIBits(mem_dc,
                    static_cast<int>(target_x), static_cast<int>(target_y),
                    static_cast<int>(target_width), static_cast<int>(target_height),
                    0, 0, static_cast<int>(local_image_width), static_cast<int>(local_image_height),
                    data_ptr->data(),
                    &current_bitmap_info, DIB_RGB_COLORS, SRCCOPY);
            }

            ::BitBlt(hdc, 0, 0, window_width, window_height, mem_dc, 0, 0, SRCCOPY);

            ::SelectObject(mem_dc, old_bitmap);
            ::DeleteObject(mem_bitmap);
            ::DeleteDC(mem_dc);

            ::EndPaint(hwnd_paint, &paint_struct);
        }

        ::HWND _hwnd = nullptr;
        std::string _window_class_name_ansi; // Changed to std::string for ANSI
        ::HINSTANCE _hinstance;
        bool _is_event_loop_active = false;
        bool _is_class_registered = false;

        const std::vector<uint8_t>* _current_image_data_ptr = nullptr;
        int32_t _active_image_width = 0;
        int32_t _active_image_height = 0;
        ::BITMAPINFO _bitmap_info;
        std::mutex _buffer_mutex;
    };

#else 
    class stub_platform_window : public platform_window {
    public:
        // Changed title to std::string_view
        bool create_os_window(std::string_view, int32_t, int32_t, tiny_image_viewer*) override {
            throw std::runtime_error("GUI not implemented on this platform."); return false;
        }
        void show_os_window() override { throw std::runtime_error("GUI not implemented on this platform."); }
        void hide_os_window() override { throw std::runtime_error("GUI not implemented on this platform."); }
        bool is_os_window_active() const override { return false; }
        void process_os_events() override { /* No-op or throw */ }
        void close_os_window() override { /* No-op */ }
        void request_os_redraw() override { /* No-op */ }
        void update_os_display_surface(const std::vector<uint8_t>*, int32_t, int32_t) override { /* No-op */ }
    };
#endif

    std::unique_ptr<platform_window> create_platform_window_instance() {
#ifdef TINY_VIEWER_PLATFORM_WINDOWS
        return std::make_unique<windows_platform_window>();
#else
        return std::make_unique<stub_platform_window>();
#endif
    }

    void tiny_image_viewer::image_buffer::reset() {
        _source_data.clear(); _source_width = 0; _source_height = 0;
        _display_buffer_bgra.clear(); _display_buffer_width = 0; _display_buffer_height = 0;
    }

    tiny_image_viewer::tiny_image_viewer(std::string_view title, int32_t initial_width, int32_t initial_height)
        : _window_title(title), // _window_title is already std::string
        _initial_window_width(initial_width),
        _initial_window_height(initial_height) {
        _platform_window = create_platform_window_instance();
        if (!_platform_window) {
            throw std::runtime_error("Failed to create platform window backend.");
        }
    }

    tiny_image_viewer::~tiny_image_viewer() {
        close_window();
    }

    bool tiny_image_viewer::create_window() {
        if (_is_window_actually_created) {
            return true;
        }
        if (!_platform_window) {
            throw std::runtime_error("Platform window backend is null.");
        }
        // _window_title is std::string, directly usable with std::string_view for create_os_window
        _is_window_actually_created = _platform_window->create_os_window(_window_title, _initial_window_width, _initial_window_height, this);

        if (_is_window_actually_created) {
            if (_current_image_buffer.is_valid_source()) {
                _process_image_for_display();
            }
            else {
                _platform_window->update_os_display_surface(nullptr, 0, 0);
            }
        }
        return _is_window_actually_created;
    }

    void tiny_image_viewer::show_window() {
        if (!_is_window_actually_created) {
            if (!create_window()) {
                throw std::runtime_error("Window must be created successfully before calling show_window().");
            }
        }
        if (_platform_window) {
            _platform_window->show_os_window();
        }
    }

    void tiny_image_viewer::hide_window() {
        if (_platform_window && _is_window_actually_created) {
            _platform_window->hide_os_window();
        }
    }

    bool tiny_image_viewer::is_open() const {
        return _is_window_actually_created && _platform_window && _platform_window->is_os_window_active();
    }

    void tiny_image_viewer::process_events() {
        if (_platform_window && _is_window_actually_created && _platform_window->is_os_window_active()) {
            _platform_window->process_os_events();
        }
    }

    void tiny_image_viewer::close_window() {
        if (_platform_window) {
            _platform_window->close_os_window();
        }
        _is_window_actually_created = false;
    }

    void tiny_image_viewer::set_image(const uint8_t* pixel_data, int32_t image_width, int32_t image_height, image_format pixel_format) {
        if (!pixel_data || image_width <= 0 || image_height <= 0) {
            clear_image();
            return;
        }
        _current_image_buffer._source_width = image_width;
        _current_image_buffer._source_height = image_height;
        _current_image_buffer._source_format = pixel_format;
        switch (pixel_format) {
        case image_format::rgb:  _current_image_buffer._source_channels = 3; break;
        case image_format::bgr:  _current_image_buffer._source_channels = 3; break;
        case image_format::rgba: _current_image_buffer._source_channels = 4; break;
        case image_format::bgra: _current_image_buffer._source_channels = 4; break;
        default: throw std::runtime_error("Unsupported image format for set_image.");
        }
        size_t data_size = static_cast<size_t>(image_width) * static_cast<size_t>(image_height) * static_cast<size_t>(_current_image_buffer._source_channels);
        _current_image_buffer._source_data.assign(pixel_data, pixel_data + data_size);
        _process_image_for_display();
        if (_is_window_actually_created && _platform_window && _platform_window->is_os_window_active()) {
            request_redraw();
        }
    }

    void tiny_image_viewer::clear_image() {
        _current_image_buffer.reset();
        _process_image_for_display();
        if (_is_window_actually_created && _platform_window && _platform_window->is_os_window_active()) {
            request_redraw();
        }
    }

    void tiny_image_viewer::set_scale_mode(scale_mode mode) {
        _current_scale_mode = mode;
        if (_is_window_actually_created && _platform_window && _platform_window->is_os_window_active()) {
            request_redraw();
        }
    }

    scale_mode tiny_image_viewer::get_scale_mode() const { return _current_scale_mode; }

    void tiny_image_viewer::set_flip_axis(flip_axis axis) {
        if (_current_flip_axis != axis) {
            _current_flip_axis = axis;
            _process_image_for_display();
            if (_is_window_actually_created && _platform_window && _platform_window->is_os_window_active()) {
                request_redraw();
            }
        }
    }

    flip_axis tiny_image_viewer::get_flip_axis() const { return _current_flip_axis; }

    void tiny_image_viewer::request_redraw() {
        if (_platform_window && _is_window_actually_created && _platform_window->is_os_window_active()) {
            _platform_window->request_os_redraw();
        }
    }

    void tiny_image_viewer::run() {
        if (!create_window()) {
            return;
        }
        show_window();

        if (!_platform_window) {
            throw std::runtime_error("Platform window is null in run().");
        }

        while (is_open()) {
            process_events();
            if (is_open()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        _is_window_actually_created = false;
    }

    void tiny_image_viewer::set_key_callback(key_callback_type callback) {
        _key_callback = std::move(callback);
    }

    void tiny_image_viewer::set_resize_callback(resize_callback_type callback) {
        _resize_callback = std::move(callback);
    }

    void tiny_image_viewer::_invoke_key_callback(key_action action, special_key skey, uint32_t character_code, key_modifiers modifiers) {
        if (_key_callback) {
            _key_callback(action, skey, character_code, modifiers);
        }
    }

    void tiny_image_viewer::_invoke_resize_callback(int32_t new_client_width, int32_t new_client_height) {
        if (_resize_callback) {
            _resize_callback(new_client_width, new_client_height);
        }
    }


    void tiny_image_viewer::_process_image_for_display() {
        if (!_current_image_buffer.is_valid_source()) {
            _current_image_buffer._display_buffer_bgra.clear();
            _current_image_buffer._display_buffer_width = 0;
            _current_image_buffer._display_buffer_height = 0;
            if (_platform_window && _is_window_actually_created) {
                _platform_window->update_os_display_surface(nullptr, 0, 0);
            }
            return;
        }
        _convert_to_bgra(_current_image_buffer._source_data.data(),
            _current_image_buffer._source_width, _current_image_buffer._source_height,
            _current_image_buffer._source_channels, _current_image_buffer._source_format,
            _current_image_buffer._display_buffer_bgra);
        _current_image_buffer._display_buffer_width = _current_image_buffer._source_width;
        _current_image_buffer._display_buffer_height = _current_image_buffer._source_height;
        if (_current_flip_axis != flip_axis::none) {
            _flip_image_data_inplace(_current_image_buffer._display_buffer_bgra,
                _current_image_buffer._display_buffer_width, _current_image_buffer._display_buffer_height,
                4, _current_flip_axis);
        }
        if (_platform_window && _is_window_actually_created) {
            _platform_window->update_os_display_surface(&_current_image_buffer._display_buffer_bgra,
                _current_image_buffer._display_buffer_width,
                _current_image_buffer._display_buffer_height);
        }
    }

    void tiny_image_viewer::_convert_to_bgra(
        const uint8_t* input_data, int32_t img_w, int32_t img_h, int32_t num_channels, image_format input_fmt,
        std::vector<uint8_t>& output_bgra_data)
    {
        size_t num_pixels = static_cast<size_t>(img_w) * static_cast<size_t>(img_h);
        output_bgra_data.resize(num_pixels * 4);
        for (size_t i = 0; i < num_pixels; ++i) {
            const uint8_t* p_in = input_data + i * static_cast<size_t>(num_channels);
            uint8_t* p_out = output_bgra_data.data() + i * 4;
            switch (input_fmt) {
            case image_format::rgb:  p_out[0] = p_in[2]; p_out[1] = p_in[1]; p_out[2] = p_in[0]; p_out[3] = 255; break;
            case image_format::bgr:  p_out[0] = p_in[0]; p_out[1] = p_in[1]; p_out[2] = p_in[2]; p_out[3] = 255; break;
            case image_format::rgba: p_out[0] = p_in[2]; p_out[1] = p_in[1]; p_out[2] = p_in[0]; p_out[3] = p_in[3]; break;
            case image_format::bgra: p_out[0] = p_in[0]; p_out[1] = p_in[1]; p_out[2] = p_in[2]; p_out[3] = p_in[3]; break;
            }
        }
    }

    void tiny_image_viewer::_flip_image_data_inplace(
        std::vector<uint8_t>& image_data, int32_t img_w, int32_t img_h, int32_t channels_per_pixel, flip_axis axis)
    {
        if (image_data.empty() || img_w <= 0 || img_h <= 0 || channels_per_pixel <= 0) return;
        if (axis == flip_axis::none) return;
        size_t row_stride_bytes = static_cast<size_t>(img_w) * static_cast<size_t>(channels_per_pixel);
        if (axis == flip_axis::vertical || axis == flip_axis::both) {
            std::vector<uint8_t> temp_row(row_stride_bytes);
            for (int32_t y_idx = 0; y_idx < img_h / 2; ++y_idx) {
                uint8_t* row1_ptr = image_data.data() + static_cast<size_t>(y_idx) * row_stride_bytes;
                uint8_t* row2_ptr = image_data.data() + static_cast<size_t>(img_h - 1 - y_idx) * row_stride_bytes;
                memcpy(temp_row.data(), row1_ptr, row_stride_bytes);
                memcpy(row1_ptr, row2_ptr, row_stride_bytes);
                memcpy(row2_ptr, temp_row.data(), row_stride_bytes);
            }
        }
        if (axis == flip_axis::horizontal || axis == flip_axis::both) {
            for (int32_t y_idx = 0; y_idx < img_h; ++y_idx) {
                uint8_t* row_start_ptr = image_data.data() + static_cast<size_t>(y_idx) * row_stride_bytes;
                for (int32_t x_idx = 0; x_idx < img_w / 2; ++x_idx) {
                    uint8_t* pixel1_ptr = row_start_ptr + static_cast<size_t>(x_idx) * channels_per_pixel;
                    uint8_t* pixel2_ptr = row_start_ptr + static_cast<size_t>(img_w - 1 - x_idx) * channels_per_pixel;
                    for (int32_t channel_idx = 0; channel_idx < channels_per_pixel; ++channel_idx) {
                        std::swap(pixel1_ptr[channel_idx], pixel2_ptr[channel_idx]);
                    }
                }
            }
        }
    }

} // namespace tiny_viewer