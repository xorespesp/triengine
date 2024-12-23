#include "visualizer_window.hh"
#include "misc/string_utils.hh"
#include "misc/debug_utils.hh"
#include "misc/opengl_utils.hh"

#include <iostream>
#include <memory>

namespace triengine
{
    namespace {
        namespace detail
        {
            // NOTE: Device screen coordinates are relative to the upper-left corner of the window content area.
            inline vec2_f32 get_cursor_device_screen_pos(
                GLFWwindow* const glfw_window)
            {
                double xpos, ypos;
                ::glfwGetCursorPos(glfw_window, &xpos, &ypos);
                return vec2_f32{ static_cast<float>(xpos), static_cast<float>(ypos) };
            }

            // Basic Ref: https://learnopengl.com/In-Practice/Debugging
            void APIENTRY gl_debug_output_callback(
                const GLenum source,
                const GLenum type,
                const unsigned int id,
                const GLenum severity,
                const GLsizei message_length,
                const char* const message,
                [[maybe_unused]] const void* const userParam)
            {
                // ignore non-significant error/warning codes
                if (id == 131169 || id == 131185 || id == 131218 || id == 131204) {
                    return;
                }

                const std::string msg = misc::string::c_format(""
                    "\n------------------------------------------------------------"
                    "\nOpenGL Debug message (%lu) : %.*s"
                    "\nOpenGL Debug Source: %s"
                    "\nOpenGL Debug Type: %s"
                    "\nOpenGL Debug Severity: %s"
                    "\n------------------------------------------------------------"
                    , id
                    , message_length
                    , message
                    , [source]() -> const char* {
                        switch (source) {
                        case GL_DEBUG_SOURCE_API:             return "API";
                        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   return "Window System";
                        case GL_DEBUG_SOURCE_SHADER_COMPILER: return "Shader Compiler";
                        case GL_DEBUG_SOURCE_THIRD_PARTY:     return "Third Party";
                        case GL_DEBUG_SOURCE_APPLICATION:     return "Application";
                        case GL_DEBUG_SOURCE_OTHER:           return "Other";
                        default: return "???";
                        }
                    }()
                    , [type]() -> const char* {
                        switch (type) {
                        case GL_DEBUG_TYPE_ERROR:               return "Error";
                        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Deprecated Behaviour";
                        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  return "Undefined Behaviour";
                        case GL_DEBUG_TYPE_PORTABILITY:         return "Portability";
                        case GL_DEBUG_TYPE_PERFORMANCE:         return "Performance";
                        case GL_DEBUG_TYPE_MARKER:              return "Marker";
                        case GL_DEBUG_TYPE_PUSH_GROUP:          return "Push Group";
                        case GL_DEBUG_TYPE_POP_GROUP:           return "Pop Group";
                        case GL_DEBUG_TYPE_OTHER:               return "Other";
                        default: return "???";
                        }
                    }()
                    , [severity]() -> const char* {
                        switch (severity) {
                        case GL_DEBUG_SEVERITY_HIGH:         return "High";
                        case GL_DEBUG_SEVERITY_MEDIUM:       return "Medium";
                        case GL_DEBUG_SEVERITY_LOW:          return "Low";
                        case GL_DEBUG_SEVERITY_NOTIFICATION: return "Notification";
                        default: return "???";
                        }
                    }()
                );

                std::cout << msg << std::endl;
            }

        } // namespace
    } // namespace

    visualizer_window::visualizer_window()
    {
    }

