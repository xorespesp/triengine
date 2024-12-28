#pragma once
#include "common.h"
#include "camera.hh"
#include "renderer/light_source_renderer.hh"
#include "renderer/triangle_mesh_renderer.hh"
#include "renderer/lineset_renderer.hh"
#include "renderer/pcd_renderer.hh"
#include "renderer/skeleton_renderer.hh"
#include "lighting_options.hh"
#include "gui/gui_manager.hh"

#include "extern/glad/glad.h"
#include <GLFW/glfw3.h>

#include <functional>
#include <array>

namespace triengine
{
    class visualizer_window
    {
    public:
        enum class view_layout_mode {
            one_view = 0,
            two_views,
            three_views,
        };

        enum class skeleton_render_mode {
            skeleton_default = 0,
            skeleton_overlay,
            overlay_with_joint_axis,
        };

        struct render_config {
            view_layout_mode view_layout{ view_layout_mode::one_view };
            bool show_origin_axis{ true };
            bool show_origin_xz_plane{ false };
            bool show_object_normals{ false };
            bool show_wireframe{ false };
            lighting_options light_opts;
            color3_f32 bg_color{ 0.05f, 0.05f, 0.05f };
            std::optional<float> pcd_point_size;
            skeleton_render_mode skeleton_mode{ skeleton_render_mode::skeleton_overlay };
        };

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

        GLFWwindow* get_glfw_window() const {
            return _glfw_window.get();
        }

        const camera* get_current_camera() const {
            return _curr_focused_camera;
        }

        float get_dpi_scale_x() const {
            return _curr_dpi_scale_x;
        }

        float get_dpi_scale_y() const {
            return _curr_dpi_scale_x;
        }

        int get_window_width() const {
            return _curr_window_width;
        }

        int get_window_height() const {
            return _curr_window_height;
        }

        const render_config& get_render_config() const {
            return _render_config;
        }

        render_config& get_render_config() {
            return _render_config;
        }

        void create_window(
            const std::string& window_name,
            bool show_window = true,
            int width = -1,
            int height = -1,
            bool fullscreen = false
        );

        void close_window();

        void destroy_window();

        void set_window_position(int xpos, int ypos);

        void set_fovy(float fovy_deg);

        void enable_mirror_mode(bool enable);

        void add_render_object(std::shared_ptr<geometry::geometry_object_base> object);

        void remove_render_object(std::shared_ptr<geometry::geometry_object_base> object);

        void clear_render_objects();

        void clear_render_objects(geometry::geometry_object_type type);

        void render();

        bool poll_events();

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
        void _render_scene(
            camera& target_camera,
            view_port viewport
        );
        
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

        render_config _render_config;

        std::shared_ptr<GLFWwindow> _glfw_window;

        camera _top_left_camera;
        camera _top_right_camera;
        camera _bottom_right_camera;
        const std::array<camera*, 3> _camera_list = {
            &_top_left_camera, &_top_right_camera, &_bottom_right_camera
        };

        int32_t _curr_window_width{};
        int32_t _curr_window_height{};
        float _curr_dpi_scale_x{ 1.0f };
        float _curr_dpi_scale_y{ 1.0f };
        camera* _curr_focused_camera{ &_top_left_camera };
        vec2_f32 _last_clicked_cursor_viewport_pos{};

        renderer::light_source_renderer _light_source_renderer;
        renderer::triangle_mesh_renderer _mesh_renderer;
        renderer::lineset_renderer _lineset_renderer;
        renderer::pcd_renderer _pcd_renderer;
        renderer::skeleton_renderer _skeleton_renderer;

        std::unique_ptr<gui::gui_manager> _gui_mgr;
        std::shared_ptr<gui::scene_view_window> _scene_window;

        std::shared_ptr<geometry::light_source_object> _point_light_source_object;
        std::shared_ptr<geometry::triangle_mesh_object> _origin_axis_frame_object;
        std::shared_ptr<geometry::lineset_object> _origin_xz_plane_object;

    }; // class
} // namespace