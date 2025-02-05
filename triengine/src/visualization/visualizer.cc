#include "visualizer.hh"
#include "../misc/string_utils.hh"
#include "../misc/debug_utils.hh"
#include "../misc/gl_utils.hh"

#include <iostream>
#include <memory>

namespace triengine::visualization
{
    visualizer::visualizer()
    {
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

        // In to use the member function as callback, set the current class as the Window User Pointer
        ::glfwSetWindowUserPointer(_glctx.get_glfw_window(), this);

        // Set all callbacks
        ::glfwSetWindowCloseCallback(_glctx.get_glfw_window(),
            +[](GLFWwindow* window) {
                auto pThis = static_cast<visualizer*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_window_close_event(window);
            });

        ::glfwSetFramebufferSizeCallback(_glctx.get_glfw_window(),
            +[](GLFWwindow* window, int w, int h) {
                auto pThis = static_cast<visualizer*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_frame_buffer_resize_event(window, w, h);
            });

        ::glfwSetKeyCallback(_glctx.get_glfw_window(),
            +[](GLFWwindow* window, int key, int scancode, int action, int mods) {
                auto pThis = static_cast<visualizer*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_key_event(window, key, scancode, action, mods);
            });

        ::glfwSetMouseButtonCallback(_glctx.get_glfw_window(),
            +[](GLFWwindow* window, int button, int action, int mods) {
                auto pThis = static_cast<visualizer*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_mouse_button_event(window, button, action, mods);
            });

        ::glfwSetCursorPosCallback(_glctx.get_glfw_window(),
            +[](GLFWwindow* window, double xpos, double ypos) {
                auto pThis = static_cast<visualizer*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_mouse_move_event(window, xpos, ypos);
            });

        ::glfwSetScrollCallback(_glctx.get_glfw_window(),
            +[](GLFWwindow* window, double xoffset, double yoffset) {
                auto pThis = static_cast<visualizer*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_mouse_scroll_event(window, xoffset, yoffset);
            });

        ::glfwGetWindowContentScale(_glctx.get_glfw_window(), &_curr_dpi_scale_x, &_curr_dpi_scale_y);
        TRIENGINE_TRACE("dpi scale: %f x %f", _curr_dpi_scale_x, _curr_dpi_scale_y);

        ::glfwSetWindowContentScaleCallback(_glctx.get_glfw_window(),
            +[]([[maybe_unused]] GLFWwindow* window, float xscale, float yscale) {
                TRIENGINE_TRACE("dpi scale changed: [%f, %f]", xscale, yscale);
                auto pThis = static_cast<visualizer*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_content_scale_change_event(window, xscale, yscale);
            });

        ::glfwGetWindowSize(_glctx.get_glfw_window(), &_curr_window_width, &_curr_window_height);

        _scn_renderer.create(&_glctx);

        // Create main scene
        this->add_scene();

        // Initialize GUI system
        {
            _gui_mgr = std::make_unique<gui::gui_manager>();
            _gui_mgr->initialize(
                this,
                _curr_dpi_scale_x
            );

            _scene_window = _gui_mgr->get_scene_window();
        }

        // Special Geometries
        {
            auto& scn = *_curr_scn;

            _point_light_source_object = geometry::light_source_object::create(0.075f);
            _point_light_source_object->set_visible(scn.render_config.light_opts.point_light.enabled);
            _point_light_source_object->translate(scn.render_config.light_opts.point_light.position);
            _point_light_source_object->color = scn.render_config.light_opts.point_light.color;

            _origin_axis_frame_object = geometry::triangle_mesh_object::create_coordinate_frame(0.5f);
            _origin_axis_frame_object->set_visible(scn.render_config.show_origin_axis);
        }

        _flag_initialized = true;
    }

    void visualizer::close_window()
    {
        // Set the close flag
        ::glfwSetWindowShouldClose(_glctx.get_glfw_window(), GL_TRUE);
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

    void visualizer::set_window_position(int xpos, int ypos)
    {
        if (const auto gl_window = _glctx.get_glfw_window();
            gl_window) {
            ::glfwSetWindowPos(gl_window, xpos, ypos);
        }
    }

    bool visualizer::add_scene(std::shared_ptr<scene> new_scn)
    {
        if (_scn_map.empty()) { _curr_scn = new_scn; }
        const auto [it, success] = _scn_map.insert({ new_scn->id(), new_scn });
        return success;
    }

    void visualizer::remove_scene(std::shared_ptr<scene> scn)
    {
        if (scn) {
            auto it = _scn_map.find(scn->id());
            if (it != _scn_map.end()) {
                _scn_map.erase(it);
                if (_curr_scn->id() == scn->id()) {
                    _curr_scn = _scn_map.empty() ? nullptr : _scn_map.begin()->second;
                }
            }
        }
    }

    void visualizer::change_scene(std::shared_ptr<scene> scn) {
        _curr_scn = scn;
    }

    void visualizer::render()
    {
        // Render Scene
        _scene_window->bind_framebuffer();

        scene& scn = *_curr_scn;
        const vec2_i32 scn_size{ _scene_window->get_framebuffer_size() };
        scn.get_camera()->set_view_port(view_port{0, 0, scn_size.x(),  scn_size.y()});
        _scn_renderer.render_scene(scn);

        _scene_window->unbind_framebuffer();

        GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 1.0f)); // set clear color
        GLCall(::glClear(GL_COLOR_BUFFER_BIT)); // clear render buffers

