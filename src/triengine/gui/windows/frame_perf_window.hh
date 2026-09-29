#pragma once
#include <triengine/gui/iwindow.hh>
#include <triengine/visualization/visualizer.hh>
#include <triengine/utility/debug_utils.hh>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace triengine::gui
{
    // A frame splits into segments: one per profiler stage, in stage order, then "Other" for the rest.
    class frame_perf_window : public iwindow
    {
    private:
        class sample_buffer {
        public:
            void clear() noexcept;
            void refresh(const utility::frame_profiler& profiler, double seconds);

            size_t size() const noexcept { return _count; }
            bool empty() const noexcept { return _count == 0; }
            size_t segment_count() const noexcept { return _stride - kSampleSegmentMsOffset; }

            double time_seconds(size_t sample) const noexcept { return this->_at(sample, kSampleTimeSecondsOffset); }
            double frame_ms(size_t sample) const noexcept { return this->_at(sample, kSampleFrameMsOffset); }
            // `NaN` when the stage was not measured in that frame.
            double segment_ms(size_t sample, size_t segment) const noexcept {
                TRIENGINE_ASSERT(segment < this->segment_count());
                return this->_at(sample, kSampleSegmentMsOffset + segment);
            }

            // Strided columns for ImPlot; valid only when not empty.
            const double* time_column() const noexcept { return this->_column(kSampleTimeSecondsOffset); }
            const double* frame_column() const noexcept { return this->_column(kSampleFrameMsOffset); }
            const double* fps_column() const noexcept { return this->_column(kSampleFpsOffset); }
            const double* segment_column(size_t segment) const noexcept {
                TRIENGINE_ASSERT(segment < this->segment_count());
                return this->_column(kSampleSegmentMsOffset + segment);
            }
            int stride_bytes() const noexcept { return static_cast<int>(_stride * sizeof(double)); }

        private:
            // Offsets within a sample: [time_seconds, frame_ms, fps, segment_ms...]
            static constexpr size_t kSampleTimeSecondsOffset = 0;
            static constexpr size_t kSampleFrameMsOffset = 1;
            static constexpr size_t kSampleFpsOffset = 2;
            static constexpr size_t kSampleSegmentMsOffset = 3;

            double _at(size_t sample, size_t offset) const noexcept {
                TRIENGINE_ASSERT(sample < _count);
                return _values[sample * _stride + offset];
            }
            const double* _column(size_t offset) const noexcept {
                TRIENGINE_ASSERT(_count > 0);
                return _values.data() + offset;
            }

            std::vector<double> _values;
            size_t _stride{ kSampleSegmentMsOffset };
            size_t _count{};
        };

        struct frame_statistics {
            size_t sample_count{};
            double latest_frame_ms{};
            double duration_seconds{};
            double average_fps{};
            double average_frame_ms{};
            double min_frame_ms{};
            double max_frame_ms{};
            double p95_frame_ms{};
            double p99_frame_ms{};
            std::vector<double> average_segment_ms;
            std::vector<double> max_segment_ms;
        };

        struct plot_y_axis {
            bool auto_fit;
            double minimum, maximum;
            double input_minimum, input_maximum;
            bool editing{ false };

            void render_axis_controls(const char* label);
            void setup_y_axis(const char* label);
            void set_limits(double lower, double upper) noexcept {
                minimum = input_minimum = lower;
                maximum = input_maximum = upper;
            }
        };

    public:
        frame_perf_window() = default;
        ~frame_perf_window() override = default;

        const char* get_window_name() const override { return "Frame Performance"; }
        ImVec2 get_initial_window_size() const override { return { 620.0f, 800.0f }; }
        void render(const window_render_context& render_ctx) override;

    private:
        void _refresh_samples(const utility::frame_profiler& profiler);
        void _refresh_statistics();
        void _fit_fps_axis();

        void _render_controls(visualization::visualizer& vis);
        bool _render_plot_options(float dpi_scale);
        void _render_statistics();
        void _render_fps_plot(float dpi_scale, uint32_t fps_cap);
        void _render_frame_time(const utility::frame_profiler& profiler, float dpi_scale, uint32_t fps_cap);

    private:
        // Frame control
        uint32_t _remembered_fps_cap{ visualization::visualizer::kDefaultMaxFps }; // Restored when the cap is re-enabled.

        // Samples and statistics, rebuilt on each refresh
        sample_buffer _samples;
        size_t _samples_epoch{}; // Profiler history epoch the samples were copied from.
        double _next_refresh_time{};
        frame_statistics _statistics;
        std::vector<double> _frame_time_scratch;

        // Plot view
        double _history_seconds{ 10.0 };
        plot_y_axis _fps_axis{ false, 0.0, 300.0, 0.0, 300.0 };
        bool _fps_axis_needs_fit{ true }; // Fit to the data for the first second, then leave it to the user.
    };
}
