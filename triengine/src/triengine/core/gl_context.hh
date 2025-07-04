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
        using close_callback = std::function<void(bool& cancel)>;
        using frame_resize_callback = std::function<void(int32_t width, int32_t height)>;
        using dpi_change_callback = std::function<void(double dpi_xscale, double dpi_yscale)>;
        using key_callback = std::function<void(int32_t key, int32_t scancode, int32_t action, int32_t mods)>;
        using mouse_button_callback = std::function<void(int32_t button, int32_t action, int32_t mods)>;
        using mouse_move_callback = std::function<void(double cursor_xpos, double cursor_ypos)>;
        using mouse_scroll_callback = std::function<void(double scroll_xoffset, double scroll_yoffset)>;

    public:
        gl_context() = default;
        ~gl_context() { this->destroy(); }

        bool is_created() const noexcept;

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
        vec2_f32 get_cursor_screen_pos() const;

        bool get_window_close_flag() const;
        void set_window_close_flag(bool close);
        void set_window_position(int32_t xpos, int32_t ypos);

        void make_context_current();
        void swap_buffers();
        void poll_window_events();

        void set_close_callback(close_callback cb);
        void set_frame_resize_callback(frame_resize_callback cb);
        void set_dpi_change_callback(dpi_change_callback cb);
        void set_key_callback(key_callback cb);
        void set_mouse_button_callback(mouse_button_callback cb);
        void set_mouse_move_callback(mouse_move_callback cb);
        void set_mouse_scroll_callback(mouse_scroll_callback cb);

        std::shared_ptr<gpu_resource_manager> get_gpu_resource_manager() noexcept;
        std::shared_ptr<const gpu_resource_manager> get_gpu_resource_manager() const noexcept;

    private:
        std::shared_ptr<GLFWwindow> _glfw_window;
        bool _flag_initialized{ false };
        std::shared_ptr<gpu_resource_manager> _gpu_res_mgr;

        close_callback _cb_close;
        frame_resize_callback _cb_frame_resize;
        dpi_change_callback _cb_dpi_change;
        key_callback _cb_key;
        mouse_button_callback _cb_mouse_button;
        mouse_move_callback _cb_mouse_move;
        mouse_scroll_callback _cb_mouse_scroll;
    };

} // namespace triengine