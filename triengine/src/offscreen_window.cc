#include "offscreen_window.hh"
#include "misc/string_utils.hh"
#include "misc/debug_utils.hh"
#include "misc/opengl_utils.hh"

#include <iostream>
#include <memory>

/**
 * Offscreen rendering in GLFW
 * https://github.com/glfw/glfw/blob/master/examples/offscreen.c
 */

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

    offscreen_window::offscreen_window()
    {
    }

    void offscreen_window::set_fovy(float fovy_deg)
    {
        for (auto& cam : _camera_list) {
            cam->set_fovy(fovy_deg);
        }
    }

    void offscreen_window::enable_mirror_mode(bool enable)
    {
        for (auto& cam : _camera_list) {
            cam->enable_mirror_mode(enable);
        }
    }

    void offscreen_window::create_window(
        const int32_t width,
        const int32_t height,
        const bool multisample)
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

        //::glfwWindowHint(GLFW_SAMPLES, 4); // Set framebuffer MSAA quality to 4x

        _curr_window_width = width;
        _curr_window_height = height;
        _fb_sample_count = (multisample) ? 4 : 1;

        ::glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        // Create window
        TRIENGINE_TRACE("Creating glfw window.. (size: %dx%d)", _curr_window_width, _curr_window_height);
        _glfw_window = std::shared_ptr<GLFWwindow>(
            ::glfwCreateWindow(
                _curr_window_width,
                _curr_window_height,
                "offscreen rendering",
                nullptr,
                nullptr
            ),
            ::glfwDestroyWindow
        );

        if (!_glfw_window) {
            ::glfwTerminate();
            TRIENGINE_PANIC("Failed to create GLFW window");
        }

        // In to use the member function as callback, set the current class as the Window User Pointer
        ::glfwSetWindowUserPointer(_glfw_window.get(), this);

        // Set all callbacks
        ::glfwSetWindowCloseCallback(_glfw_window.get(),
            +[](GLFWwindow* window) {
                auto pThis = static_cast<offscreen_window*>(::glfwGetWindowUserPointer(window));
                //pThis->_handle_glfw_window_close_event(window);
            });

        ::glfwSetFramebufferSizeCallback(_glfw_window.get(),
            +[](GLFWwindow* window, int w, int h) {
                auto pThis = static_cast<offscreen_window*>(::glfwGetWindowUserPointer(window));
                pThis->_curr_window_width = w;
                pThis->_curr_window_height = h;
            });

        ::glfwMakeContextCurrent(_glfw_window.get());
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

        ::glfwSwapInterval(1); // glfwSwapInterval(1) -> Enable vsync

        // Context Settings
        GLCall(::glEnable(GL_DEPTH_TEST));
        //GLCall(::glEnable(GL_MULTISAMPLE));
        GLCall(::glDisable(GL_BLEND));
        GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
        GLCall(::glClearDepth(1.0f));

        _light_source_renderer.create(_glfw_window.get());
        _mesh_renderer.create(_glfw_window.get());
        _lineset_renderer.create(_glfw_window.get());
        _pcd_renderer.create(_glfw_window.get());
        _skeleton_renderer.create(_glfw_window.get());
        _infgrid_renderer.create(_glfw_window.get());

        _default_scene = std::make_shared<scene>();
    }

    void offscreen_window::destroy_window()
    {
        if (_flag_initialized)
        {
            _flag_initialized = false;
            
            _skeleton_renderer.destroy();
            _pcd_renderer.destroy();
            _lineset_renderer.destroy();
            _mesh_renderer.destroy();
            _light_source_renderer.destroy();
            _infgrid_renderer.destroy();

            _glfw_window.reset();
        }
    }

    bool offscreen_window::update_window()
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

    void offscreen_window::render(
        image& frame_image)
    {
        this->_begin_frame();

        scene& scn = *_default_scene;

        const auto& bg_color = scn.scn_config.bg_color;
        GLCall(::glClearColor(bg_color.r(), bg_color.g(), bg_color.b(), bg_color.a())); // set clear color
        GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // clear render buffers

        // Ref: https://learnopengl.com/Getting-started/Hello-Triangle
        GLCall(::glPolygonMode(GL_FRONT_AND_BACK, scn.scn_config.show_wireframe ? GL_LINE : GL_FILL));

        const vec2_i32 scene_size{ _curr_window_width, _curr_window_height };
        const int32_t
            W = scene_size.x(),
            H = scene_size.y();

        // NOTE: Viewport placement is relative to the lower-left corner of the window content area.
        switch (scn.scn_config.view_layout) {
        default:
        case scene_config::view_layout_mode::one_view:
            this->_render_viewport(_top_left_camera, view_port{ 0, 0, W, H });
            break;
        case scene_config::view_layout_mode::two_views:
            this->_render_viewport(_top_left_camera, view_port{ 0, 0, W / 2, H });
            this->_render_viewport(_top_right_camera, view_port{ W / 2, 0, W / 2, H });
            break;
        case scene_config::view_layout_mode::three_views:
            this->_render_viewport(_top_left_camera, view_port{ 0, 0, W / 2, H });
            this->_render_viewport(_top_right_camera, view_port{ W / 2, H / 2, W / 2, H / 2 });
            this->_render_viewport(_bottom_right_camera, view_port{ W / 2, 0, W / 2, H / 2 });
            break;
        }

        //frame_image.prepare(W, H, image_format_type::bgr);
        //GLCall(::glReadPixels(
        //    0, 0,              /* GLint x, GLint y */
        //    W, H,              /* GLsizei width, GLsizei height */
        //    GL_BGR,            /* GLenum format */
        //    GL_UNSIGNED_BYTE,  /* GLenum type */
        //    frame_image.data() /* void* pixels */
        //));

        this->_end_frame();

        frame_image.prepare(W, H, image_format_type::bgra);

        const bool msaa_enabled = _fb_sample_count > 1;
        if (msaa_enabled)
        {
            _fb_main.blit_to(_fb_msaa_copy, true, false, false);
            GLCall(::glBindTexture(GL_TEXTURE_2D, _fb_msaa_copy.color_texture_id()));
            GLCall(::glGetTexImage(
                GL_TEXTURE_2D,     /* GLenum target */
                0,                 /* GLint level */
                GL_BGRA,            /* GLenum format */
                GL_UNSIGNED_BYTE,  /* GLenum type */
                frame_image.data() /* void* pixels */
            ));
            GLCall(::glBindTexture(GL_TEXTURE_2D, 0));
        }
        else
        {
            GLCall(::glBindTexture(GL_TEXTURE_2D, _fb_main.color_texture_id()));
            GLCall(::glGetTexImage(
                GL_TEXTURE_2D,     /* GLenum target */
                0,                 /* GLint level */
                GL_BGRA,            /* GLenum format */
                GL_UNSIGNED_BYTE,  /* GLenum type */
                frame_image.data() /* void* pixels */
            ));
            GLCall(::glBindTexture(GL_TEXTURE_2D, 0));
        }
    }

    void offscreen_window::_begin_frame()
    {
        if (_flag_invalidate_fbo)
        {
            // invalidate framebuffer

            const int32_t
                width_pixels = _curr_window_width,
                height_pixels = _curr_window_height;
            
            _fb_main.reserve(
                width_pixels,
                height_pixels,
                _fb_sample_count
            );

            _fb_msaa_copy.reserve(
                width_pixels,
                height_pixels,
                1
            );

            _flag_invalidate_fbo = false;
        }

        _fb_main.bind();
    }

    void offscreen_window::_end_frame()
    {
        _fb_main.unbind();
    }

    void offscreen_window::_render_viewport(
        camera& target_camera, 
        const view_port viewport)
    {
        scene& scn = *_default_scene;

        // Assign viewport to target camera.
        target_camera.set_view_port(viewport);

        // Change view port
        GLCall(::glViewport(viewport.x, viewport.y, viewport.width, viewport.height));

        if (scn.scn_config.light_opts.dir_light.follow_camera) {
            scn.scn_config.light_opts.dir_light.direction = _curr_focused_camera->get_camera_direction();
        }

        renderer::render_context render_ctx; {
            target_camera.get_view_projection(render_ctx.view, render_ctx.projection);
            render_ctx.light_opts = &scn.scn_config.light_opts;
            render_ctx.camera = &target_camera;
        }

        _lineset_renderer.render(render_ctx, scn.lineset_objects);

        _mesh_renderer.enable_object_normal_rendering(scn.scn_config.show_object_normals);
        _mesh_renderer.render(render_ctx, scn.mesh_objects);

        //_light_source_renderer.render(render_ctx);

        if (scn.scn_config.pcd_point_size) {
            _pcd_renderer.set_pcd_point_size(scn.scn_config.pcd_point_size.value());
        }
        _pcd_renderer.render(render_ctx, scn.pcd_objects);

        _infgrid_renderer.set_options(scn.scn_config.infgrid_opts);
        if (scn.scn_config.show_origin_xz_grid) {
            // NOTE: The infinite grid renderer must be rendered last to allow for alpha-blending.
            //       (except the skeleton renderer, which sometimes causes the depth buffer to be reset).
            _infgrid_renderer.render(render_ctx);
        }

        if (scn.scn_config.skeleton_mode == scene_config::skeleton_render_mode::skeleton_overlay ||
            scn.scn_config.skeleton_mode == scene_config::skeleton_render_mode::overlay_with_joint_axis)
        {
            GLCall(::glClear(GL_DEPTH_BUFFER_BIT)); // Enable skeleton overlay
        }
        _skeleton_renderer.show_joint_axis(scn.scn_config.skeleton_mode == scene_config::skeleton_render_mode::overlay_with_joint_axis);
        _skeleton_renderer.render(render_ctx, scn.skeleton_objects);

    }

} // namespace
