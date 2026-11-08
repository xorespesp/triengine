#include "cpu_frame_profiler.hh"
#include <triengine/utility/debug_utils.hh>

#include <limits>
#include <utility>

namespace triengine::utility
{
    cpu_frame_profiler::scoped_stage_timer::scoped_stage_timer(cpu_frame_profiler& profiler, stage_id_t stage_id)
    {
        if (!profiler.is_enabled()) { return; }
        profiler._begin_measure(stage_id);
        _profiler = &profiler;
        _stage_id = stage_id;
    }

    cpu_frame_profiler::scoped_stage_timer::~scoped_stage_timer()
    {
        // The enabled state only changes at `end_frame()`, which requires no open measurement.
        if (_profiler) { _profiler->_end_measure(_stage_id); }
    }

    cpu_frame_profiler::stage_id_t cpu_frame_profiler::register_stage(const stage_descriptor& descriptor)
    {
        if (_stage_registration_closed) {
            TRIENGINE_PANIC("Register stages before enabling profiling");
        }

        if (descriptor.name.empty()) {
            TRIENGINE_PANIC("Empty frame profiler stage name");
        }

        for (const auto& stage : _stages) {
            if (stage.name == descriptor.name) { TRIENGINE_PANIC("Duplicate frame profiler stage name"); }
        }

        stage_info stage;
        stage.name = descriptor.name;
        stage.description = descriptor.description;
        _stages.push_back(std::move(stage));
        _frame_stage_times.emplace_back();
        return _stages.size() - 1;
    }

    const cpu_frame_profiler::stage_info& cpu_frame_profiler::get_stage(stage_id_t id) const
    {
        if (id >= _stages.size()) { TRIENGINE_PANIC("Invalid frame profiler stage ID"); }
        return _stages[id];
    }

    void cpu_frame_profiler::_begin_measure(stage_id_t id)
    {
        if (id >= _stages.size()) { TRIENGINE_PANIC("Invalid frame profiler stage ID"); }
        if (_active_stage) { TRIENGINE_PANIC("Frame profiler stages must not nest"); }
        _active_stage = active_stage{ id, clock::now() };
    }

    void cpu_frame_profiler::_end_measure(stage_id_t id)
    {
        TRIENGINE_ASSERT(_active_stage && _active_stage->id == id);
        auto& stage_time = _frame_stage_times[id];
        stage_time = stage_time.value_or(clock::duration::zero()) + (clock::now() - _active_stage->started_at);
        _active_stage.reset();
    }

    void cpu_frame_profiler::end_frame(const uint64_t frame_id)
    {
        if (_active_stage) { TRIENGINE_PANIC("End the frame after all measurements have ended"); }

        const auto now = clock::now();
        const bool toggles = _requested_enabled.has_value() && *_requested_enabled != _profiling_enabled;
        const bool discard = toggles || _reset_requested;

        // The enabled state only changes at a boundary, so an enabled profiler timed this whole frame.
        if (_profiling_enabled && !discard)
        {
            const size_t slot = this->_acquire_ring_slot();

            record_header& header = _ring_headers[slot];
            header.frame_id = frame_id;
            header.time_seconds = std::chrono::duration<double>(now - _recording_started_at).count();
            header.frame_ms = std::chrono::duration<double, std::milli>(now - _last_frame_boundary).count();

            double* const stage_ms = _ring_stage_ms.data() + slot * _stages.size();
            for (stage_id_t id = 0; id < _frame_stage_times.size(); ++id) {
                const auto& stage_time = _frame_stage_times[id];
                stage_ms[id] = stage_time
                    ? std::chrono::duration<double, std::milli>(*stage_time).count()
                    : std::numeric_limits<double>::quiet_NaN();
            }
        }

        for (auto& stage_time : _frame_stage_times) { stage_time.reset(); }

        if (_requested_enabled) {
            this->_apply_enabled(*_requested_enabled, now);
            _requested_enabled.reset();
        }
        if (_reset_requested) {
            this->_clear(now);
            _reset_requested = false;
        }

        _last_frame_boundary = now;
    }

    cpu_frame_profiler::record_view cpu_frame_profiler::get_record(size_t index) const
    {
        if (index >= _ring_count) { TRIENGINE_PANIC("Invalid frame profiler record index"); }
        const size_t slot = this->_ring_slot(index);
        return record_view{ &_ring_headers[slot], _ring_stage_ms.data() + slot * _stages.size(), _stages.size() };
    }

    size_t cpu_frame_profiler::find_first_record_after(double time_seconds) const
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

    void cpu_frame_profiler::_apply_enabled(bool enabled, clock::time_point now)
    {
        if (_profiling_enabled == enabled) { return; }
        if (enabled && !_stage_registration_closed) {
            // The stage count fixes the row width, so it must not change once the history exists.
            // Allocating first means a throwing allocation leaves registration open and profiling off.
            _ring_headers.resize(kRecordsCapacity);
            _ring_stage_ms.resize(kRecordsCapacity * _stages.size());
            _stage_registration_closed = true;
        }
        _profiling_enabled = enabled;
        this->_clear(now);
    }

    void cpu_frame_profiler::_clear(clock::time_point now) noexcept
    {
        _ring_oldest_slot = 0;
        _ring_count = 0;
        _recording_started_at = now;
        ++_history_epoch;
    }

    size_t cpu_frame_profiler::_ring_slot(size_t index) const noexcept
    {
        return (_ring_oldest_slot + index) % kRecordsCapacity;
    }

    size_t cpu_frame_profiler::_acquire_ring_slot() noexcept
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
