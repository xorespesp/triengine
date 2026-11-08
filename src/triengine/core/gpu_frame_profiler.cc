#include "gpu_frame_profiler.hh"
#include <triengine/utility/gl_utils.hh>
#include <triengine/utility/debug_utils.hh>

#include <algorithm>
#include <limits>
#include <utility>

namespace triengine::core
{
    namespace
    {
        // Query layout of an in-flight frame
        constexpr size_t kBeginBoundaryQuery = 0;
        constexpr size_t kEndBoundaryQuery = 1;
        constexpr size_t kFirstStageQuery = 2;

        constexpr size_t stage_begin_query(const size_t stage) noexcept { return kFirstStageQuery + 2 * stage; }
        constexpr size_t stage_end_query(const size_t stage) noexcept { return kFirstStageQuery + 2 * stage + 1; }
        constexpr size_t queries_per_frame(const size_t stage_count) noexcept { return kFirstStageQuery + 2 * stage_count; }

        double elapsed_ms(const GLuint64 begin_ns, const GLuint64 end_ns) noexcept {
            return static_cast<double>(end_ns - begin_ns) * 1.0e-6;
        }
    }

    gpu_frame_profiler::scoped_stage_timer::scoped_stage_timer(gpu_frame_profiler& profiler, stage_id_t stage_id)
    {
        if (!profiler._begin_measure(stage_id)) { return; }
        _profiler = &profiler;
        _stage_id = stage_id;
    }

    gpu_frame_profiler::scoped_stage_timer::~scoped_stage_timer()
    {
        // The enabled state only changes at `end_frame()`, which requires no open measurement.
        if (_profiler) { _profiler->_end_measure(_stage_id); }
    }

    gpu_frame_profiler::~gpu_frame_profiler()
    {
        if (this->is_valid()) {
            this->destroy();
        }
    }

    gpu_frame_profiler::stage_id_t gpu_frame_profiler::register_stage(const stage_descriptor& descriptor)
    {
        if (_stage_registration_closed) {
            TRIENGINE_PANIC("Register GPU frame profiler stages before create()");
        }

        if (descriptor.name.empty()) {
            TRIENGINE_PANIC("Empty GPU frame profiler stage name");
        }

        for (const auto& stage : _stages) {
            if (stage.name == descriptor.name) { TRIENGINE_PANIC("Duplicate GPU frame profiler stage name"); }
        }

        stage_info stage;
        stage.name = descriptor.name;
        stage.description = descriptor.description;
        _stages.push_back(std::move(stage));
        return _stages.size() - 1;
    }

    const gpu_frame_profiler::stage_info& gpu_frame_profiler::get_stage(stage_id_t id) const
    {
        if (id >= _stages.size()) { TRIENGINE_PANIC("Invalid GPU frame profiler stage ID"); }
        return _stages[id];
    }

    void gpu_frame_profiler::create()
    {
        TRIENGINE_ASSERT(!this->is_valid());

        // The query count depends on the stage count, so the stages are final from here on.
        _stage_registration_closed = true;

        _query_ids.resize(kInflightFrames * queries_per_frame(_stages.size()));
        GLCall(::glCreateQueries(GL_TIMESTAMP, static_cast<GLsizei>(_query_ids.size()), _query_ids.data()));

        for (inflight_frame& frame : _inflight_frames) {
            frame = inflight_frame{};
            frame.stage_measured.assign(_stages.size(), false);
        }
        _current_inflight_slot = 0;
    }

    void gpu_frame_profiler::destroy()
    {
        TRIENGINE_ASSERT(this->is_valid());
        if (_active_stage) { TRIENGINE_PANIC("Destroy the GPU frame profiler after all measurements have ended"); }

        GLCall(::glDeleteQueries(static_cast<GLsizei>(_query_ids.size()), _query_ids.data()));
        _query_ids.clear();

        // A later `create()` starts disabled, with an empty history.
        _profiling_enabled = false;
        _requested_enabled.reset();
        _reset_requested = false;
        this->_discard_pending_frames();
        this->_clear();
    }

