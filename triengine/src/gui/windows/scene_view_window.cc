#include "scene_view_window.hh"
#include "../../visualizer_window.hh"
#include "../../misc/string_utils.hh"

namespace triengine::gui
{
    scene_view_window::scene_view_window(
        visualizer_window* const vis_window)
        : _vis_window{ vis_window }
    {
        _state.curr_content_region.Max.x = static_cast<float>(vis_window->get_window_width());
        _state.curr_content_region.Max.y = static_cast<float>(vis_window->get_window_height());
        _state.flag_invalidate_fbo = true;
    }

    bool scene_view_window::is_framebuffer_valid() const
    {
        return _fb_main.is_valid();
    }

    vec2_i32 scene_view_window::get_framebuffer_size() const
    {
        return vec2_i32{ _fb_main.width_pixels(), _fb_main.height_pixels() };
    }

    void scene_view_window::bind_framebuffer()
    {
        if (_state.flag_invalidate_fbo)
        {
            // invalidate framebuffer

            const ImVec2
                curr_content_region_size = _state.curr_content_region.GetSize();

            _fb_main.reserve(
                static_cast<int32_t>(curr_content_region_size.x),
                static_cast<int32_t>(curr_content_region_size.y),
                _state.fb_sample_count
            );

            _fb_msaa_copy.reserve(
                static_cast<int32_t>(curr_content_region_size.x),
                static_cast<int32_t>(curr_content_region_size.y),
                1
            );

            _state.flag_invalidate_fbo = false;
        }

        _fb_main.bind();
    }

    void scene_view_window::unbind_framebuffer()
    {
        _fb_main.unbind();
    }

    bool scene_view_window::is_window_focused() const
    {
        return _state.flag_window_focused;
    }

    bool scene_view_window::test_cursor_hovered(vec2_f32 cursor_screen_pos) const {
        return
            _state.flag_window_focused &&
            _state.curr_content_region.Contains(ImVec2{ cursor_screen_pos.x(), cursor_screen_pos.y() });
    }

    std::optional<vec2_f32> scene_view_window::try_convert_screen_pos_2_viewport_pos(
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
        viewport_pos.y() = (static_cast<float>(_fb_main.height_pixels()) - viewport_pos.y() - 1.0f);

        return viewport_pos;
    }

    void scene_view_window::pre_render(
        [[maybe_unused]] ImGuiWindowFlags& window_flags)
    {
        //window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    }

    void scene_view_window::post_render()
    {
        ImGui::PopStyleVar();
    }

    void scene_view_window::render(
        [[maybe_unused]] const window_render_context& render_ctx)
    {
        _state.curr_content_region = 
            []() -> ImRect {
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

        const bool flag_window_size_changed =
            curr_content_region_size.x != prev_content_region_size.x ||
            curr_content_region_size.y != prev_content_region_size.y;

        const bool flag_window_resizing =
            flag_window_size_changed &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left);

        _state.flag_window_focused =
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_DockHierarchy) &&
            ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_DockHierarchy);

