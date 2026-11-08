#pragma once
#include <triengine/common.h>
#include <triengine/utility/cpu_frame_profiler.hh>

#include <glad/gl.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace triengine::core
{
    // Per-frame GPU time of registered stages, measured with `GL_TIMESTAMP` queries. Render thread only.
    //   - Register stages, then `create()`; `create()` and `destroy()` need the GL context current.
    //   - Time a stage with `scoped_stage_timer`: no nesting, at most once per frame (a repeat is ignored and counted).
    //   - `end_frame()` marks the frame boundary.
    //   - A frame is recorded once its results arrive, a few frames later. Reads never wait on the GPU.
    class gpu_frame_profiler
    {
    public:
        static constexpr size_t kRecordsCapacity = 32768;

        // Frames whose results can be pending at once. A frame still pending when its slot is
        // needed again is dropped, which only happens if the GPU runs this many frames behind.
        static constexpr size_t kInflightFrames = 4;

        using stage_id_t = size_t;

        // Same types as `utility::cpu_frame_profiler`, so one descriptor list registers on both.
        using stage_descriptor = utility::cpu_frame_profiler::stage_descriptor;
        using stage_info = utility::cpu_frame_profiler::stage_info;

    private:
        // Per-frame values whose count does not depend on the registered stages.
        struct record_header {
            uint64_t frame_id;
            double time_seconds;
            double frame_ms;
        };

    public:
        // One recorded frame, read in place from the history. Invalidated by the next `end_frame()`.
        class record_view {
        public:
            // Frame number the host passed to `end_frame()` for this frame.
            uint64_t frame_id() const noexcept { return _ring_header->frame_id; }

            // End of this frame on the GPU clock, in seconds from the start of the epoch's first recorded frame.
            double time_seconds() const noexcept { return _ring_header->time_seconds; }

            // GPU time from the previous frame boundary to this one.
            double frame_ms() const noexcept { return _ring_header->frame_ms; }

            size_t stage_count() const noexcept { return _stage_count; }

            // GPU time of the stage in this frame; `NaN` if it was not measured.
            double stage_ms(stage_id_t id) const noexcept {
                TRIENGINE_ASSERT(id < _stage_count);
                return _ring_stage_row[id];
            }

        private:
            friend class gpu_frame_profiler;

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
            scoped_stage_timer(gpu_frame_profiler& profiler, stage_id_t stage_id);
            ~scoped_stage_timer();
            scoped_stage_timer(const scoped_stage_timer&) = delete;
            scoped_stage_timer& operator=(const scoped_stage_timer&) = delete;
        private:
            gpu_frame_profiler* _profiler{};
            stage_id_t _stage_id{};
        };

    public:
        gpu_frame_profiler() = default;
        ~gpu_frame_profiler();
        gpu_frame_profiler(const gpu_frame_profiler&) = delete;
        gpu_frame_profiler& operator=(const gpu_frame_profiler&) = delete;

        stage_id_t register_stage(const stage_descriptor& descriptor);
        const stage_info& get_stage(stage_id_t id) const;
        size_t stage_count() const noexcept { return _stages.size(); }

        // Creates the queries for the registered stages and closes registration.
        void create();
        void destroy();
        bool is_valid() const noexcept { return !_query_ids.empty(); }

        // Requests take effect at the next `end_frame()`; `is_enabled()` reports the applied state.
        bool is_enabled() const noexcept { return _profiling_enabled; }
        void request_enabled(bool enabled) noexcept { _requested_enabled = enabled; }

        // Clears the history and starts a new epoch. Toggling the enabled state does too.
        // Frames still pending at that point are discarded.
        void request_reset() noexcept { _reset_requested = true; }
        size_t history_epoch() const noexcept { return _history_epoch; }

        // Frame boundary: ends the current frame, records the frames whose results have arrived,
        // then applies pending requests. `frame_id` is the host's frame number, used to join records.
        void end_frame(uint64_t frame_id);

        // Oldest first, in time order.
        size_t record_count() const noexcept { return _ring_count; }
        record_view get_record(size_t index) const;

        // Index of the first record newer than `time_seconds`; `record_count()` when there is none.
        size_t find_first_record_after(double time_seconds) const;

        // Totals since `create()`.
        size_t dropped_frame_count() const noexcept { return _dropped_frame_count; }
        size_t ignored_measurement_count() const noexcept { return _ignored_measurement_count; }

    private:
        // Queries of one frame: `[begin boundary, end boundary, stage 0 begin, stage 0 end, ...]`.
        struct inflight_frame {
            uint64_t frame_id{};
            bool pending{}; // ended and waiting for its results
            std::vector<bool> stage_measured; // per stage
        };

        // Only `scoped_stage_timer` measures, so a measurement never spans a frame boundary.
        // Returns false when the measurement is ignored.
        bool _begin_measure(stage_id_t id);
        void _end_measure(stage_id_t id);

        void _open_frame();
        void _record_arrived_frames();
        bool _try_record(inflight_frame& frame, size_t slot);
        void _discard_pending_frames() noexcept;
        void _apply_enabled(bool enabled);
        void _clear() noexcept;

        GLuint _query_id(size_t inflight_slot, size_t query_index) const noexcept;

        // Ring slot holding the `index`-th record, counting from the oldest.
        size_t _ring_slot(size_t index) const noexcept;

        // Takes the slot for a new newest record, evicting the oldest record once the ring is full.
        // The caller fills in the header and every stage time of that slot.
        size_t _acquire_ring_slot() noexcept;

    private:
        // Stages
        std::vector<stage_info> _stages;
        bool _stage_registration_closed{};

        // Queries of the in-flight frames, `2 + 2 * stage_count()` per frame
        std::vector<GLuint> _query_ids;
        inflight_frame _inflight_frames[kInflightFrames];
        size_t _current_inflight_slot{}; // The open frame's slot; the slots after it hold older frames.

        // Current frame
        std::optional<stage_id_t> _active_stage; // At most one, as stages do not nest.

        // State, and requests applied at the next `end_frame()`
        bool _profiling_enabled{};
        std::optional<bool> _requested_enabled;
        bool _reset_requested{};
        size_t _history_epoch{};
        std::optional<GLuint64> _epoch_start_ns; // begin boundary of the epoch's first recorded frame

        // Record ring of `kRecordsCapacity` slots, allocated on first enable.
        // Slot `s` holds header `_ring_headers[s]` and stage `i` time `_ring_stage_ms[s * stage_count() + i]`.
        std::vector<record_header> _ring_headers;
        std::vector<double> _ring_stage_ms;
        size_t _ring_oldest_slot{}; // Stays 0 until the ring is full.
        size_t _ring_count{};

        size_t _dropped_frame_count{};
        size_t _ignored_measurement_count{};
    };
}
