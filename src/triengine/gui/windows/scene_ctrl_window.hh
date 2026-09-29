#pragma once
#include <triengine/gui/iwindow.hh>
#include <triengine/core/frame_buffer.hh>

namespace triengine::gui
{
    class scene_control_window
        : public gui::iwindow
    {
    public:
        scene_control_window() = default;
        virtual ~scene_control_window() = default;

        const char* get_window_name() const override {
            return "Scene Render Control";
        }

        ImVec2 get_initial_window_size() const override {
            return ImVec2{ 450.0f, 620.0f };
        }

        void render(const window_render_context& render_ctx) override;

    }; // class

} // namespace