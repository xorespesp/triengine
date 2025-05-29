#pragma once
#include <triengine/core/gpu_resource_manager.hh>
#include <triengine/utility/noncopyable.hh>

#include <glad/gl.h>
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
        using close_callback = std::function<void(bool& handled)>;
        using key_callback = std::function<void(int key, int scancode, int action, int mods, bool& handled)>;
        using mouse_button_callback = std::function<void(int button, int action, int mods, bool& handled)>;
        using mouse_move_callback = std::function<void(double cursor_xpos, double cursor_ypos, bool& handled)>;
        using mouse_scroll_callback = std::function<void(double scroll_xoffset, double scroll_yoffset, bool& handled)>;
        using dpi_change_callback = std::function<void(float xscale, float yscale)>;

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

        GLFWwindow* get_glfw_window() const noexcept;

        vec2_i32 get_window_size() const;

        vec2_f32 get_window_dpi_scale() const;

        void make_context_current();

        void set_close_callback(close_callback cb) {
            _cb_close = std::move(cb);
        }

        void set_key_callback(key_callback cb) {
            _cb_key = std::move(cb);
        }

        void set_mouse_button_callback(mouse_button_callback cb) {
            _cb_mouse_button = std::move(cb);
        }

        void set_mouse_move_callback(mouse_move_callback cb) {
            _cb_mouse_move = std::move(cb);
        }

        void set_mouse_scroll_callback(mouse_scroll_callback cb) {
            _cb_mouse_scroll = std::move(cb);
        }

        void set_dpi_change_callback(dpi_change_callback cb) {
            _cb_dpi_change = std::move(cb);
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

        close_callback _cb_close;
        key_callback _cb_key;
        mouse_button_callback _cb_mouse_button;
        mouse_move_callback _cb_mouse_move;
        mouse_scroll_callback _cb_mouse_scroll;
        dpi_change_callback _cb_dpi_change;
    };

} // namespace triengine