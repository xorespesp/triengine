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
     * gl_context 클래스는 실제 OpenGL 컨텍스트와 GLFW 윈도우 생명주기, GLFW/GLAD/디버그 콜백 등의 공통 초기화를 담당한다.
     * 오프스크린 렌더링 시에는 visible=false로 창을 숨긴 채 생성할 수 있다.
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