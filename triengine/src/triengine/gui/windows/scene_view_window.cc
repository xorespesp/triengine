#include "scene_view_window.hh"
#include <triengine/utility/string_format.hh>
#include <triengine/visualization/visualizer.hh>

namespace triengine::gui
{
    scene_view_window::scene_view_window(
        visualization::visualizer* const vis)
        : _vis{ vis }
    {
        _state.curr_content_region.Max.x = static_cast<float>(vis->get_window_width());
        _state.curr_content_region.Max.y = static_cast<float>(vis->get_window_height());
        _state.flag_invalidate_fbo = true;
    }

    const core::frame_buffer& scene_view_window::get_framebuffer() const noexcept
    {
        return _fb_main;
    }

    void scene_view_window::bind_framebuffer()
    {
        if (_state.flag_invalidate_fbo)
        {
            // invalidate framebuffer

            const ImVec2
                curr_content_region_size = _state.curr_content_region.GetSize();

            if (!_fb_main.is_valid())
            {
                _fb_main = core::frame_buffer::create_color_depth_stencil_buffer(
                    GL_RGBA16F,
                    GL_DEPTH_COMPONENT24,
                    GL_STENCIL_INDEX8,
                    static_cast<int32_t>(curr_content_region_size.x),
                    static_cast<int32_t>(curr_content_region_size.y)
                );
            }
            else
            {
                // It is okay to call reallocate every frame, 
                // as there is an internal reallocation-skip optimization implemented.
                _fb_main.reallocate(
                    static_cast<int32_t>(curr_content_region_size.x),
                    static_cast<int32_t>(curr_content_region_size.y)
                );
            }

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
            ImGui::Image(
                static_cast<ImTextureID>(static_cast<uint64_t>(_fb_main.color_attachment()->buffer_id)),
                curr_content_region_size,
                ImVec2(0, 1),
                ImVec2(1, 0)
            );

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
        thread_local utility::string::format_string_builder<1024> sb_;

        ImGuiWindowFlags window_flags = 
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | 
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

        if (_state.overlay_location >= 0)
        {
            constexpr float kPadSize = 15.0f;
            ImVec2 work_pos = _state.curr_content_region.Min;
            ImVec2 work_size = _state.curr_content_region.GetSize();
            ImVec2 window_pos, window_pos_pivot;
            window_pos.x = (_state.overlay_location & 1) ? (work_pos.x + work_size.x - kPadSize) : (work_pos.x + kPadSize);
            window_pos.y = (_state.overlay_location & 2) ? (work_pos.y + work_size.y - kPadSize) : (work_pos.y + kPadSize);
            window_pos_pivot.x = (_state.overlay_location & 1) ? 1.0f : 0.0f;
            window_pos_pivot.y = (_state.overlay_location & 2) ? 1.0f : 0.0f;
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
            ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID); // for multi-viewport environment
            window_flags |= ImGuiWindowFlags_NoMove;
        }
        else if (_state.overlay_location == -2)
        {
            // Center window
            ImGui::SetNextWindowPos(_state.curr_content_region.GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            window_flags |= ImGuiWindowFlags_NoMove;
        }

        ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 5.0f * render_ctx.dpi_scale, 5.0f * render_ctx.dpi_scale });
        if (ImGui::Begin("##SceneWindowOverlay", &_state.flag_show_overlay, window_flags))
        {
            const auto scene_mouse_pos = 
                [this]() -> std::optional<vec2_f32> {
                    if (ImGui::IsMousePosValid()) {
                        const auto& mouse_pos = ImGui::GetIO().MousePos;
                        return this->try_convert_screen_pos_2_viewport_pos(vec2_f32{ mouse_pos.x, mouse_pos.y });
                    }
                    return std::nullopt;
                }();

            const uint32_t& curr_scn_id = _vis->get_current_scene()->get_id();
            const std::string& curr_scn_name = _vis->get_current_scene()->get_name();
            
            sb_.clear();
            sb_.appendf(
                "Scene: %s (#%X)"
                "\nDPI Scaling: %.2fX"
                "\nFrame Size: %dx%d"
                , curr_scn_name.c_str()
                , curr_scn_id
                , render_ctx.dpi_scale
                , _fb_main.width_pixels(), _fb_main.height_pixels()
            );

            if (_state.flag_show_overlay_debug_info)
            {
                const camera* const curr_camera = _vis->get_current_scene()->get_camera();
                const camera_parameters* const curr_camera_params = &curr_camera->get_parameters();
                const vec3_f32 eye_pos = curr_camera->get_position();
                const vec3_f32 eye_dir = curr_camera->get_direction();

                sb_.appendf(
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
            }
            
            {
                const float curr_dT = static_cast<float>(ImGui::GetTime());
                const float curr_fps = ImGui::GetIO().Framerate;

                sb_.appendf("\nFrame Time: %.3fms (%.1f FPS)", 1000.0f / curr_fps, curr_fps);
                ImGui::TextColored(ImVec4(64, 64, 64, 255), "%s", sb_.c_str());

                if (!_state.fps_plot_next_update_time) {
                    _state.fps_plot_next_update_time = ImGui::GetTime();
                }

                // Create data at fixed `kFPSPlotUpdateFreq`-Hz rate for the demo
                while (_state.fps_plot_next_update_time.value() < ImGui::GetTime()) {
                    _state.fps_plot_buffer.emplace_value(curr_dT, curr_fps);
                    _state.fps_plot_next_update_time.value() += 1.0f / kFPSPlotUpdateFreq;
                }

                double curr_fps_avg{ 0.0 };
                for (const auto fps_val : _state.fps_plot_buffer.data) {
                    curr_fps_avg += fps_val.y();
                }
                curr_fps_avg /= static_cast<double>(_state.fps_plot_buffer.data.size());

                ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2{ 5.0f * render_ctx.dpi_scale, 5.0f * render_ctx.dpi_scale });
                ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.1f);
                ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.0f);
                ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
                ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
                ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4{ 1.0f, 1.0f, 1.0f, 0.5f });
                ImPlot::PushStyleColor(ImPlotCol_Fill, ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f });

                const ImVec2 plot_size{ 150.0f * render_ctx.dpi_scale, 40.0f * render_ctx.dpi_scale };
                if (ImPlot::BeginPlot("##FPSPlot", plot_size, ImPlotFlags_NoFrame | ImPlotFlags_NoMouseText))
                {
                    ImPlot::SetupAxes("Time", "FPS", 
                        ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_NoHighlight,
                        ImPlotAxisFlags_NoDecorations
                    );

                    ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, 1000.0);

                    ImPlot::SetupAxisLimits(ImAxis_X1, curr_dT - kFPSPlotHistorySize, curr_dT, ImPlotCond_Always);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 800.0, ImPlotCond_Once);
                    ImPlot::SetupAxisTicks(ImAxis_Y1, 0.0, 1000.0, 10, nullptr, false);

                    ImPlot::PlotShaded("##FPSShadedPlot",
                        &_state.fps_plot_buffer.data.begin()->x(),
                        &_state.fps_plot_buffer.data.begin()->y(),
                        static_cast<int>(_state.fps_plot_buffer.data.size()),
                        0.0,
                        ImPlotShadedFlags_None,
                        _state.fps_plot_buffer.offset,
                        sizeof(std::decay_t<decltype(_state.fps_plot_buffer)>::value_type)
                    );

                    ImPlot::PlotLine("##FPSLinePlot",
                        &_state.fps_plot_buffer.data.begin()->x(),
                        &_state.fps_plot_buffer.data.begin()->y(),
                        static_cast<int>(_state.fps_plot_buffer.data.size()),
                        ImPlotShadedFlags_None,
                        _state.fps_plot_buffer.offset,
                        sizeof(std::decay_t<decltype(_state.fps_plot_buffer)>::value_type)
                    );

                    if (ImDrawList* const draw_list{ ImPlot::GetPlotDrawList() };
                        draw_list)
                    {
                        const ImPlotRect plot_limits{ ImPlot::GetPlotLimits() };
                        const ImPlotPoint plot_center_point{ (plot_limits.X.Min + plot_limits.X.Max) * 0.5, (plot_limits.Y.Min + plot_limits.Y.Max) * 0.5 };
                        const ImVec2 plot_center_pixels{ ImPlot::PlotToPixels(plot_center_point) };

                        sb_.clear();
                        sb_.appendf("avg %.2f", curr_fps_avg);

                        const ImVec2 text_size{ ImGui::CalcTextSize(sb_.c_str()) };
                        const ImVec2 text_start_pos{
                            plot_center_pixels.x - text_size.x * 0.5f,
                            plot_center_pixels.y - text_size.y * 0.5f
                        };

                        draw_list->AddText(text_start_pos, ImGui::GetColorU32(ImGuiCol_Text), sb_.c_str());
                    }

                    if (ImPlot::IsPlotHovered())
                    {
                        const ImPlotPoint mouse_pos{ ImPlot::GetPlotMousePos() };
                        ImGui::BeginTooltip();
                        ImGui::Text("dT: %.3f (sec)", mouse_pos.x);
                        ImGui::Text("FPS: %.3f", mouse_pos.y);
                        ImGui::EndTooltip();
                    }

                    ImPlot::EndPlot();
                }

                ImPlot::PopStyleColor(4);
                ImPlot::PopStyleVar(3);
            } // fps plot

            // right-click context menu
            if (ImGui::BeginPopupContextWindow())
            {
                if (ImGui::BeginMenu("Layout")) {
                    if (ImGui::MenuItem("Custom##Layout", NULL, _state.overlay_location == -1)) { _state.overlay_location = -1; }
                    if (ImGui::MenuItem("Center##Layout", NULL, _state.overlay_location == -2)) { _state.overlay_location = -2; }
                    if (ImGui::MenuItem("Top-left##Layout", NULL, _state.overlay_location == 0)) { _state.overlay_location = 0; }
                    if (ImGui::MenuItem("Top-right##Layout", NULL, _state.overlay_location == 1)) { _state.overlay_location = 1; }
                    if (ImGui::MenuItem("Bottom-left##Layout", NULL, _state.overlay_location == 2)) { _state.overlay_location = 2; }
                    if (ImGui::MenuItem("Bottom-right##Layout", NULL, _state.overlay_location == 3)) { _state.overlay_location = 3; }
                    ImGui::EndMenu();
                }

                ImGui::MenuItem("Debug Info", NULL, &_state.flag_show_overlay_debug_info);

                //if (ImGui::MenuItem("Close")) { _state.flag_show_overlay = false; }

                ImGui::EndPopup();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

} // namespace