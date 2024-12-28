#pragma once
#include "../iwindow.hh"
#include "../../frame_buffer.hh"

namespace triengine {
    // forward declaration
    class visualizer_window;
}

namespace triengine::gui
{
    class scene_control_window
        : public gui::iwindow
    {
    private:
        visualizer_window* _vis_window;

    public:
        scene_control_window(visualizer_window* vis_window);
        virtual ~scene_control_window() = default;

        const char* get_window_name() const override {
            return "Scene Control";
        }

        ImVec2 get_initial_window_size() const override {
            return ImVec2{ 450.0f, 620.0f };
        }

        void render(const window_render_context& render_ctx) override;

    }; // class

} // namespace