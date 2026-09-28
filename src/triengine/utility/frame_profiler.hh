#pragma once
#include <triengine/utility/debug_utils.hh>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace triengine::utility
{
    // Render-thread ownership only. Stages must not nest; register them before profiling is first enabled.
    class frame_profiler
    {
    private:
        using clock = std::chrono::steady_clock;

        // Offsets within a record: [time_seconds, frame_ms, stage_ms...]
        static constexpr size_t kRecordTimeSecondsOffset = 0;
        static constexpr size_t kRecordFrameMsOffset = 1;
        static constexpr size_t kRecordStageMsOffset = 2;

    public:
        static constexpr size_t kRecordsCapacity = 32768;
        using stage_id_t = size_t;

        // Stages are identified by `stage_id_t`; `name` is the unique display name.
        struct stage_descriptor {
            std::string_view name;
            std::string_view description;
        };

        struct stage_info {
            std::string name;
            std::string description;
        };

        // Read-only view of one recorded frame. Valid until the next `end_frame()`.
        class record_view {
        public:
            // Seconds since the start of the current epoch.
            double time_seconds() const noexcept { return _values[kRecordTimeSecondsOffset]; }
            double frame_ms() const noexcept { return _values[kRecordFrameMsOffset]; }

            size_t stage_count() const noexcept { return _stage_count; }

            // `NaN` when the stage was not measured in this frame.
            double stage_ms(stage_id_t id) const noexcept {
                TRIENGINE_ASSERT(id < _stage_count);
                return _values[kRecordStageMsOffset + id];
            }

        private:
            friend class frame_profiler;

            record_view(const double* values, size_t stage_count) noexcept
                : _values{ values }, _stage_count{ stage_count }
            {}

            const double* _values;
            size_t _stage_count;
        };

        class scoped_stage_timer {
        public:
            scoped_stage_timer(frame_profiler& profiler, stage_id_t stage_id);
            ~scoped_stage_timer();
            scoped_stage_timer(const scoped_stage_timer&) = delete;
            scoped_stage_timer& operator=(const scoped_stage_timer&) = delete;
        private:
            frame_profiler* _profiler{};
            stage_id_t _stage_id{};
        };

    public:
        frame_profiler() = default;
        frame_profiler(const frame_profiler&) = delete;
        frame_profiler& operator=(const frame_profiler&) = delete;

        stage_id_t register_stage(const stage_descriptor& descriptor);
        const stage_info& get_stage(stage_id_t id) const;
        size_t stage_count() const noexcept { return _stages.size(); }

        // Requests take effect at the next `end_frame()`; `is_enabled()` reports the applied state.
        bool is_enabled() const noexcept { return _profiling_enabled; }
        void request_enabled(bool enabled) noexcept { _requested_enabled = enabled; }

        // Clears the history and starts a new epoch. Toggling the enabled state does too.
        void request_reset() noexcept { _reset_requested = true; }
        size_t history_epoch() const noexcept { return _history_epoch; }

        // Frame boundary: records the frame that just ended, then applies pending requests.
        void end_frame();

        // Oldest first, in time order.
        size_t record_count() const noexcept { return _record_count; }
        record_view get_record(size_t index) const;

        // Index of the first record newer than `time_seconds`; `record_count()` when there is none.
        size_t find_first_record_after(double time_seconds) const;

    private:
        struct active_stage {
            stage_id_t id;
            clock::time_point started_at;
        };

        // Only `scoped_stage_timer` measures, so a measurement never spans a frame boundary.
        void _begin_measure(stage_id_t id);
        void _end_measure(stage_id_t id);
        void _apply_enabled(bool enabled, clock::time_point now);
        void _clear(clock::time_point now) noexcept;

        // Values of the record at logical `index` (0 = oldest).
        const double* _record_at(size_t index) const noexcept;

        // Newest record's values, reusing the oldest slot when full; the caller must write every value.
        double* _acquire_record_slot() noexcept;

    private:
        // Stages
        std::vector<stage_info> _stages;
        bool _stage_registration_closed{};

        // Current frame
        std::optional<active_stage> _active_stage; // At most one, as stages do not nest.
        std::vector<std::optional<clock::duration>> _frame_stage_times; // Per stage; `nullopt` if not measured.
        bool _frame_open{}; // started while profiling was enabled
        clock::time_point _last_frame_boundary{};

        // State, and requests applied at the next `end_frame()`
        bool _profiling_enabled{};
        std::optional<bool> _requested_enabled;
        bool _reset_requested{};
        size_t _history_epoch{};

        // Ring of `kRecordsCapacity` records, `_values_per_record` doubles each; allocated once on first enable.
        std::vector<double> _record_storage;
        size_t _values_per_record{};
        size_t _oldest_record_slot{}; // Stays 0 until the ring is full.
        size_t _record_count{};
        clock::time_point _recording_started_at{};
    };
}
