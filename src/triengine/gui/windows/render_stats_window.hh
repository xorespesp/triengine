#pragma once
#include <triengine/gui/iwindow.hh>

#include <algorithm>
#include <array>

namespace triengine::gui
{
    // Ref: https://gist.github.com/dougbinks/8089b4bbaccaaf6fa204236978d165a9
    class render_stats_window
        : public iwindow
    {
        // configuration params - modify these at will
        static constexpr int kNumOfSamples = 101; //last value is from T-1 to inf.

        static constexpr float dT = 0.001f; // in seconds, default 1ms
        static constexpr float kRefreshRate = 1.0f / 60.0f; // set this to your target refresh rate

        static constexpr int kNumOfMarkers = 2;
        const float _markers[kNumOfMarkers] = { 0.99f, 0.999f };

        // histogram data
        float _lastdT = 0.0f;
        float _timesTotal;
        float _countsTotal;
        std::array<float, kNumOfSamples> _times{};
        std::array<float, kNumOfSamples> _counts{};
        std::array<float, kNumOfSamples> _hitchTimes{};
        std::array<float, kNumOfSamples> _hitchCounts{};

        // fps plot data
        // Fill an array of contiguous float values to plot
        // Tip: If your float aren't contiguous but part of a structure, you can pass a pointer to your first float
        // and the sizeof() of your structure in the "stride" parameter.
        std::array<float, kNumOfSamples> _values{};
        int _values_offset = 0;
        double _refresh_time = 0.0;
        float _phase = 0.0f;

    public:
        render_stats_window() { this->ClearData(); }
        virtual ~render_stats_window() = default;

        const char* get_window_name() const override {
            return "Scene Render Stats";
        }

        ImVec2 get_initial_window_size() const override {
            return ImVec2{ 280.0f, 400.0f };
        }

        void render(
            [[maybe_unused]] const window_render_context& render_ctx) override
        {
            const float curr_fps = ImGui::GetIO().Framerate;

            {
                //ImGui::Text("Build: " __DATE__ ", " __TIME__);
                ImGui::Text("Avg %.3f ms/frame (%.1f FPS)", 1000.0f / curr_fps, curr_fps);

                if (_refresh_time == 0.0) {
                    _refresh_time = ImGui::GetTime();
                }

                // Create data at fixed 60 Hz rate for the demo
                while (_refresh_time < ImGui::GetTime()) {
                    _values[_values_offset] = curr_fps;
                    _values_offset = (_values_offset + 1) % _values.size();

                    _phase += 0.10f * _values_offset;
                    _refresh_time += 1.0f / 60.0f;
                }

                // Plots can display overlay texts
                // (in this example, we will display an average value)
                {
                    float avg = 0.0f;
                    for (size_t n = 0; n < _values.size(); n++) { avg += _values[n]; }
                    avg /= static_cast<float>(_values.size());

                    std::array<char, 32> overlay_text;
                    std::snprintf(overlay_text.data(), overlay_text.size(), "avg %f", avg);

                    ImVec2 graphSize{ ImGui::GetContentRegionMax() };
                    graphSize.y = std::clamp(graphSize.y, 0.0f, 90.0f);
                    ImGui::PlotLines("", 
                        _values.data(), 
                        static_cast<int>(_values.size()), 
                        _values_offset, 
                        overlay_text.data(),
                        0.0f, 
                        120.0f, 
                        graphSize
                    );
                }
            }

            this->UpdateData(1000.0f / curr_fps);

            if (ImGui::CollapsingHeader("Time Histogram")) {
                ImVec2 graphSize{ ImGui::GetContentRegionMax() };
                graphSize.y = std::clamp(graphSize.y, 0.0f, 180.0f);
                ImGui::PlotHistogram(
                    "", 
                    _times.data(),
                    static_cast<int>(_times.size()), 
                    0, 
                    nullptr, 
                    FLT_MAX, FLT_MAX, 
                    graphSize
                );
                //this->PlotRefreshLines(_timesTotal, _times.data());
            }

            if (ImGui::CollapsingHeader("Count Histogram")) {
                ImVec2 graphSize{ ImGui::GetContentRegionMax() };
                graphSize.y = std::clamp(graphSize.y, 0.0f, 180.0f);
                ImGui::PlotHistogram(
                    "", 
                    _counts.data(),
                    static_cast<int>(_counts.size()),
                    0, 
                    nullptr,
                    FLT_MAX, FLT_MAX, 
                    graphSize
                );
                //this->PlotRefreshLines(_countsTotal, _counts.data());
            }

            if (ImGui::CollapsingHeader("Hitch Time Histogram")) {
                ImVec2 graphSize{ ImGui::GetContentRegionMax() };
                graphSize.y = std::clamp(graphSize.y, 0.0f, 180.0f);
                ImGui::PlotHistogram(
                    "", 
                    _hitchTimes.data(),
                    static_cast<int>(_hitchTimes.size()),
                    0, 
                    nullptr,
                    FLT_MAX, FLT_MAX, 
                    graphSize
                );
                //this->PlotRefreshLines();
            }

            if (ImGui::CollapsingHeader("Hitch Count Histogram")) {
                ImVec2 graphSize{ ImGui::GetContentRegionMax() };
                graphSize.y = std::clamp(graphSize.y, 0.0f, 180.0f);
                ImGui::PlotHistogram(
                    "", 
                    _hitchCounts.data(),
                    static_cast<int>(_hitchCounts.size()),
                    0, 
                    nullptr,
                    FLT_MAX, FLT_MAX, 
                    graphSize
                );
                //this->PlotRefreshLines();
            }

            if (ImGui::Button("Clear")) {
                this->ClearData();
            }
        }

    private:

        void ClearData()
        {
            _timesTotal = 0.0f;
            _countsTotal = 0.0f;
            std::fill(_times.begin(), _times.end(), 0.0f);
            std::fill(_counts.begin(), _counts.end(), 0.0f);
            std::fill(_hitchTimes.begin(), _hitchTimes.end(), 0.0f);
            std::fill(_hitchCounts.begin(), _hitchCounts.end(), 0.0f);
            std::fill(_values.begin(), _values.end(), 0.0f);
        }

        int GetHistogramBin(float time_)
        {
            int bin = static_cast<int>(std::floor(time_ / dT));
            if (bin >= kNumOfSamples) { 
                bin = kNumOfSamples - 1;
            }
            return bin;
        }

        void UpdateData(float deltaT_)
        {
            if (deltaT_ < 0.0f) {
                assert(false);
                return;
            }

            int bin = GetHistogramBin(deltaT_);
            _times[bin] += deltaT_;
            _timesTotal += deltaT_;
            _counts[bin] += 1.0f;
            _countsTotal += 1.0f;

            float hitch = abs(_lastdT - deltaT_);
            int deltaBin = GetHistogramBin(hitch);
            _hitchTimes[deltaBin] += hitch;
            _hitchCounts[deltaBin] += 1.0f;
            _lastdT = deltaT_;
        }

        void PlotRefreshLines(
            float total_ = 0.0f, 
            float* pValues_ = nullptr)
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();

            const ImGuiStyle& style = ImGui::GetStyle();
            ImVec2 pad = style.FramePadding;
            ImVec2 min = ImGui::GetItemRectMin();
            min.x += pad.x;
            ImVec2 max = ImGui::GetItemRectMax();
            max.x -= pad.x;

            float xRefresh = (max.x - min.x) * kRefreshRate / (dT * kNumOfSamples);

            float xCurr = xRefresh + min.x;
            while (xCurr < max.x)
            {
                float xP = std::ceil(xCurr); // use ceil to get integer coords or else lines look odd
                draw->AddLine(ImVec2(xP, min.y), ImVec2(xP, max.y), 0x50FFFFFF);
                xCurr += xRefresh;
            }

            if (pValues_)
            {
                // calc markers
                float currTotal = 0.0f;
                int   mark = 0;
                for (int i = 0; i < kNumOfSamples && mark < kNumOfMarkers; ++i)
                {
                    currTotal += pValues_[i];
                    if (total_ * _markers[mark] < currTotal)
                    {
                        float xP = std::ceil((float)(i + 1) / (float)kNumOfSamples * (max.x - min.x) + min.x); // use ceil to get integer coords or else lines look odd
                        draw->AddLine(ImVec2(xP, min.y), ImVec2(xP, max.y), 0xFFFF0000);
                        ++mark;
                    }
                }
            }
        }

    }; // class

} // namespace