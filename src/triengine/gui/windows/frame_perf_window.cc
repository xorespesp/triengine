#include "frame_perf_window.hh"
#include <triengine/visualization/visualizer.hh>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace triengine::gui
{
    namespace
    {
        constexpr ImU32 kOtherColor = IM_COL32(0x65, 0x63, 0x5e, 255);
        constexpr ImU32 kFullFrameColor = IM_COL32(0xe4, 0xe2, 0xda, 255);
        constexpr ImU32 kFpsColor = kFullFrameColor; // FPS is derived from the full frame time.
        constexpr ImU32 kCapColor = IM_COL32(0xe5, 0x48, 0x4d, 255);
        constexpr ImU32 kDarkTextColor = IM_COL32(0x0b, 0x0b, 0x0b, 255);
        constexpr ImU32 kLightTextColor = IM_COL32(0xf5, 0xf5, 0xf2, 255);

        // Range of the fps drag control only; `visualizer::change_max_fps()` accepts any value.
        constexpr std::uint32_t kMinFps = 30;
        constexpr std::uint32_t kMaxFps = 2000;

        // The last segment: the part of a frame outside every stage.
        constexpr const char* kOtherSegmentName = "Other";
        constexpr const char* kOtherSegmentDescription =
            "Time in the frame outside the measured stages: application work and waits.";

        const char* segment_name(const utility::frame_profiler& profiler, size_t segment)
        {
            return segment < profiler.stage_count() ? profiler.get_stage(segment).name.c_str() : kOtherSegmentName;
        }

        const char* segment_description(const utility::frame_profiler& profiler, size_t segment)
        {
            return segment < profiler.stage_count() ? profiler.get_stage(segment).description.c_str() : kOtherSegmentDescription;
        }

        // Stages sample Viridis in stage order, skipping its dark end that would sink into the background.
        ImU32 segment_color(const utility::frame_profiler& profiler, size_t segment)
        {
            const size_t stage_count = profiler.stage_count();
            if (segment >= stage_count) { return kOtherColor; }
            constexpr float kFirst = 0.35f, kLast = 1.00f;
            const float t = stage_count > 1
                ? kFirst + (kLast - kFirst) * static_cast<float>(segment) / static_cast<float>(stage_count - 1)
                : kFirst;
            return ImGui::ColorConvertFloat4ToU32(ImPlot::SampleColormap(t, ImPlotColormap_Viridis));
        }

        ImU32 text_color_on(ImU32 fill)
        {
            const ImVec4 c = ImGui::ColorConvertU32ToFloat4(fill);
            const float luminance = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
            return luminance > 0.5f ? kDarkTextColor : kLightTextColor;
        }

        void sparkline(
            const char* id, const double* xs, const double* ys, int count, int stride,
            double x_min, double x_max, double y_max, ImU32 color, ImVec2 size,
            const double* reference_y = nullptr, ImU32 reference_color = 0)
        {
            ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0.0f, 0.0f));
            if (ImPlot::BeginPlot(id, size, ImPlotFlags_CanvasOnly | ImPlotFlags_NoInputs)) {
                ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
                ImPlot::SetupAxesLimits(x_min, x_max, 0.0, y_max, ImPlotCond_Always);
                if (reference_y) {
                    ImPlot::PlotInfLines("##reference", reference_y, 1, ImPlotSpec(
                        ImPlotProp_LineColor, reference_color,
                        ImPlotProp_Flags, ImPlotInfLinesFlags_Horizontal));
                }
                // FIXME: ImPlot applies `FillAlpha` twice on shaded plots (epezent/implot#717), so the fill alpha goes into the color.
                const ImU32 fill_color = (color & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 64);
                ImPlot::PlotLine("##value", xs, ys, count, ImPlotSpec(
                    ImPlotProp_LineColor, color,
                    ImPlotProp_FillColor, fill_color,
                    ImPlotProp_Stride, stride,
                    ImPlotProp_Flags, ImPlotLineFlags_Shaded));
                ImPlot::EndPlot();
            }
            ImPlot::PopStyleVar();
        }

        // The value `values` would hold at `index` if sorted, in O(n); reorders `values`.
        double select_nth(std::vector<double>& values, size_t index)
        {
            std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values.end());
            return values[index];
        }

        void help(const char* text)
        {
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
                ImGui::TextUnformatted(text);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // frame_perf_window::sample_buffer
    ////////////////////////////////////////////////////////////////////////////////

    void frame_perf_window::sample_buffer::clear() noexcept
    {
        _values.clear();
        _count = 0;
    }

    void frame_perf_window::sample_buffer::refresh(const utility::frame_profiler& profiler, double seconds)
    {
        const size_t stage_count = profiler.stage_count();
        _stride = kSampleSegmentMsOffset + stage_count + 1;
        const size_t record_count = profiler.is_enabled() ? profiler.record_count() : 0;
        if (record_count == 0) {
            this->clear();
            return;
        }

        const double latest_time = profiler.get_record(record_count - 1).time_seconds();
        const size_t first = seconds > 0.0 ? profiler.find_first_record_after(latest_time - seconds) : 0;
        _count = record_count - first;
        _values.resize(_count * _stride);

        for (size_t i = 0; i < _count; ++i) {
            const auto record = profiler.get_record(first + i);
            double* const values = _values.data() + i * _stride;
            const double frame_ms = record.frame_ms();
            values[kSampleTimeSecondsOffset] = record.time_seconds();
            values[kSampleFrameMsOffset] = frame_ms;
            values[kSampleFpsOffset] = 1000.0 / frame_ms;

            double other_ms = frame_ms;
            for (utility::frame_profiler::stage_id_t stage = 0; stage < stage_count; ++stage) {
                const double stage_ms = record.stage_ms(stage);
                values[kSampleSegmentMsOffset + stage] = stage_ms;
                if (!std::isnan(stage_ms)) { other_ms -= stage_ms; }
            }
            values[kSampleSegmentMsOffset + stage_count] = (std::max)(0.0, other_ms);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    // frame_perf_window::plot_y_axis
    ////////////////////////////////////////////////////////////////////////////////

    void frame_perf_window::plot_y_axis::render_axis_controls(const char* label)
    {
        ImGui::PushID(label);
        ImGui::SeparatorText(label);
        ImGui::Checkbox("Auto fit", &auto_fit);
        // Keep the value being edited (e.g. mid-drag), but show the linked plot limits otherwise.
        if (!editing) {
            input_minimum = minimum;
            input_maximum = maximum;
        }
        ImGui::BeginDisabled(auto_fit);
        const float width = ImGui::GetFontSize() * 6.0f;
        ImGui::SetNextItemWidth(width);
        constexpr double kZero = 0.0;
        constexpr double kMinimumSpan = 0.001;
        const double min_ceiling = (std::max)(kZero, (std::min)(maximum - kMinimumSpan,
            std::nextafter(maximum, kZero)));
        const float speed = static_cast<float>(std::clamp((maximum - minimum) * 0.005, 0.001, 1000.0));
        bool changed = ImGui::DragScalar(
            "Min", ImGuiDataType_Double, &input_minimum,
            speed, &kZero, &min_ceiling, "%.3f", ImGuiSliderFlags_AlwaysClamp
        );

        bool is_editing = ImGui::IsItemActive();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(width);
        const double max_floor = (std::max)(input_minimum + kMinimumSpan,
            std::nextafter(input_minimum, std::numeric_limits<double>::infinity()));
        changed = ImGui::DragScalar(
            "Max", ImGuiDataType_Double, &input_maximum,
            speed, &max_floor, nullptr, "%.3f", ImGuiSliderFlags_AlwaysClamp
        ) || changed;
        is_editing = ImGui::IsItemActive() || is_editing;

        const bool valid =
            std::isfinite(input_minimum) && std::isfinite(input_maximum) &&
            input_minimum >= 0.0 && input_maximum > input_minimum;

        if (changed && valid && !auto_fit) {
            minimum = input_minimum;
            maximum = input_maximum;
        }
        editing = is_editing;
        ImGui::EndDisabled();
        if (!valid) {
            input_minimum = minimum;
            input_maximum = maximum;
        }
        ImGui::PopID();
    }

    void frame_perf_window::plot_y_axis::setup_y_axis(const char* label)
    {
        const auto flags = ImPlotAxisFlags_NoMenus |
            (auto_fit ? ImPlotAxisFlags_AutoFit : ImPlotAxisFlags_None);
        ImPlot::SetupAxis(ImAxis_Y1, label, flags);
        ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, HUGE_VAL);
        // Bidirectional links apply numeric edits and receive pan, zoom, and auto-fit results.
        ImPlot::SetupAxisLinks(ImAxis_Y1, &minimum, &maximum);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // frame_perf_window
    ////////////////////////////////////////////////////////////////////////////////

    void frame_perf_window::_refresh_statistics()
    {
        const size_t segment_count = _samples.segment_count();
        _statistics = {};
        _statistics.average_segment_ms.assign(segment_count, 0.0);
        _statistics.max_segment_ms.assign(segment_count, 0.0);
        _frame_time_scratch.clear();
        _statistics.sample_count = _samples.size();
        if (!_samples.empty()) {
            _statistics.latest_frame_ms = _samples.frame_ms(_samples.size() - 1);
            _statistics.min_frame_ms = std::numeric_limits<double>::infinity();
            _frame_time_scratch.reserve(_samples.size());
            for (size_t sample = 0; sample < _samples.size(); ++sample) {
                const double frame_ms = _samples.frame_ms(sample);
                _frame_time_scratch.push_back(frame_ms);
                _statistics.average_frame_ms += frame_ms;
                _statistics.min_frame_ms = (std::min)(_statistics.min_frame_ms, frame_ms);
                _statistics.max_frame_ms = (std::max)(_statistics.max_frame_ms, frame_ms);
                for (size_t segment = 0; segment < segment_count; ++segment) {
                    const double segment_ms = _samples.segment_ms(sample, segment);
                    if (!std::isnan(segment_ms)) {
                        _statistics.average_segment_ms[segment] += segment_ms;
                        _statistics.max_segment_ms[segment] = (std::max)(_statistics.max_segment_ms[segment], segment_ms);
                    }
                }
            }
            const double count = static_cast<double>(_samples.size());
            _statistics.duration_seconds = _statistics.average_frame_ms / 1000.0;
            _statistics.average_fps = count / _statistics.duration_seconds;
            _statistics.average_frame_ms /= count;
            for (auto& segment_ms : _statistics.average_segment_ms) { segment_ms /= count; }
            _statistics.p95_frame_ms = select_nth(_frame_time_scratch, static_cast<size_t>(std::ceil(count * 0.95)) - 1);
            _statistics.p99_frame_ms = select_nth(_frame_time_scratch, static_cast<size_t>(std::ceil(count * 0.99)) - 1);
        }
    }

    void frame_perf_window::_refresh_samples(const utility::frame_profiler& profiler)
    {
        _samples.refresh(profiler, _history_seconds);
        _next_refresh_time = ImGui::GetTime() + 1.0 / 60.0;
    }

    void frame_perf_window::_render_controls(visualization::visualizer& vis)
    {
        if (!ImGui::CollapsingHeader("Frame Control")) { return; }
        bool vsync = vis.is_vsync_enabled();
        if (ImGui::Checkbox("V-Sync (requested)", &vsync)) {
            vis.set_vsync_enabled(vsync);
        }
        help("Synchronize buffer swaps to display refresh. Driver settings may override this request. "
             "The FPS limit remains independent.");

        // Mirror the applied cap as is, so a value set through the API outside the drag range is not rewritten.
        auto fps_cap = vis.get_max_fps();
        if (fps_cap != 0) { _remembered_fps_cap = fps_cap; }
        std::uint32_t fps_input = _remembered_fps_cap;
        bool limited = fps_cap != 0;
        if (ImGui::Checkbox("Max FPS", &limited)) {
            vis.change_max_fps(limited ? _remembered_fps_cap : 0);
            fps_cap = vis.get_max_fps();
        }
        help("Enable the maximum FPS cap. Uncheck for uncapped rendering.");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f * (ImGui::GetFontSize() / 16.0f));
        ImGui::BeginDisabled(!limited);
        if (ImGui::DragScalar("##Max FPS", ImGuiDataType_U32, &fps_input, 1.0f, &kMinFps, &kMaxFps, "%u", ImGuiSliderFlags_AlwaysClamp)) {
            vis.change_max_fps(fps_input);
            _remembered_fps_cap = fps_input;
        }
        ImGui::SameLine(0.0f, 0.0f);
        if (ImGui::ArrowButton("##FpsPresets", ImGuiDir_Down)) { ImGui::OpenPopup("FpsPresets"); }
        if (ImGui::BeginPopup("FpsPresets")) {
            for (const auto fps : { 30u, 60u, 90u, 120u, 144u, 165u, 240u, 360u }) {
                char label[24];
                std::snprintf(label, sizeof(label), "%u FPS", fps);
                if (ImGui::Selectable(label, fps == vis.get_max_fps())) {
                    vis.change_max_fps(fps);
                    _remembered_fps_cap = fps;
                }
            }
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
        fps_cap = vis.get_max_fps();
        if (fps_cap) {
            ImGui::Text("CPU limit: %u FPS / %.3f ms budget", fps_cap, 1000.0 / fps_cap);
        } else {
            ImGui::TextUnformatted("CPU limit: unlimited");
        }
    }

    void frame_perf_window::_render_statistics()
    {
        if (_statistics.sample_count == 0) {
            ImGui::TextDisabled("Waiting for completed frames...");
            return;
        }
        const auto& stats = _statistics;
        ImGui::Text("Latest: %.1f FPS / %.3f ms", 1000.0 / stats.latest_frame_ms, stats.latest_frame_ms);
        ImGui::Text("Average: %.1f FPS / %.3f ms", stats.average_fps, stats.average_frame_ms);
        ImGui::Text("Frame time: min %.3f / P95 %.3f / P99 %.3f / max %.3f ms",
            stats.min_frame_ms, stats.p95_frame_ms, stats.p99_frame_ms, stats.max_frame_ms);
        help("Percentiles use raw frame times, not smoothed FPS. "
             "P99 means 99% of these frames completed within this time.");
        ImGui::TextDisabled("%zu frames / %.2f s retained (up to %zu frames)",
            stats.sample_count, stats.duration_seconds, utility::frame_profiler::kRecordsCapacity);

    }

    // Ignores the outermost 1% on each side, since the first frames after a reset are often far off.
    void frame_perf_window::_fit_fps_axis()
    {
        auto& frame_times = _frame_time_scratch;
        if (frame_times.empty()) { return; }
        const auto quantile = [&](double q) {
            return select_nth(frame_times, static_cast<size_t>(q * static_cast<double>(frame_times.size() - 1)));
        };
        const double lower_fps = 1000.0 / quantile(0.99);
        const double upper_fps = 1000.0 / quantile(0.01);
        const double padding = (std::max)((upper_fps - lower_fps) * 0.1, 1.0);
        _fps_axis.set_limits((std::max)(0.0, lower_fps - padding), upper_fps + padding);
    }

    void frame_perf_window::_render_fps_plot(const float dpi_scale, const std::uint32_t fps_cap)
    {
        if (_samples.empty()) { return; }
        const double end = _samples.time_seconds(_samples.size() - 1);
        const double begin = (std::max)(0.0, end - _history_seconds);

        ImGui::SeparatorText("FPS");
        if (!ImPlot::BeginPlot("##FrameRate", { -1.0f, 200.0f * dpi_scale })) { return; }
        ImPlot::SetupAxis(ImAxis_X1, "Time since reset (s)", ImPlotAxisFlags_NoHighlight);
        _fps_axis.setup_y_axis("FPS");
        ImPlot::SetupAxisLimits(ImAxis_X1, begin, (std::max)(end, begin + 0.001), ImPlotCond_Always);
        ImPlot::SetupLegend(ImPlotLocation_NorthEast, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
        ImPlot::PlotLine("FPS", _samples.time_column(), _samples.fps_column(), static_cast<int>(_samples.size()), ImPlotSpec(
            ImPlotProp_LineColor, kFpsColor,
            ImPlotProp_LineWeight, 1.5f,
            ImPlotProp_Stride, _samples.stride_bytes()));
        if (fps_cap) {
            const double target = static_cast<double>(fps_cap);
            ImPlot::PlotInfLines("Cap", &target, 1, ImPlotSpec(
                ImPlotProp_LineColor, kCapColor,
                ImPlotProp_LineWeight, 1.5f,
                ImPlotProp_Flags, ImPlotInfLinesFlags_Horizontal));
        }
        ImPlot::EndPlot();
    }

    void frame_perf_window::_render_frame_time(
        const utility::frame_profiler& profiler, const float dpi_scale, const std::uint32_t fps_cap)
    {
        const auto& stats = _statistics;
        if (_samples.empty() || stats.sample_count == 0 || !(stats.average_frame_ms > 0.0)) { return; }
        ImGui::SeparatorText("Frame Time");

        ImDrawList* const draw_list = ImGui::GetWindowDrawList();
        {
            const ImVec2 bar_min = ImGui::GetCursorScreenPos();
            const float bar_width = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
            const float bar_height = ImGui::GetFrameHeight() * 1.25f;
            const float bar_max_x = bar_min.x + bar_width;
            ImGui::InvisibleButton("##FrameBreakdownBar", { bar_width, bar_height });
            const bool bar_hovered = ImGui::IsItemHovered();
            const float mouse_x = ImGui::GetIO().MousePos.x;
            const float text_padding = ImGui::GetStyle().FramePadding.x;

            float x = bar_min.x;
            for (size_t segment = 0; segment < stats.average_segment_ms.size(); ++segment) {
                const double ms = stats.average_segment_ms[segment];
                const double fraction = ms / stats.average_frame_ms;
                const float right = (std::min)(bar_max_x, x + static_cast<float>(fraction) * bar_width);
                if (right <= x) { continue; }

                const ImU32 fill = segment_color(profiler, segment);
                draw_list->AddRectFilled({ x, bar_min.y }, { right, bar_min.y + bar_height }, fill);
                char text[16];
                std::snprintf(text, sizeof(text), "%.0f%%", 100.0 * fraction);
                const ImVec2 text_size = ImGui::CalcTextSize(text);
                if (text_size.x + text_padding * 2.0f <= right - x) {
                    draw_list->AddText(
                        { x + (right - x - text_size.x) * 0.5f, bar_min.y + (bar_height - text_size.y) * 0.5f },
                        text_color_on(fill), text);
                }
                if (bar_hovered && mouse_x >= x && mouse_x < right) {
                    ImGui::SetTooltip("%s: %.3f ms (%.1f%%)", segment_name(profiler, segment), ms, 100.0 * fraction);
                }
                x = right;
            }
        }

        const int count = static_cast<int>(_samples.size());
        const int stride = _samples.stride_bytes();
        const double end = _samples.time_seconds(_samples.size() - 1);
        const double begin = (std::max)(0.0, end - _history_seconds);
        const double x_max = (std::max)(end, begin + 0.001);
        const ImVec2 trace_size{ -1.0f, 28.0f * dpi_scale };
        constexpr double kHeadroom = 1.15;

        const ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH;
        if (!ImGui::BeginTable("FrameTimeSegments", 4, flags)) { return; }
        ImGui::TableSetupColumn("Segment");
        ImGui::TableSetupColumn("Average");
        ImGui::TableSetupColumn("Percent");
        ImGui::TableSetupColumn("Trace", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        const float marker = ImGui::GetFontSize() * 0.65f;
        const auto name_cell = [&](const char* name, ImU32 color, const char* description) {
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float offset = (ImGui::GetFrameHeight() - marker) * 0.5f;
            draw_list->AddRectFilled({ pos.x, pos.y + offset }, { pos.x + marker, pos.y + offset + marker }, color);
            ImGui::Dummy({ marker, ImGui::GetFrameHeight() });
            ImGui::SameLine();
            ImGui::TextUnformatted(name);
            if (description && *description) { help(description); }
        };
        const auto value_cells = [&](double ms, double percent) {
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::Text("%.3f ms", ms);
            ImGui::TableNextColumn(); ImGui::AlignTextToFramePadding(); ImGui::Text("%.1f%%", percent);
        };

        ImGui::PushID("FullFrame");
        ImGui::TableNextRow();
        name_cell("Full frame", kFullFrameColor, nullptr);
        value_cells(stats.average_frame_ms, 100.0);
        ImGui::TableNextColumn();
        {
            const double budget = fps_cap ? 1000.0 / fps_cap : 0.0;
            const double y_max = (std::max)(stats.max_frame_ms, budget) * kHeadroom;
            sparkline("##trace", _samples.time_column(), _samples.frame_column(), count, stride,
                begin, x_max, y_max, kFullFrameColor, trace_size, fps_cap ? &budget : nullptr, kCapColor);
        }
        ImGui::PopID();

        for (size_t segment = 0; segment < stats.average_segment_ms.size(); ++segment) {
            const double ms = stats.average_segment_ms[segment];
            const ImU32 color = segment_color(profiler, segment);
            ImGui::PushID(static_cast<int>(segment));
            ImGui::TableNextRow();
            name_cell(segment_name(profiler, segment), color, segment_description(profiler, segment));
            value_cells(ms, 100.0 * ms / stats.average_frame_ms);
            ImGui::TableNextColumn();
            const double y_max = (std::max)(stats.max_segment_ms[segment] * kHeadroom, 0.001);
            sparkline("##trace", _samples.time_column(), _samples.segment_column(segment), count, stride,
                begin, x_max, y_max, color, trace_size);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    bool frame_perf_window::_render_plot_options(const float dpi_scale)
    {
        if (ImGui::Button("Plot options")) { ImGui::OpenPopup("PlotOptionsPopup"); }
        const auto* viewport = ImGui::GetWindowViewport();
        const ImVec2 button_min = ImGui::GetItemRectMin();
        const ImVec2 button_max = ImGui::GetItemRectMax();
        const float gap = ImGui::GetStyle().ItemSpacing.y;
        const float below = viewport->WorkPos.y + viewport->WorkSize.y - button_max.y - gap;
        const float above = button_min.y - viewport->WorkPos.y - gap;
        const bool open_above = below < 400.0f * dpi_scale && above > below;
        const float width = (std::min)(400.0f * dpi_scale, viewport->WorkSize.x);
        const float left = std::clamp(button_min.x, viewport->WorkPos.x,
            viewport->WorkPos.x + viewport->WorkSize.x - width);
        ImGui::SetNextWindowPos(
            { left, open_above ? button_min.y - gap : button_max.y + gap },
            ImGuiCond_Appearing, { 0.0f, open_above ? 1.0f : 0.0f });
        ImGui::SetNextWindowSizeConstraints(
            { width, 0.0f }, { width, (std::max)(100.0f, open_above ? above : below) });
        ImGui::SetNextWindowBgAlpha(1.0f);
        bool refresh = false;
        if (ImGui::BeginPopup("PlotOptionsPopup", ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::SeparatorText("Plot options");
            if (ImGui::IsWindowAppearing()) {
                _fps_axis.editing = false;
            }
            ImGui::SetNextItemWidth(220.0f * dpi_scale);
            constexpr double kMinSeconds = 1.0, kMaxSeconds = 10.0;
            refresh = ImGui::SliderScalar("History window", ImGuiDataType_Double, &_history_seconds,
                &kMinSeconds, &kMaxSeconds, "%.1f s", ImGuiSliderFlags_AlwaysClamp);
            _fps_axis.render_axis_controls("FPS");
            ImGui::Separator();
            if (ImGui::Button("Close")) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        return refresh;
    }

    void frame_perf_window::render(const window_render_context& render_ctx)
    {
        auto* vis = render_ctx.visualizer;
        if (!vis) {
            ImGui::TextDisabled("Register this window with a visualizer to access frame controls.");
            return;
        }
        this->_render_controls(*vis);
        if (!ImGui::CollapsingHeader("Profiling", ImGuiTreeNodeFlags_DefaultOpen)) { return; }

        auto& profiler = vis->get_frame_profiler();
        bool measuring = profiler.is_enabled();
        if (ImGui::Checkbox("Enable profiling", &measuring)) {
            profiler.request_enabled(measuring);
            _samples.clear();
            _statistics = {};
            _next_refresh_time = 0.0;
        }
        if (!measuring) {
            ImGui::TextDisabled("Frame profiling is disabled.");
            return;
        }
        bool refresh = false;
        if (ImGui::Button("Reset profiling")) {
            profiler.request_reset();
            refresh = true;
        }
        ImGui::SameLine();
        if (this->_render_plot_options(render_ctx.dpi_scale)) { refresh = true; }
        const bool new_epoch = _samples_epoch != profiler.history_epoch();
        if (new_epoch) { _fps_axis_needs_fit = true; }
        refresh = refresh || new_epoch;

        if (refresh || ImGui::GetTime() >= _next_refresh_time) {
            this->_refresh_samples(profiler);
            this->_refresh_statistics();
            _samples_epoch = profiler.history_epoch();
            if (_fps_axis_needs_fit && !_fps_axis.auto_fit && !_frame_time_scratch.empty()) {
                this->_fit_fps_axis();
                _fps_axis_needs_fit = _statistics.duration_seconds < 1.0;
            }
        }
        ImGui::SeparatorText("Statistics");
        this->_render_statistics();
        this->_render_fps_plot(render_ctx.dpi_scale, vis->get_max_fps());
        this->_render_frame_time(profiler, render_ctx.dpi_scale, vis->get_max_fps());
        ImGui::TextDisabled("CPU wall time only; GPU execution time is not measured.");
    }

    void frame_perf_window::on_added(visualization::visualizer& vis)
    {
        _next_refresh_time = 0.0;
        _samples.clear();
        _statistics = {};
        _fps_axis_needs_fit = true;
        const auto fps_cap = vis.get_max_fps();
        _remembered_fps_cap = fps_cap ? fps_cap : visualization::visualizer::kDefaultMaxFps;
        vis.get_frame_profiler().request_enabled(true);
    }

}
