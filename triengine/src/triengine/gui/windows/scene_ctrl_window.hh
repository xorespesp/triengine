#pragma once
#include <triengine/gui/iwindow.hh>
#include <triengine/frame_buffer.hh>

namespace triengine::visualization {
    class visualizer; // forward declaration
}

namespace triengine::gui
{
    class scene_control_window
        : public gui::iwindow
    {
    private:
        visualization::visualizer* _vis{ nullptr };

    public:
        scene_control_window(visualization::visualizer* vis);
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