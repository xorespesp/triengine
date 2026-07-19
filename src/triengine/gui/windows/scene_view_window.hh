#pragma once
#include <triengine/gui/iwindow.hh>
#include <triengine/core/frame_buffer.hh>
#include <triengine/utility/scrolling_buffer.hh>

#include <optional>
#include <array>

namespace triengine::visualization {
    class visualizer; // forward declaration
}

namespace triengine::gui
{
    class scene_view_window
        : public gui::iwindow
    {
    private:
        static constexpr float
            kFPSPlotUpdateFreq{ 60.0f }, // Unit: [Hz]
            kFPSPlotHistorySize{ 10.0f }; // Unit: [sec]

    private:
        struct window_state_t {
            ImRect prev_content_region{};
            ImRect curr_content_region{};
            bool flag_window_focused{ false };
            bool flag_invalidate_fbo{ false };

            // overlay options
            bool flag_show_overlay{ true };
            bool flag_show_overlay_debug_info{ false };
            int overlay_location{ 0 };
            utility::scrolling_buffer<vec2_f32> fps_plot_buffer{ static_cast<int32_t>(kFPSPlotUpdateFreq * kFPSPlotHistorySize) };
            std::optional<double> fps_plot_next_update_time;

            window_state_t() = default;
        };

    private:
        visualization::visualizer* _vis{ nullptr };
        core::frame_buffer _fb_main;
        window_state_t _state;

    public:
        scene_view_window(visualization::visualizer* vis);

        virtual ~scene_view_window() = default;

        const char* get_window_name() const override {
            return "Scene";
        }

        ImVec2 get_initial_window_size() const override {
            return ImVec2{ 800.0f, 600.0f };
        }

        const core::frame_buffer& get_framebuffer() const noexcept;

        void bind_framebuffer();

        void unbind_framebuffer();

        bool is_window_focused() const;

        // Check if the cursor is hovered over the scene viewport area.
        bool check_cursor_in_scene_viewport(vec2_f32 cursor_screen_pos) const;

        // Convert a win32 screen position to a gl viewport position.
        std::optional<vec2_f32> try_convert_screen_pos_2_viewport_pos(vec2_f32 screen_pos) const;

        void pre_render(ImGuiWindowFlags& window_flags) override;

        void post_render() override;

        void render(const window_render_context& render_ctx) override;

    private:
        void _render_overlay_ui(const window_render_context& render_ctx);

    }; // class

} // namespace