    void visualizer_window::create_window(
        const std::string& window_name,
        const bool show_window,
        const int width,
        const int height,
        const bool fullscreen)
    {
        TRIENGINE_ASSERT(!_flag_initialized);
        _flag_initialized = true;

        // glfw: initialize and configure
        // ------------------------------

        // NOTE: Should be called in main thread
        misc::global_glfw_environment::initialize();

        ::glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        ::glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

        // NOTE: https://stackoverflow.com/a/27762480
        ::glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
        ::glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Enable OpenGL forward-compatibility

#if defined (TRIENGINE_DEBUG_MODE)
        //::glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE); // Enable OpenGL debug context
#endif // ^^^ TRIENGINE_DEBUG_MODE ^^^

        ::glfwWindowHint(GLFW_SAMPLES, 4); // Set framebuffer MSAA quality to 4x

        GLFWmonitor* const monitor_info = ::glfwGetPrimaryMonitor();
        const GLFWvidmode* const display_info = ::glfwGetVideoMode(monitor_info);

        if (fullscreen)
        {
            int num_modes = 0, best_mode_idx = 0;
            const GLFWvidmode* const modes = ::glfwGetVideoModes(monitor_info, &num_modes);
            for (int i = 0; i < num_modes; ++i) {
                if ((modes[i].width >= modes[best_mode_idx].width) ||
                    (modes[i].height >= modes[best_mode_idx].height) ||
                    (modes[i].refreshRate >= modes[best_mode_idx].refreshRate) ||
                    (modes[i].blueBits + modes[i].greenBits + modes[i].redBits) >= (modes[best_mode_idx].blueBits + modes[best_mode_idx].greenBits + modes[best_mode_idx].redBits))
                {
                    best_mode_idx = i;
                }
            }

            _curr_window_width = modes[best_mode_idx].width;
            _curr_window_height = modes[best_mode_idx].height;
        }
        else // !fullscreen
        {
            if (const bool auto_size_window = (width <= 0 || height <= 0);
                auto_size_window)
            {
                // Create invisible temporary window
                // for size measurement of non-client area
                ::glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
                std::shared_ptr<GLFWwindow> tmp_window{
                    ::glfwCreateWindow(1, 1, "", nullptr, nullptr),
                    ::glfwDestroyWindow
                };

                if (!tmp_window) {
                    ::glfwTerminate();
                    TRIENGINE_PANIC("Failed to create temporary window for frame size measurement");
                }

                // get non-client area size
                int pad_left{}, pad_top{}, pad_right{}, pad_bottom{};
                ::glfwGetWindowFrameSize(
                    tmp_window.get(),
                    &pad_left,
                    &pad_top,
                    &pad_right,
                    &pad_bottom
                );
                TRIENGINE_TRACE("frame size: %d,%d,%d,%d", pad_left, pad_top, pad_right, pad_bottom);

                constexpr float // Default window size ratio relative to the full display screen
                    kDefaultWindowWidthRatio = 0.95f,
                    kDefaultWindowHeightRatio = 0.9f;

                // client-area width, height
                _curr_window_width = static_cast<int>(display_info->width * kDefaultWindowWidthRatio) - (pad_left + pad_right);
                _curr_window_height = static_cast<int>(display_info->height * kDefaultWindowHeightRatio) - (pad_top + pad_bottom);
            }
            else
            {
                _curr_window_width = width;
                _curr_window_height = height;
            }
        }

        ::glfwWindowHint(GLFW_VISIBLE, show_window ? GLFW_TRUE : GLFW_FALSE);

        // Create window
        TRIENGINE_TRACE("Creating window.. (size: %dx%d)", _curr_window_width, _curr_window_height);
        _glfw_window = std::shared_ptr<GLFWwindow>(
            ::glfwCreateWindow(
                _curr_window_width,
                _curr_window_height,
                window_name.c_str(),
                (fullscreen) ? monitor_info : nullptr,
                nullptr
            ),
            ::glfwDestroyWindow
        );

        if (!_glfw_window) {
            ::glfwTerminate();
            TRIENGINE_PANIC("Failed to create GLFW window");
        }

        if (!fullscreen) {
            // the upper-left corner of the window client position
            const vec2_i32 window_client_start_pos{
                (display_info->width - _curr_window_width) / 2,
                (display_info->height - _curr_window_height) / 2
            };

            // set window client start position (upper-left corner position)
            ::glfwSetWindowPos(_glfw_window.get(), window_client_start_pos.x(), window_client_start_pos.y());
        }

        // In to use the member function as callback, set the current class as the Window User Pointer
        ::glfwSetWindowUserPointer(_glfw_window.get(), this);

        // Set all callbacks
        ::glfwSetWindowCloseCallback(_glfw_window.get(),
            +[](GLFWwindow* window) {
                auto pThis = static_cast<visualizer_window*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_window_close_event(window);
            });

        ::glfwSetFramebufferSizeCallback(_glfw_window.get(),
            +[](GLFWwindow* window, int w, int h) {
                auto pThis = static_cast<visualizer_window*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_frame_buffer_resize_event(window, w, h);
            });

        ::glfwSetKeyCallback(_glfw_window.get(),
            +[](GLFWwindow* window, int key, int scancode, int action, int mods) {
                auto pThis = static_cast<visualizer_window*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_key_event(window, key, scancode, action, mods);
            });

        ::glfwSetMouseButtonCallback(_glfw_window.get(),
            +[](GLFWwindow* window, int button, int action, int mods) {
                auto pThis = static_cast<visualizer_window*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_mouse_button_event(window, button, action, mods);
            });

        ::glfwSetCursorPosCallback(_glfw_window.get(),
            +[](GLFWwindow* window, double xpos, double ypos) {
                auto pThis = static_cast<visualizer_window*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_mouse_move_event(window, xpos, ypos);
            });

        ::glfwSetScrollCallback(_glfw_window.get(),
            +[](GLFWwindow* window, double xoffset, double yoffset) {
                auto pThis = static_cast<visualizer_window*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_mouse_scroll_event(window, xoffset, yoffset);
            });

        ::glfwGetWindowContentScale(_glfw_window.get(), &_curr_dpi_scale_x, &_curr_dpi_scale_y);
        TRIENGINE_TRACE("dpi scale: %f x %f", _curr_dpi_scale_x, _curr_dpi_scale_y);

        ::glfwSetWindowContentScaleCallback(_glfw_window.get(),
            +[]([[maybe_unused]] GLFWwindow* window, float xscale, float yscale) {
                TRIENGINE_TRACE("dpi scale changed: [%f, %f]", xscale, yscale);
                auto pThis = static_cast<visualizer_window*>(::glfwGetWindowUserPointer(window));
                pThis->_handle_glfw_content_scale_change_event(window, xscale, yscale);
            });

        ::glfwMakeContextCurrent(_glfw_window.get());

        // glad: load all OpenGL function pointers
        // ---------------------------------------
        if (!::gladLoadGLLoader((GLADloadproc)::glfwGetProcAddress)) {
            ::glfwTerminate();
            TRIENGINE_PANIC("Failed to load GL functions");
        }

        const GLint gl_context_flags = []() -> GLint { GLint flags{}; ::glGetIntegerv(GL_CONTEXT_FLAGS, &flags); return flags; }();
        if (gl_context_flags & GL_CONTEXT_FLAG_DEBUG_BIT)
        {
            // Initialize OpenGL debug output
            // Basic Ref: https://learnopengl.com/In-Practice/Debugging
            ::glEnable(GL_DEBUG_OUTPUT);
            ::glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            ::glDebugMessageCallback(detail::gl_debug_output_callback, nullptr/* userParam */);
            ::glDebugMessageControl(
                /* GLenum source     */GL_DONT_CARE,
                /* GLenum type       */GL_DONT_CARE,
                /* GLenum severity   */GL_DONT_CARE,
                /* GLsizei count     */0,
                /* const GLuint* ids */nullptr,
                /* GLboolean enabled */GL_TRUE
            );
        }

        ::glfwSwapInterval((show_window) ? 1 : 0); // glfwSwapInterval(1) -> Enable vsync

        // Context Settings
        GLCall(::glEnable(GL_MULTISAMPLE));
        GLCall(::glDisable(GL_BLEND));
        GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
        GLCall(::glClearDepth(1.0f));

        _light_source_renderer.create(_glfw_window.get());
        _mesh_renderer.create(_glfw_window.get());
        _lineset_renderer.create(_glfw_window.get());
        _pcd_renderer.create(_glfw_window.get());
        _skeleton_renderer.create(_glfw_window.get());

        // Initialize GUI system
        {
            _gui_mgr = std::make_unique<gui::gui_manager>();
            _gui_mgr->initialize(
                this,
                _curr_dpi_scale_x
            );

            _scene_window = _gui_mgr->get_scene_window();
        }

        _point_light_source_object = geometry::light_source_object::create(0.075f);
        _point_light_source_object->set_visible(_render_config.light_opts.point_light.enabled);
        _point_light_source_object->translate(_render_config.light_opts.point_light.position);
        _point_light_source_object->color = _render_config.light_opts.point_light.color;
        _light_source_renderer.add_object(_point_light_source_object);

        _origin_axis_frame_object = geometry::triangle_mesh_object::create_coordinate_frame(0.5f);
        _origin_axis_frame_object->set_visible(_render_config.show_origin_axis);
        _mesh_renderer.add_object(_origin_axis_frame_object);

        _origin_xz_plane_object = geometry::lineset_object::create_xz_plane(200.0f, 200);
        _origin_xz_plane_object->paint_uniform_color(color3_f32{ 0.3f, 0.3f, 0.3f });
        _origin_xz_plane_object->set_visible(_render_config.show_origin_xz_plane);
        _lineset_renderer.add_object(_origin_xz_plane_object);
    }

