#pragma once
#include <triengine/visualization/visualizer.hh>
#include <triengine/gui/gui_manager.hh>

#include <memory>

namespace triengine::visualization
{
    // `visualizer` with an ImGui layer
    class visualizer_gui
        : public visualizer
    {
    public:
        visualizer_gui() = default;
        ~visualizer_gui() override = default;

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

    protected:
        void _on_window_created() override;
        void _on_window_destroying() override;

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
