#include "visualizer.hh"
#include <triengine/utility/string_format.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

#include <iostream>
#include <memory>

namespace triengine::visualization
{
    visualizer::visualizer()
    { }

    const core::gl_context* visualizer::get_gl_context() const noexcept {
        return &_glctx;
    }

    core::gl_context* visualizer::get_gl_context() noexcept {
        return &_glctx;
    }

    vec2_i32 visualizer::get_window_size() const {
        return _glctx.get_window_size();
    }

    vec2_f32 visualizer::get_window_dpi_scale() const {
        return _glctx.get_window_dpi_scale();
    }

    void visualizer::set_close_callback(close_callback cb) {
        _cb_close = std::move(cb);
    }

    void visualizer::set_dpi_change_callback(dpi_change_callback cb) {
        _cb_dpi_change = std::move(cb);
    }

    void visualizer::set_key_callback(key_callback cb) {
        _cb_key = std::move(cb);
    }

    void visualizer::set_mouse_button_callback(mouse_button_callback cb) {
        _cb_mouse_button = std::move(cb);
    }

    void visualizer::set_mouse_move_callback(mouse_move_callback cb) {
        _cb_mouse_move = std::move(cb);
    }

    void visualizer::set_mouse_scroll_callback(mouse_scroll_callback cb) {
        _cb_mouse_scroll = std::move(cb);
    }

