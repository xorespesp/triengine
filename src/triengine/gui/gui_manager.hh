#pragma once
#include <triengine/gui/iwindow.hh>
#include <triengine/gui/dock_slot.hh>
#include <triengine/gui/windows/log_window.hh>
#include <triengine/gui/windows/render_stats_window.hh>
#include <triengine/gui/windows/scene_view_window.hh>
#include <triengine/utility/noncopyable.hh>

#include <memory>
#include <string>
#include <vector>
#include <map>

namespace triengine::visualization {
    class visualizer; // forward declaration
}

namespace triengine::gui
{
    class gui_manager final
        : utility::noncopyable
    {
    public:
        gui_manager();
        ~gui_manager();

        bool is_initialized() const noexcept;

        void initialize(
            visualization::visualizer* vis,
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

        void add_window(
            std::shared_ptr<iwindow> new_window,
            dock_slot slot = dock_slot::floating
        ) {
            _windows.push_back({ std::move(new_window), slot });
        }

        // Override the default dock split ratios.
        // NOTE: Must be called before the first render()
        // (the initial dock layout is built on the first frame only). 
        // Out-of-range values are clamped to a safe sub-range of (0, 1).
        void set_dock_split_ratios(const dock_split_ratios& ratios);

        const dock_split_ratios& get_dock_split_ratios() const noexcept {
            return _dock_ratios;
        }

        void render();

    private:
        struct window_entry {
            std::shared_ptr<iwindow> window;
            dock_slot slot{};
        };

        bool render_window(std::shared_ptr<iwindow> window);

        void setup_imgui_style();
        void setup_imgui_fonts(float scale_factor);
        void setup_dock_space(
            ImGuiID main_dockspace_id,
            ImGuiViewport* viewport
        );

    private:
        visualization::visualizer* _vis{ nullptr };
        std::map<std::string, ImFont*> _fonts_map;
        std::shared_ptr<gui::scene_view_window> _scene_window;
        std::vector<window_entry> _windows;
        dock_split_ratios _dock_ratios{};
        bool _flag_initialized{ false };
        bool _flag_show_main_menu{ true };
        bool _flag_show_imgui_demo_window{ false };
        bool _flag_show_implot_demo_window{ false };
        float _dpi_scale_factor{ 1.0f };

    }; // class

} // namespace
