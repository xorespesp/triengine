#pragma once
#include "common.h"
#include "gl_context.hh"
#include "scene_renderer.hh"
#include "gui/gui_manager.hh"

#include <functional>

namespace triengine
{
    class visualizer_window
    {
    public:
        using close_callback = std::function<void(visualizer_window& vis, bool& handled)>;
        using key_callback = std::function<void(visualizer_window& vis, int key, int scancode, int action, int mods, bool& handled)>;
        using mouse_button_callback = std::function<void(visualizer_window& vis, int button, int action, int mods, bool& handled)>;
        using mouse_move_callback = std::function<void(visualizer_window& vis, double cursor_xpos, double cursor_ypos, bool& handled)>;
        using mouse_scroll_callback = std::function<void(visualizer_window& vis, double scroll_xoffset, double scroll_yoffset, bool& handled)>;
        using dpi_change_callback = std::function<void(visualizer_window& vis, float xscale, float yscale)>;

    public:
        visualizer_window();
        virtual ~visualizer_window() = default;

        visualizer_window(const visualizer_window&) = delete;
        visualizer_window& operator=(const visualizer_window&) = delete;

        const gl_context* get_gl_context() const noexcept { return &_glctx; }
        gl_context* get_gl_context() noexcept { return &_glctx; }

        float get_dpi_scale_x() const { return _curr_dpi_scale_x; }
        float get_dpi_scale_y() const { return _curr_dpi_scale_x; }

        int32_t get_window_width() const { return _curr_window_width; }
        int32_t get_window_height() const { return _curr_window_height; }

        std::shared_ptr<const scene> get_current_scene() const { return _curr_scn; }
        std::shared_ptr<scene> get_current_scene() { return _curr_scn; }

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
            int32_t width = -1,
            int32_t height = -1,
            bool fullscreen = false
        );

        void close_window();

        void destroy_window();

        void set_window_position(int xpos, int ypos);

        std::shared_ptr<scene> create_new_scene() {
            auto new_scn = std::make_shared<scene>();
            if (_scn_map.empty()) { _curr_scn = new_scn; }
            _scn_map.insert({ new_scn->id(), new_scn });
            return new_scn;
        }

        void remove_scene(std::shared_ptr<scene> scn) {
            if (scn) {
                auto it = _scn_map.find(scn->id());
                if (it != _scn_map.end()) {
                    _scn_map.erase(it);
                    if (_curr_scn->id() == scn->id()) {
                        _curr_scn = _scn_map.empty() ? nullptr : _scn_map.begin()->second;
                    }
                }
            }
        }

        void change_scene(std::shared_ptr<scene> scn) {
            _curr_scn = scn;
        }

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
        
        gl_context _glctx;
        int32_t _curr_window_width{};
        int32_t _curr_window_height{};
        float _curr_dpi_scale_x{ 1.0f };
        float _curr_dpi_scale_y{ 1.0f };
        vec2_f32 _last_clicked_cursor_viewport_pos{};

        scene_renderer _scn_renderer;
        std::unordered_map<uint32_t/* scene id */, std::shared_ptr<scene>> _scn_map;
        std::shared_ptr<scene> _curr_scn;

        std::unique_ptr<gui::gui_manager> _gui_mgr;
        std::shared_ptr<gui::scene_view_window> _scene_window;

        std::shared_ptr<geometry::light_source_object> _point_light_source_object;
        std::shared_ptr<geometry::triangle_mesh_object> _origin_axis_frame_object;

    }; // class
} // namespace