    void visualizer::create_window(
        const std::string& window_name,
        const bool show_window,
        const int32_t window_width,
        const int32_t window_height,
        const bool fullscreen)
    {
        if (_flag_initialized) {
            TRIENGINE_PANIC("already created");
        }

        _glctx.create(
            window_name,
            show_window,
            window_width, 
            window_height,
            fullscreen
        );

        _glctx.set_close_callback(std::bind(&visualizer::_handle_close_event, this, 
            std::placeholders::_1));
        _glctx.set_frame_resize_callback(std::bind(&visualizer::_handle_frame_resize_event, this,
            std::placeholders::_1, std::placeholders::_2));
        _glctx.set_dpi_change_callback(std::bind(&visualizer::_handle_dpi_change_event, this,
            std::placeholders::_1, std::placeholders::_2));
        _glctx.set_key_callback(std::bind(&visualizer::_handle_key_event, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        _glctx.set_mouse_button_callback(std::bind(&visualizer::_handle_mouse_button_event, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _glctx.set_mouse_move_callback(std::bind(&visualizer::_handle_mouse_move_event, this,
            std::placeholders::_1, std::placeholders::_2));
        _glctx.set_mouse_scroll_callback(std::bind(&visualizer::_handle_mouse_scroll_event, this,
            std::placeholders::_1, std::placeholders::_2));

        _scn_renderer.create(&_glctx);

        // Initialize GUI system
        {
            _gui_mgr = std::make_unique<gui::gui_manager>();
            _gui_mgr->initialize(
                this,
                _glctx.get_window_dpi_scale().x()
            );

            _scene_window = _gui_mgr->get_scene_window();
        }

        _flag_initialized = true;
    }

    void visualizer::close_window()
    {
        _glctx.set_window_close_flag(true);
    }

    void visualizer::destroy_window()
    {
        if (_flag_initialized)
        {
            _flag_initialized = false;
            
            _gui_mgr->deinitialize();
            _gui_mgr.reset();

            _scn_renderer.destroy();
            _glctx.destroy();
        }
    }

    void visualizer::set_window_position(int32_t xpos, int32_t ypos)
    {
        _glctx.set_window_position(xpos, ypos);
    }

    std::shared_ptr<scene> visualizer::add_scene()
    {
        auto new_scn = std::make_shared<scene>(_glctx.get_gpu_resource_manager());
        if (_scn_id_map.count(new_scn->get_id())) {
            TRIENGINE_PANIC("Failed to add scene (id #%X already exists)", new_scn->get_id());
        }

        const bool is_first{ _scn_list.empty() };

        _scn_list.push_back(new_scn);
        _scn_id_map[new_scn->get_id()] = std::prev(_scn_list.end());

        if (is_first) {
            _curr_scn_it = std::prev(_scn_list.end());
        }

        return new_scn;
    }
    
    void visualizer::remove_scene(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        if (map_it != _scn_id_map.end()) {
            _scn_list.erase(map_it->second);
            _scn_id_map.erase(map_it);
            if ((*_curr_scn_it)->get_id() == scn_id) {
                _curr_scn_it = _scn_id_map.empty() 
                    ? _scn_list.end() 
                    : _scn_list.begin();
            }
        } else {
            TRIENGINE_WARN("Failed to remove scene #%X (not found)", scn_id);
        }
    }

    void visualizer::switch_scene(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        if (map_it == _scn_id_map.end()) {
            TRIENGINE_PANIC("Failed to change scene (invalid scene id #%X)", scn_id);
        }
        _curr_scn_it = map_it->second;
    }

    void visualizer::switch_to_previous_scene()
    {
        if (_curr_scn_it != _scn_list.end()) {
            _curr_scn_it = std::prev((_curr_scn_it != _scn_list.begin())
                ? _curr_scn_it
                : _scn_list.end()
            );
        }
    }

    void visualizer::switch_to_next_scene()
    {
        if (_curr_scn_it != _scn_list.end()) {
            const auto next_it = std::next(_curr_scn_it);
            _curr_scn_it = (next_it != _scn_list.end())
                ? next_it
                : _scn_list.begin();
        }
    }

    std::shared_ptr<const scene> visualizer::find_scene(scene_id_t scn_id) const
    {
        auto map_it = _scn_id_map.find(scn_id);
        return (map_it != _scn_id_map.end())
            ? *(map_it->second)
            : nullptr;
    }

    std::shared_ptr<scene> visualizer::find_scene(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        return (map_it != _scn_id_map.end())
            ? *(map_it->second)
            : nullptr;
    }

    std::shared_ptr<const scene> visualizer::get_current_scene() const
    {
        return (_curr_scn_it != _scn_list.end())
            ? *_curr_scn_it
            : nullptr;
    }

    std::shared_ptr<scene> visualizer::get_current_scene()
    {
        return (_curr_scn_it != _scn_list.end())
            ? *_curr_scn_it
            : nullptr;
    }

    void visualizer::render()
    {
        if (_curr_scn_it == _scn_list.end()) {
            TRIENGINE_PANIC("No scenes added");
            return;
        }

        // Render Scene
        _scene_window->bind_framebuffer();

        const core::frame_buffer& target_fb = _scene_window->get_framebuffer();
        scene& target_scn = *(_curr_scn_it->get());
        target_scn.get_camera()->set_view_port(view_port{ 0, 0, target_fb.width_pixels(), target_fb.height_pixels() });
        _scn_renderer.render(
            target_fb.fbo_id(),
            target_fb.width_pixels(),
            target_fb.height_pixels(),
            target_scn
        );

        _scene_window->unbind_framebuffer();

        GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 1.0f)); // set clear color
        GLCall(::glClear(GL_COLOR_BUFFER_BIT)); // clear render buffers

        // Render GUI
        _gui_mgr->render();
    }

    bool visualizer::update_window()
    {
        _glctx.swap_buffers();
        _glctx.poll_window_events();
        return !_glctx.get_window_close_flag();
    }

    void visualizer::_handle_close_event(
        bool& cancel)
    {
        if (_cb_close) {
            _cb_close(cancel);
        }
    }

    void visualizer::_handle_frame_resize_event(
        [[maybe_unused]] const int32_t width,
        [[maybe_unused]] const int32_t height)
    {
        // ...
    }

    void visualizer::_handle_key_event(
        [[maybe_unused]] const int32_t key,
        [[maybe_unused]] const int32_t scancode,
        [[maybe_unused]] const int32_t action,
        [[maybe_unused]] const int32_t mods)
    {
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if (ImGui::GetIO().WantCaptureKeyboard && !_scene_window->is_window_focused()) {
            return;
        }

        if (_cb_key) {
            bool handled = false;
            _cb_key(key, scancode, action, mods, handled);
            if (handled) { return; }
        }

        // https://www.glfw.org/docs/latest/group__keys.html
        if (action == GLFW_RELEASE) { return; }

        switch (key) {
        case GLFW_KEY_HOME:
            this->get_current_scene()->get_camera()->reset();
            break;
        }
    }

    void visualizer::_handle_mouse_button_event(
        [[maybe_unused]] const int32_t button,
        [[maybe_unused]] const int32_t action,
        [[maybe_unused]] const int32_t mods)
    {
        const vec2_f32 curr_cursor_screen_pos = _glctx.get_cursor_screen_pos();
        const bool cursor_test_succeeded = _scene_window->test_cursor_hovered(curr_cursor_screen_pos);

        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if (ImGui::GetIO().WantCaptureMouse && !cursor_test_succeeded) {
            return;
        }

        if (_cb_mouse_button) {
            bool handled = false;
            _cb_mouse_button(button, action, mods, handled);
            if (handled) { return; }
        }

        if (action == GLFW_PRESS && cursor_test_succeeded) {
            _last_clicked_cursor_viewport_pos =
                _scene_window->try_convert_screen_pos_2_viewport_pos(curr_cursor_screen_pos).value();
        }
    }

    void visualizer::_handle_mouse_move_event(
        [[maybe_unused]] const double cursor_screen_xpos,
        [[maybe_unused]] const double cursor_screen_ypos)
    {
        const vec2_f32 curr_cursor_screen_pos{
            static_cast<float>(cursor_screen_xpos),
            static_cast<float>(cursor_screen_ypos)
        };

        const bool cursor_test_succeeded = _scene_window->test_cursor_hovered(curr_cursor_screen_pos);

        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if (ImGui::GetIO().WantCaptureMouse && !cursor_test_succeeded) {
            return;
        }

        if (_cb_mouse_move) {
            bool handled = false;
            _cb_mouse_move(cursor_screen_xpos, cursor_screen_ypos, handled);
            if (handled) { return; }
        }

        if (cursor_test_succeeded)
        {
            const bool
                flag_l_mouse_pressed = GLFW_PRESS == ::glfwGetMouseButton(_glctx.get_glfw_window(), GLFW_MOUSE_BUTTON_LEFT),
                flag_r_mouse_pressed = GLFW_PRESS == ::glfwGetMouseButton(_glctx.get_glfw_window(), GLFW_MOUSE_BUTTON_RIGHT),
                flag_m_mouse_pressed = GLFW_PRESS == ::glfwGetMouseButton(_glctx.get_glfw_window(), GLFW_MOUSE_BUTTON_MIDDLE);

            if (flag_l_mouse_pressed || flag_r_mouse_pressed || flag_m_mouse_pressed)
            {
                const vec2_f32 curr_cursor_viewport_pos =
                    _scene_window->try_convert_screen_pos_2_viewport_pos(curr_cursor_screen_pos).value();

                camera* const scn_camera = this->get_current_scene()->get_camera();

                if (flag_l_mouse_pressed) {
                    scn_camera->process_mouse_move_for_rotation(curr_cursor_viewport_pos - _last_clicked_cursor_viewport_pos);
                }
                else if (flag_m_mouse_pressed) {
                    scn_camera->process_mouse_move_for_translation(_last_clicked_cursor_viewport_pos, curr_cursor_viewport_pos);
                }

                _last_clicked_cursor_viewport_pos = curr_cursor_viewport_pos;
            }
        }
    }

    void visualizer::_handle_mouse_scroll_event(
        [[maybe_unused]] const double scroll_xoffset,
        [[maybe_unused]] const double scroll_yoffset)
    {
        const vec2_f32 curr_cursor_screen_pos = _glctx.get_cursor_screen_pos();
        const bool cursor_test_succeeded = _scene_window->test_cursor_hovered(curr_cursor_screen_pos);

        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if (ImGui::GetIO().WantCaptureMouse && !cursor_test_succeeded) {
            return;
        }

        if (_cb_mouse_scroll) {
            bool handled = false;
            _cb_mouse_scroll(scroll_xoffset, scroll_yoffset, handled);
            if (handled) { return; }
        }

        if (cursor_test_succeeded)
        {
            camera* const scn_camera = this->get_current_scene()->get_camera();

            const bool ctrl_pressed = ::glfwGetKey(_glctx.get_glfw_window(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
            if (!ctrl_pressed) {
                scn_camera->process_mouse_scroll_for_zoom(static_cast<float>(scroll_yoffset));
            } else {
                scn_camera->process_mouse_scroll_for_perspective(static_cast<float>(scroll_yoffset));
            }
        }
    }

    void visualizer::_handle_dpi_change_event(
        [[maybe_unused]] const double dpi_xscale,
        [[maybe_unused]] const double dpi_yscale)
    {
        if (_cb_dpi_change) {
            _cb_dpi_change(dpi_xscale, dpi_yscale);
        }
    }

} // namespace