        if (!flag_window_resizing)
        {
            const bool msaa_enabled = _state.fb_sample_count > 1;
            if (msaa_enabled)
            {
                _fb_main.blit_to(_fb_msaa_copy, true, false, false);
                ImGui::Image(
                    static_cast<ImTextureID>(static_cast<uint64_t>(_fb_msaa_copy.color_texture_id())),
                    curr_content_region_size,
                    ImVec2(0, 1),
                    ImVec2(1, 0)
                );
            }
            else
            {
                ImGui::Image(
                    static_cast<ImTextureID>(static_cast<uint64_t>(_fb_main.color_texture_id())),
                    curr_content_region_size,
                    ImVec2(0, 1),
                    ImVec2(1, 0)
                );
            }

            if (flag_window_size_changed)
            {
                _state.flag_invalidate_fbo = true;
                _state.prev_content_region = _state.curr_content_region;
            }

            this->_render_overlay_ui(render_ctx);
        }
        else
        {
            // Refs:
            // https://github.com/ocornut/imgui/issues/2486
            // https://github.com/ocornut/imgui/issues/423#issuecomment-161273431

            const ImVec2 local_pos = ImGui::GetWindowContentRegionMin();
            ImGui::SetCursorPos(ImVec2(local_pos.x + 10.0f, local_pos.y + 10.0f));

            ImGui::TextColored(ImVec4(255, 255, 0, 255),
                "curr_content_region_size: [%.2f, %.2f]\n"
                "curr_content_region_min: [%.2f, %.2f]\n"
                "curr_content_region_max: [%.2f, %.2f]\n"
                "flag_window_size_changed: %d\n"
                "flag_window_resizing: %d\n"
                "flag_window_focused: %d\n"
                , curr_content_region_size.x, curr_content_region_size.y
                , _state.curr_content_region.Min.x, _state.curr_content_region.Min.y
                , _state.curr_content_region.Max.x, _state.curr_content_region.Max.y
                , flag_window_size_changed
                , flag_window_resizing
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

    // Ref: `ShowExampleAppSimpleOverlay(bool* p_open)`
    void scene_view_window::_render_overlay_ui(const window_render_context& render_ctx)
    {
        thread_local misc::string::format_string_builder<1024> sb_;

        static int location = 0;

        ImGuiWindowFlags window_flags = 
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | 
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

        if (location >= 0)
        {
            constexpr float kPadSize = 15.0f;
            ImVec2 work_pos = _state.curr_content_region.Min;
            ImVec2 work_size = _state.curr_content_region.GetSize();
            ImVec2 window_pos, window_pos_pivot;
            window_pos.x = (location & 1) ? (work_pos.x + work_size.x - kPadSize) : (work_pos.x + kPadSize);
            window_pos.y = (location & 2) ? (work_pos.y + work_size.y - kPadSize) : (work_pos.y + kPadSize);
            window_pos_pivot.x = (location & 1) ? 1.0f : 0.0f;
            window_pos_pivot.y = (location & 2) ? 1.0f : 0.0f;
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
            ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID); // for multi-viewport environment
            window_flags |= ImGuiWindowFlags_NoMove;
        }
        else if (location == -2)
        {
            // Center window
            ImGui::SetNextWindowPos(_state.curr_content_region.GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            window_flags |= ImGuiWindowFlags_NoMove;
        }

        ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 5, 5 });
        if (ImGui::Begin("##SceneWindowOverlay", &_state.flag_show_overlay, window_flags))
        {
            bool msaa_enabled_state = _state.fb_sample_count > 1;

            const auto scene_mouse_pos = 
                [this]() -> std::optional<vec2_f32> {
                    if (ImGui::IsMousePosValid()) {
                        const auto& mouse_pos = ImGui::GetIO().MousePos;
                        return this->try_convert_screen_pos_2_viewport_pos(vec2_f32{ mouse_pos.x, mouse_pos.y });
                    }
                    return std::nullopt;
                }();

            const camera* const curr_camera = _vis_window->get_current_scene()->get_camera();
            const camera_parameters* const curr_camera_params = &curr_camera->get_parameters();
            const vec3_f32 eye_pos = curr_camera->get_camera_position();
            const vec3_f32 eye_dir = curr_camera->get_camera_direction();

            sb_.clear();
            sb_.appendf(
                "Frame Size: %dx%d"
                , _fb_main.width_pixels(), _fb_main.height_pixels()
            );

            if (msaa_enabled_state) {
                sb_.appendf("\nMSAA: Enabled (%dx)", _state.fb_sample_count);
            } else {
                sb_.append("\nMSAA: Disabled");
            }

            sb_.appendf(
                "\nCamera ID: #%X"
                "\nEye Position: [%f, %f, %f]"
                "\nEye Direction: [%f, %f, %f]"
                "\nEye Center: [%f, %f, %f]"
                "\nFront: [%f, %f, %f]"
                "\nRight: [%f, %f, %f]"
                "\nUp: [%f, %f, %f]"
                "\nYaw: %f"
                "\nPitch: %f"
                "\nZoom: %f"
                "\nFovy: %.1fdeg"
                "\nPerspective Scale: %f"
                , curr_camera
                , eye_pos.x(), eye_pos.y(), eye_pos.z()
                , eye_dir.x(), eye_dir.y(), eye_dir.z()
                , curr_camera_params->lookat_center.x(), curr_camera_params->lookat_center.y(), curr_camera_params->lookat_center.z()
                , curr_camera_params->camera_front.x(), curr_camera_params->camera_front.y(), curr_camera_params->camera_front.z()
                , curr_camera_params->camera_right.x(), curr_camera_params->camera_right.y(), curr_camera_params->camera_right.z()
                , curr_camera_params->camera_up.x(), curr_camera_params->camera_up.y(), curr_camera_params->camera_up.z()
                , curr_camera_params->yaw
                , curr_camera_params->pitch
                , curr_camera_params->zoom
                , curr_camera->get_fovy()
                , curr_camera->get_perspective_scale_factor()
            );
            
            if (scene_mouse_pos) {
                sb_.appendf("\nCursor Position: [%.1f, %.1f]", scene_mouse_pos->x(), scene_mouse_pos->y());
            } else {
                sb_.append("\nCursor Position: N/A");
            }

            {
                const float curr_fps = ImGui::GetIO().Framerate;
                sb_.appendf("\nFrame Time: %.3fms (%.1f FPS)", 1000.0f / curr_fps, curr_fps);

                if (_state.refresh_time == 0.0) {
                    _state.refresh_time = ImGui::GetTime();
                }

                // Create data at fixed 60 Hz rate for the demo
                while (_state.refresh_time < ImGui::GetTime()) {
                    _state.values[_state.values_offset] = curr_fps;
                    _state.values_offset = (_state.values_offset + 1) % _state.values.size();

                    //_phase += 0.10f * _state.values_offset;
                    _state.refresh_time += 1.0f / 60.0f;
                }
            }

            ImGui::TextColored(ImVec4(64, 64, 64, 255), "%s", sb_.c_str());

            // Plots can display overlay texts
            // (in this example, we will display an average value)
            {
                float avg = 0.0f;
                for (size_t n = 0; n < _state.values.size(); n++) { avg += _state.values[n]; }
                avg /= static_cast<float>(_state.values.size());

                sb_.clear();
                sb_.appendf("avg %f", avg);

                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
                ImVec2 graphSize{ 128.0f, 64.0f };
                graphSize.x *= render_ctx.dpi_scale;
                graphSize.y *= render_ctx.dpi_scale;
                ImGui::PlotLines("",
                    _state.values.data(),
                    static_cast<int>(_state.values.size()),
                    _state.values_offset,
                    sb_.c_str(),
                    0.0f,
                    120.0f,
                    graphSize
                );
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();
            }
            // right-click context menu
            if (ImGui::BeginPopupContextWindow())
            {
                if (ImGui::MenuItem("Enable MSAA", nullptr, &msaa_enabled_state)) {
                    _state.fb_sample_count = msaa_enabled_state ? 4 : 1;
                    _state.flag_invalidate_fbo = true;
                }

                if (ImGui::BeginMenu("Layout")) {
                    if (ImGui::MenuItem("Custom", NULL, location == -1)) { location = -1; }
                    if (ImGui::MenuItem("Center", NULL, location == -2)) { location = -2; }
                    if (ImGui::MenuItem("Top-left", NULL, location == 0)) { location = 0; }
                    if (ImGui::MenuItem("Top-right", NULL, location == 1)) { location = 1; }
                    if (ImGui::MenuItem("Bottom-left", NULL, location == 2)) { location = 2; }
                    if (ImGui::MenuItem("Bottom-right", NULL, location == 3)) { location = 3; }
                    ImGui::EndMenu();
                }

                //if (ImGui::MenuItem("Close")) { _state.flag_show_overlay = false; }

                ImGui::EndPopup();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

} // namespace