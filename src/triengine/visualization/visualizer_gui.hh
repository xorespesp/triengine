#pragma once
#include <triengine/visualization/visualizer.hh>
#include <triengine/gui/iwindow.hh>
#include <triengine/gui/dock_slot.hh>

#include <memory>

// forward declaration
namespace triengine::gui {
    class gui_manager;
    class scene_view_window;
}

namespace triengine::visualization
{
    // `visualizer` with an ImGui layer
    class visualizer_gui
        : public visualizer
    {
    public:
        visualizer_gui();
        ~visualizer_gui() override;

        bool is_main_menu_enabled() const;
        void enable_main_menu(bool enable);

        void add_gui_window(
            std::shared_ptr<gui::iwindow> window,
            gui::dock_slot slot = gui::dock_slot::floating
        );

        // Override the dock area width/height ratios.
        // NOTE: Must be called before the first render() call;
        // afterwards the initial layout is already frozen.
        void set_dock_split_ratios(const gui::dock_split_ratios& ratios);

    protected:
        void _on_window_created() override;
        void _on_window_destroying() override;
        void _on_dpi_changed(vec2_f32 dpi_scale) override;

        scene_render_target _begin_scene_frame() override;
        void _end_scene_frame() override;

        bool _is_scene_focused() const override;
        std::optional<vec2_f32> _try_convert_screen_pos_2_viewport_pos(vec2_f32 screen_pos) const override;

        bool _accepts_keyboard_input() const override;
        bool _accepts_mouse_input(vec2_f32 cursor_screen_pos) const override;

    private:
        std::unique_ptr<gui::gui_manager> _gui_mgr;
        std::shared_ptr<gui::scene_view_window> _scene_window;

    }; // class

} // namespace
