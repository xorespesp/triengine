#include "gl_context.hh"
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>
#include <triengine/utility/singleton.hh>

#include <iostream>

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
        class global_glfw_environment final : public utility::singleton_trait<global_glfw_environment> {
        public:
            // Constructor: Initializes the GLFW library and sets up an error callback.
            // (Called automatically when the singleton instance is first accessed.)
            global_glfw_environment() {
                //std::cout << "glfwInit() START.." << std::endl;
                if (!::glfwInit()) {
                    std::cerr << "\nglfwInit() failed" << std::endl;
                    ::exit(EXIT_FAILURE);
                }

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
                //std::cout << "glfwTerminate() START.." << std::endl;
                ::glfwTerminate();
            }

            // Provides an explicit point to ensure GLFW initialization has occurred.
            // The actual initialization happens in the constructor when this singleton
            // is first instantiated (e.g., via the first call to `instance()` or this method).
            // This method is primarily syntactic-sugar for making the initialization step
            // explicit in the application's startup sequence. It performs no additional operations.
            void initialize() {}

        }; // class

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

    void gl_context::create(
        const std::string& window_name,
        const bool visible,
        const int32_t width,
        const int32_t height,
        const bool fullscreen,
        const bool enable_vsync)
    {
        if (_flag_initialized) {
            TRIENGINE_PANIC("gl_context already created");
        }

        // NOTE: Should be called in main thread
        global_glfw_environment::instance()->initialize();

        ::glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        ::glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

        // NOTE: https://stackoverflow.com/a/27762480
        ::glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
        ::glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Enable OpenGL forward-compatibility

#if defined (TRIENGINE_DEBUG_MODE)
        //::glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE); // Enable OpenGL debug context
#endif // ^^^ TRIENGINE_DEBUG_MODE ^^^

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

        ::glfwMakeContextCurrent(_glfw_window.get());
        if (!::gladLoadGLLoader(reinterpret_cast<GLADloadproc>(::glfwGetProcAddress))) {
            TRIENGINE_PANIC("Failed to load GL functions");
        }

        const GLint gl_context_flags{ []() { GLint flags{}; ::glGetIntegerv(GL_CONTEXT_FLAGS, &flags); return flags; }() };
        if (gl_context_flags & GL_CONTEXT_FLAG_DEBUG_BIT)
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

        // glfwSwapInterval(1) -> Enable vsync
        ::glfwSwapInterval((enable_vsync) ? 1 : 0);

        _gpu_res_mgr = std::make_shared<gpu_resource_manager>();

        _flag_initialized = true;
        TRIENGINE_TRACE(
            "gl_context created. window size=%dx%d, visible=%d, fullscreen=%d"
            , initial_widow_size.x()
            , initial_widow_size.y()
            , visible
            , fullscreen
        );
    }

    void gl_context::destroy()
    {
        if (_flag_initialized) {
            _glfw_window.reset();
            _flag_initialized = false;
            TRIENGINE_TRACE("gl_context destroyed.");
        }
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

    void gl_context::make_context_current()
    {
        ::glfwMakeContextCurrent(_glfw_window.get());
    }

} // namespace triengine