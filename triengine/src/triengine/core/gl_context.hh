#pragma once
#include <triengine/core/gpu_resource_manager.hh>
#include <triengine/utility/noncopyable.hh>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <string>

namespace triengine::core
{
    /**
     * The `gl_context` class is responsible for managing the lifecycle of the actual OpenGL context and the GLFW window,
     * as well as handling common initialization tasks such as GLFW/GLAD setup and debug callbacks.
     * For offscreen rendering, the window can be created hidden by setting `visible=false`.
     */
    class gl_context
        : utility::noncopyable
    {
    public:
        gl_context() = default;
        ~gl_context() { this->destroy(); }

        bool is_created() const noexcept {
            return _flag_initialized;
        }

        void create(
            const std::string& window_name,
            bool visible = true,
            int32_t width = -1,
            int32_t height = -1,
            bool fullscreen = false,
            bool enable_vsync = false
        );

        void destroy();

        GLFWwindow* get_glfw_window() const noexcept {
            return _glfw_window.get();
        }

        std::shared_ptr<gpu_resource_manager> get_gpu_resource_manager() noexcept {
            return _gpu_res_mgr;
        }

        std::shared_ptr<const gpu_resource_manager> get_gpu_resource_manager() const noexcept {
            return _gpu_res_mgr;
        }

    private:
        std::shared_ptr<GLFWwindow> _glfw_window;
        bool _flag_initialized{ false };
        std::shared_ptr<gpu_resource_manager> _gpu_res_mgr;
    };

} // namespace triengine