    bool gpu_frame_profiler::_begin_measure(stage_id_t id)
    {
        if (id >= _stages.size()) { TRIENGINE_PANIC("Invalid GPU frame profiler stage ID"); }
        if (!_profiling_enabled) { return false; }
        if (_active_stage) { TRIENGINE_PANIC("GPU frame profiler stages must not nest"); }

        inflight_frame& frame = _inflight_frames[_current_inflight_slot];
        TRIENGINE_ASSERT(!frame.stage_measured[id]); // A stage is measured at most once per frame.
        if (frame.stage_measured[id]) {
            ++_ignored_measurement_count;
            return false;
        }

        // Non-blocking: queues a command; the GPU writes its clock into the query when it reaches it.
        GLCall(::glQueryCounter(this->_query_id(_current_inflight_slot, stage_begin_query(id)), GL_TIMESTAMP));
        _active_stage = id;
        return true;
    }

    void gpu_frame_profiler::_end_measure(stage_id_t id)
    {
        TRIENGINE_ASSERT(_active_stage && *_active_stage == id);
        GLCall(::glQueryCounter(this->_query_id(_current_inflight_slot, stage_end_query(id)), GL_TIMESTAMP));
        _inflight_frames[_current_inflight_slot].stage_measured[id] = true;
        _active_stage.reset();
    }

    void gpu_frame_profiler::end_frame(const uint64_t frame_id)
    {
        if (!this->is_valid()) { TRIENGINE_PANIC("Create the GPU frame profiler before ending a frame"); }
        if (_active_stage) { TRIENGINE_PANIC("End the frame after all measurements have ended"); }

        const bool toggles = _requested_enabled.has_value() && *_requested_enabled != _profiling_enabled;
        const bool discard = toggles || _reset_requested;

        // The enabled state only changes at a boundary, so an enabled profiler opened this frame.
        if (_profiling_enabled) {
            inflight_frame& frame = _inflight_frames[_current_inflight_slot];
            GLCall(::glQueryCounter(this->_query_id(_current_inflight_slot, kEndBoundaryQuery), GL_TIMESTAMP));
            frame.frame_id = frame_id;
            frame.pending = !discard;
            _current_inflight_slot = (_current_inflight_slot + 1) % kInflightFrames;
        }

        this->_record_arrived_frames();

        if (_requested_enabled) {
            this->_apply_enabled(*_requested_enabled);
            _requested_enabled.reset();
        }
        if (_reset_requested) {
            this->_discard_pending_frames();
            this->_clear();
            _reset_requested = false;
        }

        if (_profiling_enabled) {
            this->_open_frame();
        }
    }

    void gpu_frame_profiler::_open_frame()
    {
        inflight_frame& frame = _inflight_frames[_current_inflight_slot];
        if (frame.pending) {
            // Its results did not arrive within `kInflightFrames` frames.
            frame.pending = false;
            ++_dropped_frame_count;
        }
        std::fill(frame.stage_measured.begin(), frame.stage_measured.end(), false);
        GLCall(::glQueryCounter(this->_query_id(_current_inflight_slot, kBeginBoundaryQuery), GL_TIMESTAMP));
    }

    void gpu_frame_profiler::_record_arrived_frames()
    {
        // Oldest first, starting from the slot after the last ended frame. The GPU finishes frames
        // in order, so a frame whose results have not arrived means the newer ones have not either.
        for (size_t i = 0; i < kInflightFrames; ++i) {
            const size_t slot = (_current_inflight_slot + i) % kInflightFrames;
            inflight_frame& frame = _inflight_frames[slot];
            if (!frame.pending) {
                continue;
            }
            if (!this->_try_record(frame, slot)) {
                break;
            }
        }
    }

