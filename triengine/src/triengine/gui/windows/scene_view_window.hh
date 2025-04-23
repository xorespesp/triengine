#pragma once
#include <triengine/gui/iwindow.hh>
#include <triengine/frame_buffer.hh>

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
    public:
        struct window_state_t {
            ImRect prev_content_region{};
            ImRect curr_content_region{};
            bool flag_show_overlay{ true };
            bool flag_window_focused{ false };
            bool flag_invalidate_fbo{ false };

            std::array<float, 100> values{};
            int32_t values_offset = 0;
            double refresh_time = 0.0;

            window_state_t() = default;
        };

    private:
        visualization::visualizer* _vis{ nullptr };
        frame_buffer _fb_main;
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

        const frame_buffer& get_framebuffer() const noexcept;

        void bind_framebuffer();

        void unbind_framebuffer();

        bool is_window_focused() const;

        bool test_cursor_hovered(vec2_f32 cursor_screen_pos) const;

        // Convert device screen coordinates to OpenGL viewport coordinates.
        // NOTE: Device screen coordinates are relative to the upper-left corner of the window content area.
        //       OpenGL viewport coordinates are relative to the lower-left corner of the window content area.
        // https://learnopengl.com/img/getting-started/coordinate_systems.png
        std::optional<vec2_f32> try_convert_screen_pos_2_viewport_pos(vec2_f32 screen_pos) const;

        void pre_render(ImGuiWindowFlags& window_flags) override;

        void post_render() override;

        void render(const window_render_context& render_ctx) override;

    private:
        void _render_overlay_ui(const window_render_context& render_ctx);

    }; // class

} // namespace