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
        using close_callback = std::function<void(visualizer& vis, bool& handled)>;
        using key_callback = std::function<void(visualizer& vis, int key, int scancode, int action, int mods, bool& handled)>;
        using mouse_button_callback = std::function<void(visualizer& vis, int button, int action, int mods, bool& handled)>;
        using mouse_move_callback = std::function<void(visualizer& vis, double cursor_xpos, double cursor_ypos, bool& handled)>;
        using mouse_scroll_callback = std::function<void(visualizer& vis, double scroll_xoffset, double scroll_yoffset, bool& handled)>;
        using dpi_change_callback = std::function<void(visualizer& vis, float xscale, float yscale)>;

    public:
        visualizer();
        virtual ~visualizer() = default;

        const core::gl_context* get_gl_context() const noexcept { return &_glctx; }
        core::gl_context* get_gl_context() noexcept { return &_glctx; }

        vec2_i32 get_window_size() const;
        vec2_f32 get_window_dpi_scale() const;

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

        void create_window(
            const std::string& window_name,
            bool show_window = true,
            int32_t window_width = -1,
            int32_t window_height = -1,
            bool fullscreen = false
        );

        void close_window();

        void destroy_window();

        void set_window_position(int xpos, int ypos);

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

        void add_gui_window(std::shared_ptr<gui::iwindow> window) {
            _gui_mgr->add_window(window);
        }

    private:
        void _handle_glfw_window_close_event(
            GLFWwindow* window
        );

        void _handle_glfw_frame_buffer_resize_event(
            GLFWwindow* window,
            int width,
            int height
        );

        void _handle_glfw_key_event(
            GLFWwindow* window,
            int key,
            int scancode,
            int action,
            int mods
        );

        void _handle_glfw_mouse_button_event(
            GLFWwindow* window,
            int button,
            int action,
            int mods
        );

        void _handle_glfw_mouse_move_event(
            GLFWwindow* window,
            double cursor_xpos,
            double cursor_ypos
        );

        void _handle_glfw_mouse_scroll_event(
            GLFWwindow* window,
            double scroll_xoffset,
            double scroll_yoffset
        );
        
        void _handle_glfw_content_scale_change_event(
            GLFWwindow* window,
            float xscale,
            float yscale
        );

    private:
        bool _flag_initialized{ false };

        close_callback _cb_close;
        key_callback _cb_key;
        mouse_button_callback _cb_mouse_button;
        mouse_move_callback _cb_mouse_move;
        mouse_scroll_callback _cb_mouse_scroll;
        dpi_change_callback _cb_dpi_change;
        
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

        vec2_f32 _last_clicked_cursor_viewport_pos{};

    }; // class

} // namespace