    void visualizer_window::close_window()
    {
        // Set the close flag
        ::glfwSetWindowShouldClose(_glfw_window.get(), GL_TRUE);
    }

    void visualizer_window::destroy_window()
    {
        if (_flag_initialized)
        {
            _flag_initialized = false;
            
            _gui_mgr->deinitialize();
            _gui_mgr.reset();

            _skeleton_renderer.destroy();
            _pcd_renderer.destroy();
            _lineset_renderer.destroy();
            _mesh_renderer.destroy();
            _light_source_renderer.destroy();

            _glfw_window.reset();
        }
    }

    void visualizer_window::set_window_position(int xpos, int ypos)
    {
        if (_glfw_window) {
            ::glfwSetWindowPos(_glfw_window.get(), xpos, ypos);
        }
    }

    void visualizer_window::set_vertical_fov(float fovy_deg)
    {
        for (auto& cam : _camera_list) {
            cam->set_vertical_fov(fovy_deg);
        }
    }

    void visualizer_window::enable_mirror_mode(bool enable)
    {
        for (auto& cam : _camera_list) {
            cam->enable_mirror_mode(enable);
        }
    }

    void visualizer_window::add_render_object(std::shared_ptr<geometry::geometry_object_base> object)
    {
        switch (object->get_type()) {
        case geometry::geometry_object_type::light_source:
            _light_source_renderer.add_object(std::static_pointer_cast<geometry::light_source_object>(object));
            break;
        case geometry::geometry_object_type::lineset:
            _lineset_renderer.add_object(std::static_pointer_cast<geometry::lineset_object>(object));
            break;
        case geometry::geometry_object_type::pointcloud:
            _pcd_renderer.add_object(std::static_pointer_cast<geometry::pcd_object>(object));
            break;
        case geometry::geometry_object_type::triangle_mesh:
            _mesh_renderer.add_object(std::static_pointer_cast<geometry::triangle_mesh_object>(object));
            break;
        case geometry::geometry_object_type::skeleton:
            _skeleton_renderer.add_object(std::static_pointer_cast<geometry::skeleton_object>(object));
            break;
        }
    }
    
