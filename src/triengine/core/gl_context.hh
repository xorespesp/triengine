#pragma once
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <string>

#include <triengine/core/shader.hh>
#include <triengine/core/shader_loader.hh>
#include <triengine/core/gpu_resource_manager.hh>
#include <triengine/utility/noncopyable.hh>

namespace triengine::core
{
    /**
     * The `gl_context` class manages the lifecycle of an OpenGL context and its GLFW window,
     * and handles common initialization tasks such as GLFW/GLAD setup and debug callbacks.
     *
     * The lifecycle is split into a window half and a context half, so that an offscreen
     * context can be created on the GLFW main thread but owned and used entirely by a worker render thread:
     *   - window half  (GLFW main thread): `create_window()` / `destroy_window()`
     *   - context half (render thread)   : `init_context()` / `reset_context()`
     * 
     * NOTE: the "GLFW main thread" is the thread that called `glfwInit()`,
     *       which is distinct from the render thread that owns the GL context.
     * 
     * The OpenGL context is created together with the window by `create_window()`, 
     * but it is only made current and populated with GL resources by `init_context()`. 
     * See the per-method NOTE comments below for the exact thread and ordering constraints.
     */
    class gl_context
        : utility::noncopyable
    {
    public:
        using close_callback_type = std::function<void(bool& cancel)>;
        using frame_resize_callback_type = std::function<void(vec2_i32 new_frame_size)>;
        using dpi_change_callback_type = std::function<void(vec2_f32 dpi_scale)>;
        using key_callback_type = std::function<void(int32_t key, int32_t scancode, int32_t action, int32_t mods)>;
        using mouse_button_callback_type = std::function<void(int32_t button, int32_t action, int32_t mods)>;
        using mouse_move_callback_type = std::function<void(vec2_f64 cursor_pos)>;
        using mouse_scroll_callback_type = std::function<void(vec2_f64 scroll_offset)>;

    public:
        gl_context() = default;
        ~gl_context();

        // Creates the GLFW window (and its OpenGL context).
        // Does not make the context current and issues no GL calls. (call `init_context()` for that)
        // Pass `visible=false` for offscreen rendering.
        //
        // NOTE: MUST be called on the GLFW main thread.
        // (the thread that called `glfwInit()`; GLFW requires window creation there)
        void create_window(
            const std::string& window_name,
            bool visible = true,
            int32_t width = -1,
            int32_t height = -1,
            bool fullscreen = false
        );

        // Makes the GL context current on the calling thread and initializes the
        // GL-side state (function loader, shaders, GPU resources).
        //
        // NOTE: MUST be called on the render thread that will own this context.
        //       (the single thread used for all GL/scene/render work) 
        //       The context stays current on this thread until `reset_context()`.
        // Precondition: `create_window()` has run.
        void init_context(bool enable_vsync = false);

        // Releases the GL-side resources and unbinds the context from this thread,
        // leaving it re-initializable via `init_context()` afterwards.
        //
        // NOTE: MUST be called on the same render thread that called `init_context()`,
        // while the context is still current.
        void reset_context();

        // Destroys the GLFW window (and its OpenGL context).
        //
        // NOTE: MUST be called on the GLFW main thread.
        // Precondition:
        //   - the context must not be current on any thread.
        //   - call `reset_context()` (and join the render thread) first.
        //     Skipping `reset_context()` is not fatal but disorderly: destroying the
        //     window here already frees the context's GL objects, yet the GL resource
        //     managers stay alive until this object is destroyed, and their destructors
        //     then issue GL calls with no current context (no-ops, possibly fence-wait
        //     warnings).
        void destroy_window();

        GLFWwindow* get_glfw_window() const noexcept;
        vec2_i32 get_window_size() const;
        vec2_i32 get_framebuffer_size() const; // in pixels (may differ from the window size on scaled displays)
        vec2_f32 get_window_dpi_scale() const;
        vec2_f32 get_cursor_screen_pos() const;

        bool is_window_focused() const;
        bool get_window_close_flag() const;
        void set_window_close_flag(bool close);
        void set_window_position(int32_t xpos, int32_t ypos);
        void set_window_visible(bool visible);

        void make_context_current();
        void swap_buffers();
        void poll_window_events();

        int get_key_state(int32_t glfw_key) const;

        void set_close_callback(close_callback_type cb);
        void set_frame_resize_callback(frame_resize_callback_type cb);
        void set_dpi_change_callback(dpi_change_callback_type cb);
        void set_key_callback(key_callback_type cb);
        void set_mouse_button_callback(mouse_button_callback_type cb);
        void set_mouse_move_callback(mouse_move_callback_type cb);
        void set_mouse_scroll_callback(mouse_scroll_callback_type cb);

        std::shared_ptr<shader_loader> get_shader_loader() noexcept;
        std::shared_ptr<const shader_loader> get_shader_loader() const noexcept;

        std::shared_ptr<gpu_resource_manager> get_gpu_resource_manager() noexcept;
        std::shared_ptr<const gpu_resource_manager> get_gpu_resource_manager() const noexcept;

    private:
        bool _window_created{ false };
        bool _context_initialized{ false };

        std::shared_ptr<GLFWwindow> _glfw_window;
        std::shared_ptr<shader_loader> _shader_ldr;
        std::shared_ptr<gpu_resource_manager> _gpu_res_mgr;

        close_callback_type _cb_close;
        frame_resize_callback_type _cb_frame_resize;
        dpi_change_callback_type _cb_dpi_change;
        key_callback_type _cb_key;
        mouse_button_callback_type _cb_mouse_button;
        mouse_move_callback_type _cb_mouse_move;
        mouse_scroll_callback_type _cb_mouse_scroll;
    };

} // namespace triengine