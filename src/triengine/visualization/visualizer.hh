#pragma once
#include <triengine/common.h>
#include <triengine/core/frame_buffer.hh>
#include <triengine/core/gl_context.hh>
#include <triengine/core/scene_renderer.hh>
#include <triengine/utility/noncopyable.hh>
#include <triengine/utility/frame_profiler.hh>
#include <triengine/utility/frame_limiter.hh>

#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace triengine::visualization
{
    // Interactive window that renders the current scene over the whole window area, without any GUI.
    // Unless documented otherwise, methods are thread-unsafe and must be called on the render thread.
    class visualizer
        : utility::noncopyable
    {
    public:
        // Stages of the frame profiler, registered in loop order. The rest of a frame is app time.
        struct profiling_stages {
            using descriptor = utility::frame_profiler::stage_descriptor;
            static constexpr descriptor kRender{ "Frame Render",
                "CPU wall time in render(), including scene and GUI. Not GPU execution time." };
            static constexpr descriptor kSwap{ "Swap",
                "Buffer swap. Includes waiting for the GPU and for V-Sync, so a GPU-bound frame shows up here." };
            static constexpr descriptor kLimiter{ "Limiter",
                "Waiting in the fps limiter to hold the configured cap." };
            static constexpr descriptor kEvents{ "Events",
                "Polling window and input events." };
        };

        static constexpr uint32_t kDefaultMaxFps = 120;

        using close_callback_type = std::function<void(bool& cancel)>;
        using dpi_change_callback_type = std::function<void(vec2_f32 dpi_scale)>;
        using key_callback_type = std::function<void(int32_t key, int32_t scancode, int32_t action, int32_t mods, bool& handled)>;
        using mouse_button_callback_type = std::function<void(int32_t button, int32_t action, int32_t mods, bool& handled)>;
        using mouse_move_callback_type = std::function<void(vec2_f64 cursor_pos, bool& handled)>;
        using mouse_scroll_callback_type = std::function<void(vec2_f64 scroll_offset, bool& handled)>;

    public:
        visualizer();
        virtual ~visualizer() = default;

        // `nullptr` until `create_window()` has initialized the context.
        const core::graphics_device_info* get_graphics_device_info() const noexcept;

        vec2_i32 get_window_size() const;

        // Scene render frame size in pixels. (the whole window framebuffer here; `visualizer_gui` excludes gui regions)
        // Valid only after the first render; `{ 0, 0 }` before that.
        vec2_i32 get_frame_size() const;

        vec2_f32 get_window_dpi_scale() const;

        void set_close_callback(close_callback_type cb);
        void set_dpi_change_callback(dpi_change_callback_type cb);
        void set_key_callback(key_callback_type cb);
        void set_mouse_button_callback(mouse_button_callback_type cb);
        void set_mouse_move_callback(mouse_move_callback_type cb);
        void set_mouse_scroll_callback(mouse_scroll_callback_type cb);

        // Whether the user can move the camera with the mouse and keyboard.
        // Programmatic camera control and the input callbacks above are not affected.
        bool is_camera_interaction_enabled() const noexcept { return _flag_camera_interaction; }
        void enable_camera_interaction(bool enable) noexcept { _flag_camera_interaction = enable; }

        void create_window(
            const std::string& window_name,
            bool show_window = true,
            int32_t window_width = -1,
            int32_t window_height = -1,
            bool fullscreen = false,
            bool enable_vsync = false,
            uint32_t max_fps = kDefaultMaxFps // `max_fps` caps the loop driven by `update_window()`. Pass 0 for uncapped rendering.
        );

        // Runtime controls: call after creation, on the owning render thread with this context current.
        // V-Sync and the fps limiter remain independent.
        uint32_t get_max_fps() const;
        void change_max_fps(uint32_t max_fps); // `max_fps == 0` disables the CPU limiter.
        bool is_vsync_enabled() const;
        void set_vsync_enabled(bool enabled);

        // NOTE: Render-thread access only; the profiler is thread-unsafe.
        const utility::frame_profiler& get_frame_profiler() const noexcept { return _frame_profiler; }
        utility::frame_profiler& get_frame_profiler() noexcept { return _frame_profiler; }

        void close_window();

        void destroy_window();

        void set_window_position(int32_t xpos, int32_t ypos);
        void set_window_visible(bool visible);

        std::shared_ptr<scene> add_scene();
        void remove_scene(scene_id_t scn_id);

        void switch_scene(scene_id_t scn_id);
        void switch_to_previous_scene();
        void switch_to_next_scene();

        std::shared_ptr<const scene> find_scene(scene_id_t scn_id) const;
        std::shared_ptr<scene> find_scene(scene_id_t scn_id);

        std::shared_ptr<const scene> get_current_scene() const;
        std::shared_ptr<scene> get_current_scene();

        void render();

        bool update_window();

    protected:
        // Framebuffer the current scene is rendered into.
        struct scene_render_target
        {
            GLuint fbo_id{ 0 };
            vec2_i32 frame_size{ 0, 0 };
        };

        // Hooks for subclasses that present the scene somewhere other than the whole window.
        // The defaults render into the default framebuffer and treat the whole window as the scene viewport.

        // Called at the end of `create_window()`, once the GL context and the scene renderer are ready.
        virtual void _on_window_created() {}

        // Called at the start of `destroy_window()`, while the GL context is still current.
        virtual void _on_window_destroying() {}

        // Called when the window moves to a monitor with a different content scale, before the user callback.
        virtual void _on_dpi_changed([[maybe_unused]] vec2_f32 dpi_scale) {}

        // Prepares and returns the render target for this frame.
        // A zero-sized `frame_size` skips the scene render. (e.g. minimized window)
        virtual scene_render_target _begin_scene_frame();

        // Presents the rendered scene. (called after the scene render, or after its skip)
        virtual void _end_scene_frame();

        // Whether keyboard input should drive the camera. (WASD, arrow keys)
        virtual bool _is_scene_focused() const;

        // Converts a window screen position (upper-left origin) to a scene viewport position (lower-left origin).
        // Returns `std::nullopt` when the position is outside the scene viewport.
        // (this is also what decides whether the cursor is over the scene at all)
        virtual std::optional<vec2_f32> _try_convert_screen_pos_2_viewport_pos(vec2_f32 screen_pos) const;

        // Input filters applied before the user callbacks. Return false to drop the event.
        // (e.g. when the event belongs to a gui widget)
        virtual bool _accepts_keyboard_input() const { return true; }
        virtual bool _accepts_mouse_input([[maybe_unused]] vec2_f32 cursor_screen_pos) const { return true; }

        // For subclasses that drive backend-specific layers on top of the scene. (e.g. the GUI renderer)
        core::gl_context& _get_gl_context() noexcept;

    private:
        // Whether the cursor is over the scene viewport and the scene may react to it.
        bool _is_cursor_in_scene_viewport(vec2_f32 cursor_screen_pos) const {
            // a position that converts to a viewport position is, by definition, over the scene viewport
            return this->_try_convert_screen_pos_2_viewport_pos(cursor_screen_pos).has_value();
        }

        void _handle_close_event(bool& cancel);
        void _handle_frame_resize_event(vec2_i32 new_frame_size);
        void _handle_key_event(int32_t key, int32_t scancode, int32_t action, int32_t mods);
        void _handle_mouse_button_event(int32_t button, int32_t action, int32_t mods);
        void _handle_mouse_move_event(vec2_f64 cursor_pos);
        void _handle_mouse_scroll_event(vec2_f64 scroll_offset);
        void _handle_dpi_change_event(vec2_f32 dpi_scale);

    private:
        bool _flag_initialized{ false };
        bool _flag_camera_interaction{ true };

        close_callback_type _cb_close;
        key_callback_type _cb_key;
        mouse_button_callback_type _cb_mouse_button;
        mouse_move_callback_type _cb_mouse_move;
        mouse_scroll_callback_type _cb_mouse_scroll;
        dpi_change_callback_type _cb_dpi_change;

        core::gl_context _glctx;
        core::scene_renderer _scn_renderer;

        // The scene is rendered into this offscreen buffer and then blitted to the window backbuffer;
        core::frame_buffer _scene_fb;

        std::list<std::shared_ptr<scene>> _scn_list;
        std::list<std::shared_ptr<scene>>::iterator _curr_scn_it{ _scn_list.end() };
        std::unordered_map<
            scene_id_t,
            std::list<std::shared_ptr<scene>>::iterator
        > _scn_id_map;

        vec2_i32 _frame_size{ 0, 0 }; // frame size of the last render

        utility::frame_limiter _frame_limiter;
        utility::frame_profiler _frame_profiler;
        // Registered in declaration order, which is the display order.
        const utility::frame_profiler::stage_id_t _render_stage{ _frame_profiler.register_stage(profiling_stages::kRender) };
        const utility::frame_profiler::stage_id_t _swap_stage{ _frame_profiler.register_stage(profiling_stages::kSwap) };
        const utility::frame_profiler::stage_id_t _limiter_stage{ _frame_profiler.register_stage(profiling_stages::kLimiter) };
        const utility::frame_profiler::stage_id_t _events_stage{ _frame_profiler.register_stage(profiling_stages::kEvents) };

        // frame time calculation
        double _frame_time_delta{ 0.0 }, _last_frame_time{ 0.0 };

        std::optional<vec2_f32> _begin_click_cursor_screen_pos;

    }; // class

} // namespace