    void visualizer_window::remove_render_object(std::shared_ptr<geometry::geometry_object_base> object)
    {
        switch (object->get_type()) {
        case geometry::geometry_object_type::light_source:
            _light_source_renderer.remove_object(std::static_pointer_cast<geometry::light_source_object>(object));
            break;
        case geometry::geometry_object_type::lineset:
            _lineset_renderer.remove_object(std::static_pointer_cast<geometry::lineset_object>(object));
            break;
        case geometry::geometry_object_type::pointcloud:
            _pcd_renderer.remove_object(std::static_pointer_cast<geometry::pcd_object>(object));
            break;
        case geometry::geometry_object_type::triangle_mesh:
            _mesh_renderer.remove_object(std::static_pointer_cast<geometry::triangle_mesh_object>(object));
            break;
        case geometry::geometry_object_type::skeleton:
            _skeleton_renderer.remove_object(std::static_pointer_cast<geometry::skeleton_object>(object));
            break;
        }
    }

    void visualizer_window::clear_render_objects()
    {
        _light_source_renderer.clear_objects();
        _lineset_renderer.clear_objects();
        _pcd_renderer.clear_objects();
        _mesh_renderer.clear_objects();
        _skeleton_renderer.clear_objects();
    }

    void visualizer_window::clear_render_objects(geometry::geometry_object_type type)
    {
        switch (type) {
        case geometry::geometry_object_type::light_source:
            _light_source_renderer.clear_objects();
            break;
        case geometry::geometry_object_type::lineset:
            _lineset_renderer.clear_objects();
            break;
        case geometry::geometry_object_type::pointcloud:
            _pcd_renderer.clear_objects();
            break;
        case geometry::geometry_object_type::triangle_mesh:
            _mesh_renderer.clear_objects();
            break;
        case geometry::geometry_object_type::skeleton:
            _skeleton_renderer.clear_objects();
            break;
        }
    }