        // Render GUI
        _gui_mgr->render();
    }

    bool visualizer::update_window()
    {
        ::glfwSwapBuffers(_glctx.get_glfw_window());
        ::glfwPollEvents();

        /**
         * https://www.glfw.org/docs/3.0/window.html
         *
         * When the user attempts to close the window,
         * for example by clicking the close widget or using a key chord like Alt+F4,
         * the close flag of the window is set.
         *
         * The window is however not actually destroyed and, unless you watch for this state change, nothing further happens.
         * The current state of the close flag is returned by glfwWindowShouldClose and can be set or cleared directly with glfwSetWindowShouldClose.
         */
        return !static_cast<bool>(::glfwWindowShouldClose(_glctx.get_glfw_window()));
    }

    void visualizer::_handle_glfw_window_close_event(
        [[maybe_unused]] GLFWwindow* const window)
    {
        if (_cb_close) {
            bool canceled = false;
            _cb_close(*this, canceled);
            if (canceled) {
                // cancel close requests (reset close flag)
                ::glfwSetWindowShouldClose(window, GL_FALSE);
            }
        }
    }

    void visualizer::_handle_glfw_frame_buffer_resize_event(
        [[maybe_unused]] GLFWwindow* const window,
        [[maybe_unused]] const int width,
        [[maybe_unused]] const int height)
    {
        _curr_window_width = width;
        _curr_window_height = height;
    }

    void visualizer::_handle_glfw_key_event(
        [[maybe_unused]] GLFWwindow* const window,
        [[maybe_unused]] const int key,
        [[maybe_unused]] const int scancode,
        [[maybe_unused]] const int action,
        [[maybe_unused]] const int mods)
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
            _cb_key(*this, key, scancode, action, mods, handled);
            if (handled) { return; }
        }

        // https://www.glfw.org/docs/latest/group__keys.html
        if (action == GLFW_RELEASE) { return; }

        switch (key) {
        case GLFW_KEY_HOME:
            _curr_scn->get_camera()->reset();
            break;
        }
    }

    void visualizer::_handle_glfw_mouse_button_event(
        [[maybe_unused]] GLFWwindow* const window,
        [[maybe_unused]] const int button,
        [[maybe_unused]] const int action,
        [[maybe_unused]] const int mods)
    {
        const vec2_f32 curr_cursor_screen_pos = misc::get_cursor_device_screen_pos(window);
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
            _cb_mouse_button(*this, button, action, mods, handled);
            if (handled) { return; }
        }

        if (action == GLFW_PRESS && cursor_test_succeeded) {
            _last_clicked_cursor_viewport_pos =
                _scene_window->try_convert_screen_pos_2_viewport_pos(curr_cursor_screen_pos).value();
        }
    }

    void visualizer::_handle_glfw_mouse_move_event(
        [[maybe_unused]] GLFWwindow* const window,
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
            _cb_mouse_move(*this, cursor_screen_xpos, cursor_screen_ypos, handled);
            if (handled) { return; }
        }

        if (cursor_test_succeeded)
        {
            camera& scn_camera = *_curr_scn->get_camera();

            const vec2_f32 curr_cursor_viewport_pos = 
                _scene_window->try_convert_screen_pos_2_viewport_pos(curr_cursor_screen_pos).value();

            const bool
                flag_l_mouse_pressed = GLFW_PRESS == ::glfwGetMouseButton(_glctx.get_glfw_window(), GLFW_MOUSE_BUTTON_LEFT),
                flag_r_mouse_pressed = GLFW_PRESS == ::glfwGetMouseButton(_glctx.get_glfw_window(), GLFW_MOUSE_BUTTON_RIGHT),
                flag_m_mouse_pressed = GLFW_PRESS == ::glfwGetMouseButton(_glctx.get_glfw_window(), GLFW_MOUSE_BUTTON_MIDDLE);

            if (!flag_l_mouse_pressed && !flag_r_mouse_pressed && !flag_m_mouse_pressed) {
                return; // ignore
            }

            if (flag_l_mouse_pressed) {
                scn_camera.process_mouse_move_for_rotation(curr_cursor_viewport_pos - _last_clicked_cursor_viewport_pos);
            }
            else if (flag_m_mouse_pressed) {
                scn_camera.process_mouse_move_for_translation(_last_clicked_cursor_viewport_pos, curr_cursor_viewport_pos);
            }

            _last_clicked_cursor_viewport_pos = curr_cursor_viewport_pos;
        }
    }

    void visualizer::_handle_glfw_mouse_scroll_event(
        [[maybe_unused]] GLFWwindow* const window,
        [[maybe_unused]] const double scroll_xoffset,
        [[maybe_unused]] const double scroll_yoffset)
    {
        const vec2_f32 curr_cursor_screen_pos = misc::get_cursor_device_screen_pos(window);
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
            _cb_mouse_scroll(*this, scroll_xoffset, scroll_yoffset, handled);
            if (handled) { return; }
        }

        if (cursor_test_succeeded)
        {
            camera& scn_camera = *_curr_scn->get_camera();

            const bool ctrl_pressed = ::glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
            if (!ctrl_pressed) {
                scn_camera.process_mouse_scroll_for_zoom(static_cast<float>(scroll_yoffset));
            } else {
                scn_camera.process_mouse_scroll_for_perspective(static_cast<float>(scroll_yoffset));
            }
        }
    }

    void visualizer::_handle_glfw_content_scale_change_event(
        [[maybe_unused]] GLFWwindow* const window,
        [[maybe_unused]] const float xscale,
        [[maybe_unused]] const float yscale)
    {
        _curr_dpi_scale_x = xscale;
        _curr_dpi_scale_y = yscale;
        if (_cb_dpi_change) {
            _cb_dpi_change(*this, xscale, yscale);
        }
    }

} // namespace
