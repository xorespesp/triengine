#pragma once
#include <triengine/utility/debug_utils.hh>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace triengine::utility
{
    // Render-thread ownership only. Stages must not nest; register them before profiling is first enabled.
    class cpu_frame_profiler
    {
    private:
        using clock = std::chrono::steady_clock;

        // Per-frame values whose count does not depend on the registered stages.
        struct record_header {
            uint64_t frame_id;
            double time_seconds;
            double frame_ms;
        };

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

        // One recorded frame, read in place from the history. Invalidated by the next `end_frame()`.
        class record_view {
        public:
            // Frame number the host passed to `end_frame()` for this frame.
            uint64_t frame_id() const noexcept { return _ring_header->frame_id; }

            // Recording time of this frame, in seconds from the start of the current epoch.
            double time_seconds() const noexcept { return _ring_header->time_seconds; }

            // Time from the previous frame boundary to this one.
            double frame_ms() const noexcept { return _ring_header->frame_ms; }

            size_t stage_count() const noexcept { return _stage_count; }

            // Total time of the stage in this frame; `NaN` if it was not measured.
            double stage_ms(stage_id_t id) const noexcept {
                TRIENGINE_ASSERT(id < _stage_count);
                return _ring_stage_row[id];
            }

        private:
            friend class cpu_frame_profiler;

            record_view(const record_header* ring_header, const double* ring_stage_row, size_t stage_count) noexcept
                : _ring_header{ ring_header }, _ring_stage_row{ ring_stage_row }, _stage_count{ stage_count }
            {}

            // Point into the profiler's record ring, not copies.
            const record_header* _ring_header; // `_ring_headers[slot]`
            const double* _ring_stage_row; // `_ring_stage_ms` row of the slot, `stage_count` values
            size_t _stage_count;
        };

        class scoped_stage_timer {
        public:
            scoped_stage_timer(cpu_frame_profiler& profiler, stage_id_t stage_id);
            ~scoped_stage_timer();
            scoped_stage_timer(const scoped_stage_timer&) = delete;
            scoped_stage_timer& operator=(const scoped_stage_timer&) = delete;
        private:
            cpu_frame_profiler* _profiler{};
            stage_id_t _stage_id{};
        };

    public:
        cpu_frame_profiler() = default;
        cpu_frame_profiler(const cpu_frame_profiler&) = delete;
        cpu_frame_profiler& operator=(const cpu_frame_profiler&) = delete;

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
        // `frame_id` is the host's frame number; profilers stepped by the same host share it, so their records can be joined.
        void end_frame(uint64_t frame_id);

        // Oldest first, in time order.
        size_t record_count() const noexcept { return _ring_count; }
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

        // Ring slot holding the `index`-th record, counting from the oldest.
        size_t _ring_slot(size_t index) const noexcept;

        // Takes the slot for a new newest record, evicting the oldest record once the ring is full.
        // The caller fills in the header and every stage time of that slot.
        size_t _acquire_ring_slot() noexcept;

    private:
        // Stages
        std::vector<stage_info> _stages;
        bool _stage_registration_closed{};

        // Current frame
        std::optional<active_stage> _active_stage; // At most one, as stages do not nest.
        std::vector<std::optional<clock::duration>> _frame_stage_times; // Per stage; `nullopt` if not measured.
        clock::time_point _last_frame_boundary{};

        // State, and requests applied at the next `end_frame()`
        bool _profiling_enabled{};
        std::optional<bool> _requested_enabled;
        bool _reset_requested{};
        size_t _history_epoch{};

        // Record ring of `kRecordsCapacity` slots, allocated on first enable.
        // Slot `s` holds header `_ring_headers[s]` and stage `i` time `_ring_stage_ms[s * stage_count() + i]`.
        std::vector<record_header> _ring_headers;
        std::vector<double> _ring_stage_ms;
        size_t _ring_oldest_slot{}; // Stays 0 until the ring is full.
        size_t _ring_count{};
        clock::time_point _recording_started_at{};
    };
}
