#if defined(_WIN32)
#  include <windows.h>
#  include <timeapi.h>
#endif
#include "frame_limiter.hh"
#include <triengine/utility/debug_utils.hh>
#include <algorithm>
#include <system_error>
#include <thread>

namespace triengine::utility
{
    struct frame_limiter::platform_timer
    {
#if defined(_WIN32)
        HANDLE handle{};
        bool raised_timer_resolution{ false };
        platform_timer() {
            handle = ::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                TIMER_MODIFY_STATE | SYNCHRONIZE);
            if (!handle) {
                // High-resolution timers need Windows 10 1803+. A regular timer fires on the system tick
                // (15.6 ms by default), coarser than any capped interval, so raise the tick to 1 ms while it lives.
                raised_timer_resolution = ::timeBeginPeriod(1) == TIMERR_NOERROR;
                handle = ::CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_MODIFY_STATE | SYNCHRONIZE);
            }
            if (!handle) {
                const DWORD error = ::GetLastError();
                if (raised_timer_resolution) { ::timeEndPeriod(1); }
                throw std::system_error(static_cast<int>(error), std::system_category(), "CreateWaitableTimerExW");
            }
        }
        ~platform_timer() {
            ::CloseHandle(handle);
            if (raised_timer_resolution) { ::timeEndPeriod(1); }
        }
#endif
    };

    frame_limiter::frame_limiter() noexcept = default;
    frame_limiter::~frame_limiter() = default;

    void frame_limiter::set_max_fps(uint32_t max_fps)
    {
        if (_max_fps == max_fps) { return; }
        if (max_fps == 0) {
            _timer.reset();
            _interval = {};
            _deadline = {};
        } else {
#if defined(_WIN32)
            if (!_timer) { _timer = std::make_unique<platform_timer>(); }
#endif
            _interval = std::chrono::nanoseconds{ (1'000'000'000ull + max_fps - 1) / max_fps };
            _deadline = std::chrono::steady_clock::now() + _interval;
        }
        _max_fps = max_fps;

        if (max_fps > 0) {
            TRIENGINE_DEBUG("Frame rate cap set to %u FPS", max_fps);
        } else {
            TRIENGINE_DEBUG("Frame rate cap disabled");
        }
    }

    void frame_limiter::wait()
    {
        if (_interval.count() == 0) {
            return;
        }

        using clock = std::chrono::steady_clock;
        auto now = clock::now();
        if (_deadline == clock::time_point{} ||
            now >= _deadline + _interval)
        {
            // Start immediately, or discard missed slots after a slow frame or a stall.
            _deadline = now + _interval;
            return;
        }

        // Sleep for most of the interval; reserve at most 0.5 ms (and at most 1/8 of the interval) 
        // to absorb scheduler wake-up latency with a short active wait.
        const auto spin_budget = std::min(
            _interval / 8, std::chrono::nanoseconds{ 500'000 });
        while (now < _deadline)
        {
            const auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>(
                _deadline - now);
            if (remaining > spin_budget)
            {
#if defined(_WIN32)
                LARGE_INTEGER due_time;
                // Relative deadlines are negative and use 100 ns units. Round up.
                due_time.QuadPart = -((remaining.count() - spin_budget.count() + 99) / 100);
                if (!::SetWaitableTimer(_timer->handle, &due_time, 0, nullptr, nullptr, FALSE)) {
                    throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "SetWaitableTimer");
                }
                if (::WaitForSingleObject(_timer->handle, INFINITE) != WAIT_OBJECT_0) {
                    throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "WaitForSingleObject");
                }
#else
                std::this_thread::sleep_until(_deadline - spin_budget);
#endif
            }
            else
            {
#if defined(_WIN32)
                ::YieldProcessor();
#else
                std::this_thread::yield();
#endif
            }
            now = clock::now();
        }

        // Keep the original cadence so small oversleeps do not accumulate each frame.
        // If a wait itself stalled for a whole interval, discard the missed slots.
        _deadline += _interval;
        if (_deadline <= now) {
            _deadline = now + _interval;
        }
    }

}