    void visualizer_window::render()
    {
        _scene_window->bind_framebuffer();

        GLCall(::glClearColor(_render_config.bg_color.r(), _render_config.bg_color.g(), _render_config.bg_color.b(), 1.0f)); // set clear color
        GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // clear render buffers

        // Ref: https://learnopengl.com/Getting-started/Hello-Triangle
        GLCall(::glPolygonMode(GL_FRONT_AND_BACK, _render_config.show_wireframe ? GL_LINE : GL_FILL));

        const int32_t
            W = _scene_window->get_framebuffer_width(),
            H = _scene_window->get_framebuffer_height();

        // NOTE: Viewport placement is relative to the lower-left corner of the window content area.
        switch (_render_config.view_layout) {
        default:
        case view_layout_mode::one_view:
            this->_render_scene(_top_left_camera, view_port{ 0, 0, W, H });
            break;
        case view_layout_mode::two_views:
            this->_render_scene(_top_left_camera, view_port{ 0, 0, W / 2, H });
            this->_render_scene(_top_right_camera, view_port{ W / 2, 0, W / 2, H });
            break;
        case view_layout_mode::three_views:
            this->_render_scene(_top_left_camera, view_port{ 0, 0, W / 2, H });
            this->_render_scene(_top_right_camera, view_port{ W / 2, H / 2, W / 2, H / 2 });
            this->_render_scene(_bottom_right_camera, view_port{ W / 2, 0, W / 2, H / 2 });
            break;
        }

        _scene_window->unbind_framebuffer();

        GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 1.0f)); // set clear color
        GLCall(::glClear(GL_COLOR_BUFFER_BIT)); // clear render buffers

        // Render GUI
        _gui_mgr->render();
    }

    bool visualizer_window::poll_events()
    {
        ::glfwSwapBuffers(_glfw_window.get());
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
        return !static_cast<bool>(::glfwWindowShouldClose(_glfw_window.get()));
    }

    void visualizer_window::_render_scene(
        camera& target_camera, 
        const view_port viewport)
    {
        // Assign viewport to target camera.
        target_camera.set_view_port(viewport);

        // Change view port
        GLCall(::glViewport(viewport.x, viewport.y, viewport.width, viewport.height));

        // Update view/projection matrix
        mat4_f32 view, projection;
        target_camera.get_view_projection(view, projection);

        if (_render_config.light_opts.dir_light.follow_camera) {
            _curr_focused_camera->get_camera_direction(_render_config.light_opts.dir_light.direction);
        }

        _point_light_source_object->set_visible(_render_config.light_opts.point_light.enabled && _render_config.light_opts.point_light.show_light_source);
        _point_light_source_object->translate(_render_config.light_opts.point_light.position);
        _point_light_source_object->color = _render_config.light_opts.point_light.color;

        _lineset_renderer.render(view, projection, _render_config.light_opts);

        _origin_axis_frame_object->set_visible(_render_config.show_origin_axis);
        _origin_xz_plane_object->set_visible(_render_config.show_origin_xz_plane);

        _mesh_renderer.enable_object_normal_rendering(_render_config.show_object_normals);
        _mesh_renderer.render(view, projection, _render_config.light_opts);
        _light_source_renderer.render(view, projection, _render_config.light_opts);

        if (_render_config.pcd_point_size) {
            _pcd_renderer.set_pcd_point_size(*_render_config.pcd_point_size);
        }
        _pcd_renderer.render(view, projection, _render_config.light_opts);

        if (_render_config.skeleton_mode == skeleton_render_mode::skeleton_overlay ||
            _render_config.skeleton_mode == skeleton_render_mode::overlay_with_joint_axis)
        {
            GLCall(::glClear(GL_DEPTH_BUFFER_BIT)); // Enable skeleton overlay
        }

        _skeleton_renderer.show_joint_axis(_render_config.skeleton_mode == skeleton_render_mode::overlay_with_joint_axis);
        _skeleton_renderer.render(view, projection, _render_config.light_opts);
    }

    void visualizer_window::_handle_glfw_window_close_event(
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

    void visualizer_window::_handle_glfw_frame_buffer_resize_event(
        [[maybe_unused]] GLFWwindow* const window,
        [[maybe_unused]] const int width,
        [[maybe_unused]] const int height)
    {
        _curr_window_width = width;
        _curr_window_height = height;
    }

    void visualizer_window::_handle_glfw_key_event(
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
            for (auto cam : _camera_list) {
                cam->reset();
            }
            break;
        }
    }

    void visualizer_window::_handle_glfw_mouse_button_event(
        [[maybe_unused]] GLFWwindow* const window,
        [[maybe_unused]] const int button,
        [[maybe_unused]] const int action,
        [[maybe_unused]] const int mods)
    {
        const vec2_f32 curr_cursor_screen_pos = detail::get_cursor_device_screen_pos(window);
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

    void visualizer_window::_handle_glfw_mouse_move_event(
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
            const vec2_f32 curr_cursor_viewport_pos = 
                _scene_window->try_convert_screen_pos_2_viewport_pos(curr_cursor_screen_pos).value();

            _curr_focused_camera =
                [this, curr_cursor_viewport_pos]() -> camera*
                {
                    const float
                        W = static_cast<float>(_scene_window->get_framebuffer_width()),
                        H = static_cast<float>(_scene_window->get_framebuffer_height());

                    const bool
                        is_top_side = curr_cursor_viewport_pos.y() > H / 2,
                        is_left_side = curr_cursor_viewport_pos.x() < W / 2;

                    switch (_render_config.view_layout) {
                    case view_layout_mode::three_views:
                        if (is_left_side) {
                            return &_top_left_camera;
                        } else {
                            return (is_top_side) ? &_top_right_camera : &_bottom_right_camera;
                        }
                    case view_layout_mode::two_views:
                        return (is_left_side) ? &_top_left_camera : &_top_right_camera;
                    case view_layout_mode::one_view:
                    default:
                        return &_top_left_camera;
                    } // switch
                }();

            const bool
                flag_l_mouse_pressed = GLFW_PRESS == ::glfwGetMouseButton(_glfw_window.get(), GLFW_MOUSE_BUTTON_LEFT),
                flag_r_mouse_pressed = GLFW_PRESS == ::glfwGetMouseButton(_glfw_window.get(), GLFW_MOUSE_BUTTON_RIGHT),
                flag_m_mouse_pressed = GLFW_PRESS == ::glfwGetMouseButton(_glfw_window.get(), GLFW_MOUSE_BUTTON_MIDDLE);

            if (!flag_l_mouse_pressed && !flag_r_mouse_pressed && !flag_m_mouse_pressed) {
                return; // ignore
            }

            if (flag_l_mouse_pressed) {
                _curr_focused_camera->process_mouse_move_for_rotation(curr_cursor_viewport_pos - _last_clicked_cursor_viewport_pos);
            }
            else if (flag_m_mouse_pressed) {
                _curr_focused_camera->process_mouse_move_for_translation(_last_clicked_cursor_viewport_pos, curr_cursor_viewport_pos);
            }

            _last_clicked_cursor_viewport_pos = curr_cursor_viewport_pos;
        }
    }

    void visualizer_window::_handle_glfw_mouse_scroll_event(
        [[maybe_unused]] GLFWwindow* const window,
        [[maybe_unused]] const double scroll_xoffset,
        [[maybe_unused]] const double scroll_yoffset)
    {
        const vec2_f32 curr_cursor_screen_pos = detail::get_cursor_device_screen_pos(window);
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
            const bool ctrl_pressed = ::glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
            if (!ctrl_pressed) {
                _curr_focused_camera->process_mouse_scroll_for_zoom(static_cast<float>(scroll_yoffset));
            } else {
                _curr_focused_camera->process_mouse_scroll_for_perspective(static_cast<float>(scroll_yoffset));
            }
        }
    }

    void visualizer_window::_handle_glfw_content_scale_change_event(
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
