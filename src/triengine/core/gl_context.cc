#include "gl_context.hh"

#include <triengine_generated/packed_shaders_data.h> // auto-generated header
#include <triengine/utility/logger.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>
#include <triengine/utility/singleton.hh>

#include <iostream>
#include <thread>
#include <mutex>

#if defined(TRIENGINE_FORCE_DISCRETE_GPU)
#  if defined(_WIN32) || defined(_WIN64)
extern "C" {
    // NVIDIA Optimus
    __declspec(dllexport) uint32_t NvOptimusEnablement = 0x00000001;
    // AMD PowerXpress
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#  endif // ^^^ _WIN32 || _WIN64 ^^^
#endif // ^^^ TRIENGINE_FORCE_DISCRETE_GPU ^^^

namespace triengine::core
{
    namespace
    {
        // Manages the global lifecycle of the GLFW library.
        // This class ensures that GLFW is initialized before its use (e.g., by a visualizer)
        // and terminated gracefully when the application exits.
        // It leverages the singleton pattern to guarantee a single point of initialization and termination.
        //
        // CRITICAL NOTE: GLFW functions, especially initialization and window creation,
        //                typically need to be called from the main thread. Therefore, the first
        //                access to this singleton (which triggers its constructor) must occur on the main thread.
        //
        // GLFW thread-safety constraints: https://www.glfw.org/docs/latest/intro_guide.html#thread_safety
        //
        // Main-thread-only GLFW calls used here: (main thread == the thread that called `glfwInit()`)
        //   - `glfwInit()` / `glfwTerminate()` (global GLFW library lifecycle)
        //   - `glfwCreateWindow()` / `glfwDestroyWindow()`
        //   - `glfwPollEvents()` / `glfwWaitEvents()`
        //   - `glfwGetWindowSize()`, `glfwSetWindowPos()`, `glfwGetWindowContentScale()`, `glfwShowWindow()`, ... (window state queries/mutations)
        //
        // GLFW calls that are safe from any thread (so they live in the context half):
        //   - `glfwMakeContextCurrent()` / `glfwGetCurrentContext()`
        //   - `glfwSwapBuffers()` / `glfwSwapInterval()`
        //   - `glfwGetProcAddress()` (used by glad to load function pointers)
        class global_glfw_environment final : public utility::singleton_trait<global_glfw_environment> {
        public:
            // Constructor: Initializes the GLFW library and sets up an error callback.
            // (Called automatically when the singleton instance is first accessed.)
            global_glfw_environment() {
                //std::cout << "glfwInit() start.." << std::endl;
                if (!::glfwInit()) {
                    std::cerr << "\nglfwInit() failed" << std::endl;
                    ::exit(EXIT_FAILURE);
                }

                // record current thread id so main-thread-only GLFW calls can be verified later.
                _glfw_init_thread_id = std::this_thread::get_id();

                ::glfwSetErrorCallback(
                    +[](const int err_code, const char* const err_desc) -> void {
                        const auto msg = utility::string::c_format(""
                            "GLFW Error(%d) : %s"
                            , err_code
                            , err_desc
                        );
                        std::cerr << '\n' << msg << std::endl;
                    });
            }

            // Destructor: Terminates the GLFW library.
            // (Called automatically when the singleton instance is destroyed; typically at program exit.)
            ~global_glfw_environment() {
                //std::cout << "glfwTerminate() start.." << std::endl;
                ::glfwTerminate();
            }

            // Provides an explicit point to ensure GLFW initialization has occurred.
            // The actual initialization happens in the constructor when this singleton
            // is first instantiated (e.g., via the first call to `instance()` or this method).
            // This method is primarily syntactic-sugar for making the initialization step
            // explicit in the application's startup sequence. It performs no additional operations.
            void initialize() {}

            // True when the queried thread is the GLFW main thread.
            // (main thread == the thread that called `glfwInit()`)
            // Used to guard GLFW calls that are only valid on that thread.
            bool is_glfw_init_thread(std::thread::id query_thread_id = std::this_thread::get_id()) const noexcept {
                return query_thread_id == _glfw_init_thread_id;
            }

        private:
            std::thread::id _glfw_init_thread_id;
        }; // class

        // Guards the one-time load of glad's GL function pointer table.
        //
        // glad is built in non-MX mode, so the loaded function pointers and feature
        // flags live in a single process-wide global table. Every context here is
        // created with identical window hints (same pixel format) on one GPU/driver,
        // so glfwGetProcAddress resolves identical addresses for all of them; loading
        // the table exactly once is therefore valid for every context. call_once both
        // removes the write race between render threads that each init their own
        // context and publishes the populated table with a proper happens-before edge
        // for later readers (threads that skip the load still observe a fully written table).
        //
        // TODO: revisit if multi-GPU / heterogeneous-driver offscreen rendering is needed.
        //       In that case a single global table is no longer correct (per-context
        //       function pointers and feature flags can differ), and glad should be
        //       regenerated in MX mode so each gl_context owns a GladGLContext dispatch
        //       table loaded via gladLoadGLContext(). That removes the shared table
        //       entirely but requires routing the context into every GL call site.
        std::once_flag g_glad_load_once;

    } // namespace

    // Basic Ref: https://learnopengl.com/In-Practice/Debugging
    static void APIENTRY _gl_debug_output_callback(
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

        const std::string msg = utility::string::c_format(""
            "\n------------------------------------------------------------"
            "\nGL Debug message (%lu) : %.*s"
            "\nGL Debug Source: %s"
            "\nGL Debug Type: %s"
            "\nGL Debug Severity: %s"
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

        TRIENGINE_TRACE(msg);
    }

    gl_context::~gl_context()
    {
        // The context half must be reset before this object is destroyed.
        // `reset_context()` releases the GL-side resource managers.
        // If it is skipped, those managers will be destroyed here with
        // no current context, which causes GL calls to be issued
        // against the wrong or non-existent context, leading to usage errors.
        TRIENGINE_ASSERT(!_context_initialized);

        if (_window_created) {
            this->destroy_window();
        }
    }

    void gl_context::create_window(
        const std::string& window_name,
        const bool visible,
        const int32_t width,
        const int32_t height,
        const bool fullscreen)
    {
        if (_window_created) {
            TRIENGINE_PANIC("gl_context window already created");
        }

        // Ensures GLFW is initialized (lazily, on first use). The very first call
        // establishes the GLFW main thread; later calls are verified against it.
        global_glfw_environment::instance()->initialize();

        // Window creation is main-thread-only; verify we are on the GLFW main thread.
        TRIENGINE_ASSERT(global_glfw_environment::instance()->is_glfw_init_thread());

        /**
         * The GLFW_CONTEXT_VERSION_MAJOR and GLFW_CONTEXT_VERSION_MINOR hints specify the
         * client API version that the created context must be compatible with.
         * For OpenGL, these hints are not hard constraints, as they don't have to match exactly, 
         * but glfwCreateWindow will still fail if the resulting OpenGL version is less than the one requested.
         * While there is no way to ask the driver for a context of the highest supported version, 
         * most drivers provide this when you ask GLFW for a version 1.0 context.
         * 
         * Refs:
         * http://www.glfw.org/docs/latest/window.html#window_hints
         * https://stackoverflow.com/a/27762480
         */
        ::glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        ::glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

        ::glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        ::glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Enable forward-compatibility

        // Enable OpenGL debug context
        constexpr int kEnableGLDebugContext =
#if defined (TRIENGINE_DEBUG_MODE)
            GL_TRUE;
#else  // ^^^ TRIENGINE_DEBUG_MODE ^^^ / vvv !TRIENGINE_DEBUG_MODE vvv
            GL_FALSE;
#endif // ^^^ TRIENGINE_DEBUG_MODE ^^^
        ::glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, kEnableGLDebugContext);

        TRIENGINE_TRACE("GL debug context: %s", (kEnableGLDebugContext) ? "enabled" : "disabled");

        //::glfwWindowHint(GLFW_SAMPLES, 4); // Set framebuffer MSAA quality to 4x

        GLFWmonitor* const monitor_info = ::glfwGetPrimaryMonitor();
        const GLFWvidmode* const display_info = ::glfwGetVideoMode(monitor_info);

        vec2_i32 initial_widow_size{ width, height };

        if (visible && fullscreen)
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

            initial_widow_size.x() = modes[best_mode_idx].width;
            initial_widow_size.y() = modes[best_mode_idx].height;
        }
        else if (visible && !fullscreen)
        {
            if (const bool auto_size_window = (width <= 0 || height <= 0);
                auto_size_window)
            {
                // Create invisible temporary window
                // for size measurement of non-client area
                ::glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
                std::shared_ptr<GLFWwindow> temp_window{
                    ::glfwCreateWindow(1, 1, "", nullptr, nullptr),
                    ::glfwDestroyWindow
                };

                if (!temp_window) {
                    TRIENGINE_PANIC("Failed to create temporary window for frame size measurement");
                }

                // get non-client area size
                int pad_left{}, pad_top{}, pad_right{}, pad_bottom{};
                ::glfwGetWindowFrameSize(
                    temp_window.get(),
                    &pad_left,
                    &pad_top,
                    &pad_right,
                    &pad_bottom
                );
                TRIENGINE_TRACE("non-client area size: %d,%d,%d,%d", pad_left, pad_top, pad_right, pad_bottom);

                constexpr float // Default window size ratio relative to the full display screen
                    kDefaultWindowWidthRatio = 0.95f,
                    kDefaultWindowHeightRatio = 0.9f;

                // client-area width, height
                initial_widow_size.x() = static_cast<int>(display_info->width * kDefaultWindowWidthRatio) - (pad_left + pad_right);
                initial_widow_size.y() = static_cast<int>(display_info->height * kDefaultWindowHeightRatio) - (pad_top + pad_bottom);
            }
        }

        ::glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
        _glfw_window.reset(
            ::glfwCreateWindow(
                initial_widow_size.x(),
                initial_widow_size.y(),
                window_name.c_str(),
                (visible && fullscreen) ? monitor_info : nullptr,
                nullptr
            ),
            ::glfwDestroyWindow
        );

        if (!_glfw_window) {
            TRIENGINE_PANIC("Failed to create GLFW window");
        }

        if (visible && !fullscreen)
        {
            // the upper-left corner of the window client position
            const vec2_i32 window_client_start_pos{
                (display_info->width - initial_widow_size.x()) / 2,
                (display_info->height - initial_widow_size.y()) / 2
            };

            // set window client start position (upper-left corner position)
            ::glfwSetWindowPos(_glfw_window.get(), window_client_start_pos.x(), window_client_start_pos.y());
        }

        ::glfwSetWindowUserPointer(_glfw_window.get(), this);

        //
        // Setup event callbacks
        //

        ::glfwSetWindowCloseCallback(_glfw_window.get(),
            +[](GLFWwindow* window) {
            auto pThis = static_cast<gl_context*>(::glfwGetWindowUserPointer(window));
            if (pThis->_cb_close) {
                bool canceled{ false };
                pThis->_cb_close(canceled);
                if (canceled) {
                    // cancel close requests (reset close flag)
                    ::glfwSetWindowShouldClose(window, GL_FALSE);
                }
            }
        });

        ::glfwSetFramebufferSizeCallback(_glfw_window.get(),
            +[](GLFWwindow* window, int w, int h) {
            auto pThis = static_cast<gl_context*>(::glfwGetWindowUserPointer(window));
            if (pThis->_cb_frame_resize) {
                pThis->_cb_frame_resize(vec2_i32{ w, h });
            }
        });

        ::glfwSetKeyCallback(_glfw_window.get(),
            +[](GLFWwindow* window, int key, int scancode, int action, int mods) {
            auto pThis = static_cast<gl_context*>(::glfwGetWindowUserPointer(window));
            if (pThis->_cb_key) {
                pThis->_cb_key(key, scancode, action, mods);
            }
        });

        ::glfwSetMouseButtonCallback(_glfw_window.get(),
            +[](GLFWwindow* window, int button, int action, int mods) {
            auto pThis = static_cast<gl_context*>(::glfwGetWindowUserPointer(window));
            if (pThis->_cb_mouse_button) {
                pThis->_cb_mouse_button(button, action, mods);
            }
        });

        ::glfwSetCursorPosCallback(_glfw_window.get(),
            +[](GLFWwindow* window, double xpos, double ypos) {
            auto pThis = static_cast<gl_context*>(::glfwGetWindowUserPointer(window));
            if (pThis->_cb_mouse_move) {
                pThis->_cb_mouse_move(vec2_f64{ xpos, ypos });
            }
        });

        ::glfwSetScrollCallback(_glfw_window.get(),
            +[](GLFWwindow* window, double xoffset, double yoffset) {
            auto pThis = static_cast<gl_context*>(::glfwGetWindowUserPointer(window));
            if (pThis->_cb_mouse_scroll) {
                pThis->_cb_mouse_scroll(vec2_f64{ xoffset, yoffset });
            }
        });

        ::glfwSetWindowContentScaleCallback(_glfw_window.get(),
            +[]([[maybe_unused]] GLFWwindow* window, float xscale, float yscale) {
            TRIENGINE_TRACE("dpi scale changed: [%f, %f]", xscale, yscale);
            auto pThis = static_cast<gl_context*>(::glfwGetWindowUserPointer(window));
            if (pThis->_cb_dpi_change) {
                pThis->_cb_dpi_change(vec2_f32{ xscale, yscale });
            }
        });

        _window_created = true;
        TRIENGINE_TRACE(
            "gl_context window created. window size=%dx%d, visible=%d, fullscreen=%d"
            , initial_widow_size.x()
            , initial_widow_size.y()
            , visible
            , fullscreen
        );
    }

    void gl_context::init_context(const bool enable_vsync)
    {
        if (!_window_created) {
            TRIENGINE_PANIC("gl_context::init_context() called before create_window()");
        }
        if (_context_initialized) {
            TRIENGINE_PANIC("gl_context context already initialized");
        }

        ::glfwMakeContextCurrent(_glfw_window.get());

        // Load glad's global GL function pointer table exactly once across all threads and contexts. 
        // (see `g_glad_load_once`)
        bool glad_load_ok = true; // start with true to avoid false negatives if the lambda is never called.
        std::call_once(g_glad_load_once, [&glad_load_ok]() {
            glad_load_ok = (::gladLoadGL(reinterpret_cast<GLADloadfunc>(::glfwGetProcAddress)) != 0);
        });
        if (!glad_load_ok) {
            TRIENGINE_PANIC("Failed to load GL functions");
        }

        constexpr int kEnableGLDebugContext =
#if defined (TRIENGINE_DEBUG_MODE)
            GL_TRUE;
#else  // ^^^ TRIENGINE_DEBUG_MODE ^^^ / vvv !TRIENGINE_DEBUG_MODE vvv
            GL_FALSE;
#endif // ^^^ TRIENGINE_DEBUG_MODE ^^^

        if constexpr (kEnableGLDebugContext)
        {
            // Initialize OpenGL debug output
            // Basic Ref: https://learnopengl.com/In-Practice/Debugging
            ::glEnable(GL_DEBUG_OUTPUT);
            ::glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            ::glDebugMessageCallback(_gl_debug_output_callback, this/* userParam */);
            ::glDebugMessageControl(
                /* GLenum source     */GL_DONT_CARE,
                /* GLenum type       */GL_DONT_CARE,
                /* GLenum severity   */GL_DONT_CARE,
                /* GLsizei count     */0,
                /* const GLuint* ids */nullptr,
                /* GLboolean enabled */GL_TRUE
            );
        }

        // Log GPU information
        {
            const GLubyte* version = ::glGetString(GL_VERSION);
            const GLubyte* vendor = ::glGetString(GL_VENDOR);
            const GLubyte* renderer = ::glGetString(GL_RENDERER);
            TRIENGINE_ASSERT(version && vendor && renderer);

            TRIENGINE_DEBUG("GL version: %s", version);
            TRIENGINE_DEBUG("GL vendor: %s", vendor);
            TRIENGINE_DEBUG("GL renderer: %s", renderer);
        }

        TRIENGINE_TRACE("V-Sync: %s", enable_vsync ? "enabled" : "disabled");
        ::glfwSwapInterval((enable_vsync) ? 1 : 0);

        constexpr char kDefaultGLSLShaderVersion[] = "#version 450 core";

        _shader_ldr = std::make_shared<shader_loader>();
        _shader_ldr->initialize(
            kTrienginePackedShaderData,
            kTrienginePackedShaderData_size,
            kDefaultGLSLShaderVersion
        );

        _gpu_res_mgr = std::make_shared<gpu_resource_manager>();

        _context_initialized = true;
        TRIENGINE_TRACE("gl_context context initialized.");
    }

    void gl_context::reset_context()
    {
        if (!_context_initialized) {
            return;
        }

        // Release the GL-side resources while the context is still current on this
        // thread (their destructors issue GL calls, e.g. buffer/fence deletion),
        // then unbind the context so it can be re-initialized or the window destroyed.
        _gpu_res_mgr.reset();
        _shader_ldr.reset();
        ::glfwMakeContextCurrent(nullptr);

        _context_initialized = false;
        TRIENGINE_TRACE("gl_context context reset.");
    }

    void gl_context::destroy_window()
    {
        if (!_window_created) {
            return;
        }

        // Destroying the GLFW window is main-thread-only; verify we are on the GLFW main thread.
        TRIENGINE_ASSERT(global_glfw_environment::instance()->is_glfw_init_thread());

        // Precondition: the context half must already be reset (see header NOTE).
        // Otherwise the GL resource managers outlive their context and are torn
        // down later without a current one.
        TRIENGINE_ASSERT(!_context_initialized);

        _glfw_window.reset();
        _window_created = false;
        TRIENGINE_TRACE("gl_context window destroyed.");
    }

    GLFWwindow* gl_context::get_glfw_window() const noexcept
    {
        return _glfw_window.get();
    }

    vec2_i32 gl_context::get_window_size() const
    {
        vec2_i32 window_size{};
        ::glfwGetWindowSize(_glfw_window.get(), &window_size.x(), &window_size.y());
        return window_size;
    }

    vec2_f32 gl_context::get_window_dpi_scale() const
    {
        vec2_f32 dpi_scale{};
        ::glfwGetWindowContentScale(_glfw_window.get(), &dpi_scale.x(), &dpi_scale.y());
        return dpi_scale;
    }

    // NOTE: Device screen coordinates are relative to the upper-left corner of the window content area.
    vec2_f32 gl_context::get_cursor_screen_pos() const
    {
        double xpos{}, ypos{};
        ::glfwGetCursorPos(_glfw_window.get(), &xpos, &ypos);
        return vec2_f32{ static_cast<float>(xpos), static_cast<float>(ypos) };
    }

    bool gl_context::get_window_close_flag() const
    {
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
        return static_cast<bool>(::glfwWindowShouldClose(_glfw_window.get()));
    }

    void gl_context::set_window_close_flag(bool close)
    {
        ::glfwSetWindowShouldClose(_glfw_window.get(), close);
    }

    void gl_context::set_window_position(int32_t xpos, int32_t ypos)
    {
        ::glfwSetWindowPos(_glfw_window.get(), xpos, ypos);
    }

    void gl_context::set_window_visible(bool visible)
    {
        if (visible) {
            ::glfwShowWindow(_glfw_window.get());
        } else {
            ::glfwHideWindow(_glfw_window.get());
        }
    }

    void gl_context::make_context_current()
    {
        ::glfwMakeContextCurrent(_glfw_window.get());
    }

    void gl_context::swap_buffers()
    {
        ::glfwSwapBuffers(_glfw_window.get());
    }

    void gl_context::poll_window_events()
    {
        ::glfwPollEvents();
    }

    int gl_context::get_key_state(int32_t glfw_key) const
    {
        const int state = ::glfwGetKey(_glfw_window.get(), glfw_key);
        return state;
    }

    void gl_context::set_close_callback(close_callback_type cb) {
        _cb_close = std::move(cb);
    }

    void gl_context::set_frame_resize_callback(frame_resize_callback_type cb) {
        _cb_frame_resize = std::move(cb);
    }

    void gl_context::set_dpi_change_callback(dpi_change_callback_type cb) {
        _cb_dpi_change = std::move(cb);
    }

    void gl_context::set_key_callback(key_callback_type cb) {
        _cb_key = std::move(cb);
    }

    void gl_context::set_mouse_button_callback(mouse_button_callback_type cb) {
        _cb_mouse_button = std::move(cb);
    }

    void gl_context::set_mouse_move_callback(mouse_move_callback_type cb) {
        _cb_mouse_move = std::move(cb);
    }

    void gl_context::set_mouse_scroll_callback(mouse_scroll_callback_type cb) {
        _cb_mouse_scroll = std::move(cb);
    }

    std::shared_ptr<shader_loader> gl_context::get_shader_loader() noexcept {
        return _shader_ldr;
    }

    std::shared_ptr<const shader_loader> gl_context::get_shader_loader() const noexcept {
        return _shader_ldr;
    }

    std::shared_ptr<gpu_resource_manager> gl_context::get_gpu_resource_manager() noexcept {
        return _gpu_res_mgr;
    }

    std::shared_ptr<const gpu_resource_manager> gl_context::get_gpu_resource_manager() const noexcept {
        return _gpu_res_mgr;
    }

} // namespace triengine