    bool gpu_frame_profiler::_try_record(inflight_frame& frame, const size_t slot)
    {
        const auto is_available = [this, slot](const size_t query_index) {
            GLint available{ GL_FALSE };
            // Non-blocking: reports whether the GPU has written the result yet.
            GLCall(::glGetQueryObjectiv(this->_query_id(slot, query_index), GL_QUERY_RESULT_AVAILABLE, &available));
            return available != GL_FALSE;
        };
        const auto read_ns = [this, slot](const size_t query_index) {
            GLuint64 ns{};
            // Blocking until the result arrives; only called once `is_available()` holds, so it returns at once.
            GLCall(::glGetQueryObjectui64v(this->_query_id(slot, query_index), GL_QUERY_RESULT, &ns));
            return ns;
        };

        // Only reading results that have arrived keeps the read from waiting on the GPU.
        if (!is_available(kBeginBoundaryQuery) || !is_available(kEndBoundaryQuery)) {
            return false;
        }
        for (stage_id_t id = 0; id < _stages.size(); ++id) {
            if (frame.stage_measured[id] && (!is_available(stage_begin_query(id)) || !is_available(stage_end_query(id)))) {
                return false;
            }
        }

        TRIENGINE_ASSERT(!_ring_headers.empty()); // Frames are only pending while enabled, which allocates the ring.
        const GLuint64 begin_ns = read_ns(kBeginBoundaryQuery);
        const GLuint64 end_ns = read_ns(kEndBoundaryQuery);
        if (!_epoch_start_ns) {
            _epoch_start_ns = begin_ns;
        }

        const size_t ring_slot = this->_acquire_ring_slot();

        record_header& header = _ring_headers[ring_slot];
        header.frame_id = frame.frame_id;
        header.time_seconds = static_cast<double>(end_ns - *_epoch_start_ns) * 1.0e-9;
        header.frame_ms = elapsed_ms(begin_ns, end_ns);

        double* const stage_ms = _ring_stage_ms.data() + ring_slot * _stages.size();
        for (stage_id_t id = 0; id < _stages.size(); ++id) {
            stage_ms[id] = frame.stage_measured[id]
                ? elapsed_ms(read_ns(stage_begin_query(id)), read_ns(stage_end_query(id)))
                : std::numeric_limits<double>::quiet_NaN();
        }

        frame.pending = false;
        return true;
    }

    void gpu_frame_profiler::_discard_pending_frames() noexcept
    {
        for (inflight_frame& frame : _inflight_frames) {
            frame.pending = false;
        }
    }

    gpu_frame_profiler::record_view gpu_frame_profiler::get_record(size_t index) const
    {
        if (index >= _ring_count) { TRIENGINE_PANIC("Invalid GPU frame profiler record index"); }
        const size_t slot = this->_ring_slot(index);
        return record_view{ &_ring_headers[slot], _ring_stage_ms.data() + slot * _stages.size(), _stages.size() };
    }

    size_t gpu_frame_profiler::find_first_record_after(double time_seconds) const
    {
        size_t first = 0, last = _ring_count;
        while (first < last) {
            const size_t middle = first + (last - first) / 2;
            if (_ring_headers[this->_ring_slot(middle)].time_seconds > time_seconds) {
                last = middle;
            } else {
                first = middle + 1;
            }
        }
        return first;
    }

    void gpu_frame_profiler::_apply_enabled(bool enabled)
    {
        if (_profiling_enabled == enabled) { return; }
        if (enabled && _ring_headers.empty()) {
            _ring_headers.resize(kRecordsCapacity);
            _ring_stage_ms.resize(kRecordsCapacity * _stages.size());
        }
        _profiling_enabled = enabled;
        this->_discard_pending_frames();
        this->_clear();
    }

    void gpu_frame_profiler::_clear() noexcept
    {
        _ring_oldest_slot = 0;
        _ring_count = 0;
        _epoch_start_ns.reset();
        ++_history_epoch;
    }

    GLuint gpu_frame_profiler::_query_id(const size_t inflight_slot, const size_t query_index) const noexcept
    {
        return _query_ids[inflight_slot * queries_per_frame(_stages.size()) + query_index];
    }

    size_t gpu_frame_profiler::_ring_slot(size_t index) const noexcept
    {
        return (_ring_oldest_slot + index) % kRecordsCapacity;
    }

    size_t gpu_frame_profiler::_acquire_ring_slot() noexcept
    {
        size_t slot;
        if (_ring_count < kRecordsCapacity) {
            slot = _ring_count++;
        } else {
            slot = _ring_oldest_slot;
            _ring_oldest_slot = (_ring_oldest_slot + 1) % kRecordsCapacity;
        }
        return slot;
    }
}
