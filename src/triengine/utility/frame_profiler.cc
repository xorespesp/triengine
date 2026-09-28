#include "frame_profiler.hh"
#include <triengine/utility/debug_utils.hh>

#include <limits>
#include <utility>

namespace triengine::utility
{
    frame_profiler::scoped_stage_timer::scoped_stage_timer(frame_profiler& profiler, stage_id_t stage_id)
    {
        if (!profiler.is_enabled()) { return; }
        profiler._begin_measure(stage_id);
        _profiler = &profiler;
        _stage_id = stage_id;
    }

    frame_profiler::scoped_stage_timer::~scoped_stage_timer()
    {
        // The enabled state only changes at `end_frame()`, which requires no open measurement.
        if (_profiler) { _profiler->_end_measure(_stage_id); }
    }

    frame_profiler::stage_id_t frame_profiler::register_stage(const stage_descriptor& descriptor)
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

    const frame_profiler::stage_info& frame_profiler::get_stage(stage_id_t id) const
    {
        if (id >= _stages.size()) { TRIENGINE_PANIC("Invalid frame profiler stage ID"); }
        return _stages[id];
    }

    void frame_profiler::_begin_measure(stage_id_t id)
    {
        if (id >= _stages.size()) { TRIENGINE_PANIC("Invalid frame profiler stage ID"); }
        if (_active_stage) { TRIENGINE_PANIC("Frame profiler stages must not nest"); }
        _active_stage = active_stage{ id, clock::now() };
    }

    void frame_profiler::_end_measure(stage_id_t id)
    {
        TRIENGINE_ASSERT(_active_stage && _active_stage->id == id);
        auto& stage_time = _frame_stage_times[id];
        stage_time = stage_time.value_or(clock::duration::zero()) + (clock::now() - _active_stage->started_at);
        _active_stage.reset();
    }

    void frame_profiler::end_frame()
    {
        if (_active_stage) { TRIENGINE_PANIC("End the frame after all measurements have ended"); }

        const auto now = clock::now();
        const bool toggles = _requested_enabled.has_value() && *_requested_enabled != _profiling_enabled;
        const bool discard = toggles || _reset_requested;

        if (_profiling_enabled && _frame_open && !discard) {
            double* const values = this->_acquire_record_slot();
            values[kRecordTimeSecondsOffset] = std::chrono::duration<double>(now - _recording_started_at).count();
            values[kRecordFrameMsOffset] = std::chrono::duration<double, std::milli>(now - _last_frame_boundary).count();
            for (stage_id_t id = 0; id < _frame_stage_times.size(); ++id) {
                const auto& stage_time = _frame_stage_times[id];
                values[kRecordStageMsOffset + id] = stage_time
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
        _frame_open = _profiling_enabled;
    }

    frame_profiler::record_view frame_profiler::get_record(size_t index) const
    {
        if (index >= _record_count) { TRIENGINE_PANIC("Invalid frame profiler record index"); }
        return record_view{ this->_record_at(index), _stages.size() };
    }

    size_t frame_profiler::find_first_record_after(double time_seconds) const
    {
        size_t first = 0, last = _record_count;
        while (first < last) {
            const size_t middle = first + (last - first) / 2;
            if (this->_record_at(middle)[kRecordTimeSecondsOffset] > time_seconds) {
                last = middle;
            } else {
                first = middle + 1;
            }
        }
        return first;
    }

    void frame_profiler::_apply_enabled(bool enabled, clock::time_point now)
    {
        if (_profiling_enabled == enabled) { return; }
        if (enabled && !_stage_registration_closed) {
            // Allocate before closing registration so a failed allocation leaves the profiler unchanged.
            const size_t values_per_record = kRecordStageMsOffset + _stages.size();
            _record_storage.resize(kRecordsCapacity * values_per_record);
            _values_per_record = values_per_record;
            _stage_registration_closed = true;
        }
        _profiling_enabled = enabled;
        this->_clear(now);
    }

    void frame_profiler::_clear(clock::time_point now) noexcept
    {
        _oldest_record_slot = 0;
        _record_count = 0;
        _recording_started_at = now;
        ++_history_epoch;
    }

    const double* frame_profiler::_record_at(size_t index) const noexcept
    {
        return _record_storage.data() + ((_oldest_record_slot + index) % kRecordsCapacity) * _values_per_record;
    }

    double* frame_profiler::_acquire_record_slot() noexcept
    {
        size_t slot;
        if (_record_count < kRecordsCapacity) {
            slot = _record_count++;
        } else {
            slot = _oldest_record_slot;
            _oldest_record_slot = (_oldest_record_slot + 1) % kRecordsCapacity;
        }
        return _record_storage.data() + slot * _values_per_record;
    }
}
