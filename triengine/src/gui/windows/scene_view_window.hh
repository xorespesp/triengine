#pragma once
#include "../iwindow.hh"
#include "../../frame_buffer.hh"

#include <GLFW/glfw3.h>

#include <optional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace triengine::gui
{
    class scene_view_window
        : public gui::iwindow
    {
    public:
        struct window_state_t {
            ImRect prev_content_region{};
            ImRect curr_content_region{};
            bool flag_window_size_changed{ false };
            bool flag_window_resizing{ false };
            bool flag_window_focused{ false };

            window_state_t() = default;
        };

    private:
        triengine::frame_buffer _framebuffer;
        window_state_t _state;

    public:
        scene_view_window(GLFWwindow* glfw_window) {
            int32_t window_width{}, window_height{};
            ::glfwGetWindowSize(glfw_window, &window_width, &window_height);
            _framebuffer.reserve(window_width, window_height);
        }

        virtual ~scene_view_window() = default;

        const char* get_window_name() const override {
            return "Scene";
        }

        ImVec2 get_initial_window_size() const override {
            return ImVec2{ 800.0f, 600.0f };
        }

        int32_t get_framebuffer_width() const {
            return _framebuffer.width_pixels();
        }

        int32_t get_framebuffer_height() const {
            return _framebuffer.height_pixels();
        }

        void bind_framebuffer() {
            _framebuffer.bind();
        }

        void unbind_framebuffer() {
            _framebuffer.unbind();
        }

        bool is_window_focused() const {
            return _state.flag_window_focused;
        }

        bool test_cursor_hovered(vec2_f32 cursor_screen_pos) const {
            return
                _state.flag_window_focused &&
                _state.curr_content_region.Contains(ImVec2{ cursor_screen_pos.x(), cursor_screen_pos.y() });
        }

        // Convert device screen coordinates to OpenGL viewport coordinates.
        // NOTE: Device screen coordinates are relative to the upper-left corner of the window content area.
        //       OpenGL viewport coordinates are relative to the lower-left corner of the window content area.
        // https://learnopengl.com/img/getting-started/coordinate_systems.png
        std::optional<vec2_f32> try_convert_screen_pos_2_viewport_pos(
            const vec2_f32 screen_pos) const
        {
            if (!this->test_cursor_hovered(screen_pos)) {
                return std::nullopt;
            }

            // global screen pos to local screen(scene window relative) pos
            vec2_f32 viewport_pos{
                screen_pos.x() - _state.curr_content_region.Min.x,
                screen_pos.y() - _state.curr_content_region.Min.y
            };

            // screen pos to viewport pos
            viewport_pos.y() = (static_cast<float>(_framebuffer.height_pixels()) - viewport_pos.y() - 1.0f);

            return viewport_pos;
        }

        void pre_render(
            [[maybe_unused]] ImGuiWindowFlags& window_flags) override
        {
            //window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        }
        
        void post_render() override
        {
            ImGui::PopStyleVar();
        }

        void render(
            [[maybe_unused]] const gui::window_placement_info placeInfo) override
        {
            _state.curr_content_region = []() -> ImRect {
                ImVec2 window_pos{ ImGui::GetWindowPos() };
                ImRect content_region{ ImGui::GetWindowContentRegionMin(), ImGui::GetWindowContentRegionMax() };
                content_region.Min.x += window_pos.x;
                content_region.Min.y += window_pos.y;
                content_region.Max.x += window_pos.x;
                content_region.Max.y += window_pos.y;
                return content_region;
            }();

            const ImVec2 
                prev_content_region_size = _state.prev_content_region.GetSize(),
                curr_content_region_size = _state.curr_content_region.GetSize();

            _state.flag_window_size_changed =
                curr_content_region_size.x != prev_content_region_size.x ||
                curr_content_region_size.y != prev_content_region_size.y;

            _state.flag_window_resizing =
                _state.flag_window_size_changed &&
                ImGui::IsMouseDragging(ImGuiMouseButton_Left);

            _state.flag_window_focused = 
                ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_DockHierarchy) &&
                ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_DockHierarchy);

            if (!_state.flag_window_resizing)
            {
                if (auto* const texture = _framebuffer.color_texture();
                    texture)
                {
                    ImGui::Image(
                        static_cast<ImTextureID>(static_cast<uint64_t>(texture->id())),
                        curr_content_region_size,
                        ImVec2(0, 1),
                        ImVec2(1, 0)
                    );
                }
                
                if (_state.flag_window_size_changed)
                {
                    _framebuffer.reserve(
                        static_cast<int32_t>(curr_content_region_size.x),
                        static_cast<int32_t>(curr_content_region_size.y)
                    );

                    _state.prev_content_region = _state.curr_content_region;
                }
            }

            {
                // Refs:
                // https://github.com/ocornut/imgui/issues/2486
                // https://github.com/ocornut/imgui/issues/423#issuecomment-161273431

                const ImVec2 local_pos = ImGui::GetWindowContentRegionMin();
                ImGui::SetCursorPos(ImVec2(local_pos.x + 10.0f, local_pos.y + 10.0f));

                ImGui::TextColored(ImVec4(255, 255, 0, 255), 
                    "framebuffer_size: [%d, %d]\n"
                    "curr_content_region_size: [%.2f, %.2f]\n"
                    "curr_content_region_min: [%.2f, %.2f]\n"
                    "curr_content_region_max: [%.2f, %.2f]\n"
                    "flag_window_size_changed: %d\n"
                    "flag_window_resizing: %d\n"
                    "flag_window_focused: %d\n"
                    , _framebuffer.width_pixels(), _framebuffer.height_pixels()
                    , curr_content_region_size.x, curr_content_region_size.y
                    , _state.curr_content_region.Min.x, _state.curr_content_region.Min.y
                    , _state.curr_content_region.Max.x, _state.curr_content_region.Max.y
                    , _state.flag_window_size_changed
                    , _state.flag_window_resizing
                    , _state.flag_window_focused
                );

                ImDrawList* const draw_list = ImGui::GetForegroundDrawList();
                draw_list->AddRect(
                    _state.curr_content_region.Min,
                    _state.curr_content_region.Max,
                    IM_COL32(255, 255, 0, 255)
                );
            }

        }

    }; // class

} // namespace