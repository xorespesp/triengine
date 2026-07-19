#pragma once
#include <triengine/common.h>
#include <triengine/gui/gui_manager.hh>
#include <triengine/core/gl_context.hh>
#include <triengine/core/scene_renderer.hh>
#include <triengine/utility/noncopyable.hh>

#include <functional>

namespace triengine::visualization
{
    class visualizer
        : utility::noncopyable
    {
    public:
        using close_callback_type = std::function<void(bool& cancel)>;
        using dpi_change_callback_type = std::function<void(vec2_f32 dpi_scale)>;
        using key_callback_type = std::function<void(int32_t key, int32_t scancode, int32_t action, int32_t mods, bool& handled)>;
        using mouse_button_callback_type = std::function<void(int32_t button, int32_t action, int32_t mods, bool& handled)>;
        using mouse_move_callback_type = std::function<void(vec2_f64 cursor_pos, bool& handled)>;
        using mouse_scroll_callback_type = std::function<void(vec2_f64 scroll_offset, bool& handled)>;

    public:
        visualizer();
        virtual ~visualizer() = default;

        const core::gl_context* get_gl_context() const noexcept;
        core::gl_context* get_gl_context() noexcept;

        vec2_i32 get_window_size() const;

        // Scene render frame size in pixels, excluding gui regions.
        // (use `get_window_size()` for the full window)
        // Valid only after the first render; `{ 0, 0 }` before that.
        vec2_i32 get_frame_size() const;

        vec2_f32 get_window_dpi_scale() const;

        void set_close_callback(close_callback_type cb);
        void set_dpi_change_callback(dpi_change_callback_type cb);
        void set_key_callback(key_callback_type cb);
        void set_mouse_button_callback(mouse_button_callback_type cb);
        void set_mouse_move_callback(mouse_move_callback_type cb);
        void set_mouse_scroll_callback(mouse_scroll_callback_type cb);

        void create_window(
            const std::string& window_name,
            bool show_window = true,
            int32_t window_width = -1,
            int32_t window_height = -1,
            bool fullscreen = false,
            bool enable_vsync = false
        );

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

        bool is_main_menu_enabled() const {
            return _gui_mgr->is_main_menu_enabled();
        }

        void enable_main_menu(bool enable) {
            _gui_mgr->enable_main_menu(enable);
        }

        void add_gui_window(
            std::shared_ptr<gui::iwindow> window,
            gui::dock_slot slot = gui::dock_slot::floating
        ) {
            _gui_mgr->add_window(std::move(window), slot);
        }

        // Override the dock area width/height ratios. 
        // NOTE: Must be called before the first render() call; 
        // afterwards the initial layout is already frozen.
        void set_dock_split_ratios(const gui::dock_split_ratios& ratios) {
            _gui_mgr->set_dock_split_ratios(ratios);
        }

    private:
        void _handle_close_event(bool& cancel);
        void _handle_frame_resize_event(vec2_i32 new_frame_size);
        void _handle_key_event(int32_t key, int32_t scancode, int32_t action, int32_t mods);
        void _handle_mouse_button_event(int32_t button, int32_t action, int32_t mods);
        void _handle_mouse_move_event(vec2_f64 cursor_pos);
        void _handle_mouse_scroll_event(vec2_f64 scroll_offset);
        void _handle_dpi_change_event(vec2_f32 dpi_scale);

    private:
        bool _flag_initialized{ false };

        close_callback_type _cb_close;
        key_callback_type _cb_key;
        mouse_button_callback_type _cb_mouse_button;
        mouse_move_callback_type _cb_mouse_move;
        mouse_scroll_callback_type _cb_mouse_scroll;
        dpi_change_callback_type _cb_dpi_change;
        
        core::gl_context _glctx;
        core::scene_renderer _scn_renderer;
        std::list<std::shared_ptr<scene>> _scn_list;
        std::list<std::shared_ptr<scene>>::iterator _curr_scn_it{ _scn_list.end() };
        std::unordered_map<
            scene_id_t, 
            std::list<std::shared_ptr<scene>>::iterator
        > _scn_id_map;

        std::unique_ptr<gui::gui_manager> _gui_mgr;
        std::shared_ptr<gui::scene_view_window> _scene_window;

        // frame time calculation
        double _frame_time_delta{ 0.0 }, _last_frame_time{ 0.0 };

        std::optional<vec2_f32> _begin_click_cursor_screen_pos;

    }; // class

} // namespace