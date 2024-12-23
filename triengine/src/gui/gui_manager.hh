#pragma once
#include "iwindow.hh"
#include "windows/log_window.hh"
#include "windows/render_stats_window.hh"
#include "windows/scene_view_window.hh"
#include "windows/scene_ctrl_window.hh"

#include <memory>
#include <string>
#include <vector>
#include <map>

 // forward declaration
namespace triengine {
    class visualizer_window;
}

namespace triengine::gui
{
    class gui_manager final
    {
    public:
        gui_manager();
        ~gui_manager();

        gui_manager(const gui_manager&) = delete;
        gui_manager& operator=(const gui_manager&) = delete;

        bool is_initialized() const noexcept;

        void initialize(
            visualizer_window* vis_window,
            float dpi_scale_factor = 1.0f
        );

        void deinitialize();

        void change_dpi_scale(
            float scale_factor
        );

        void enable_main_menu(bool enable) {
            _flag_show_main_menu = enable;
        }

        bool is_main_menu_enabled() const {
            return _flag_show_main_menu;
        }

        std::shared_ptr<gui::scene_view_window> get_scene_window() const {
            return _scene_window;
        }
        
        void add_window(std::shared_ptr<iwindow> new_window) {
            _windows.push_back(new_window);
        }

        void render();

    private:
        bool render_window(std::shared_ptr<iwindow> window);

        void setup_imgui_style();
        void setup_imgui_fonts(float scale_factor);
        void setup_dock_space(
            ImGuiID main_dockspace_id,
            ImGuiViewport* viewport
        );

    private:
        visualizer_window* _vis_window{ nullptr };
        std::map<std::string, ImFont*> _fonts_map;
        std::shared_ptr<gui::scene_view_window> _scene_window;
        std::shared_ptr<gui::scene_control_window> _scene_ctrl_window;
        std::vector<std::shared_ptr<iwindow>> _windows;
        bool _flag_initialized{ false };
        bool _flag_show_main_menu{ true };
        bool _flag_show_imgui_demo_window{ false };
        bool _flag_show_implot_demo_window{ false };
        float _dpi_scale_factor{ 1.0f };

    }; // class

} // namespace