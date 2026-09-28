#pragma once
#include <chrono>
#include <cstdint>
#include <memory>

namespace triengine::utility
{
    class frame_limiter
    {
    public:
        frame_limiter() noexcept;
        ~frame_limiter();
        frame_limiter(const frame_limiter&) = delete;
        frame_limiter& operator=(const frame_limiter&) = delete;

        std::uint32_t get_max_fps() const noexcept { return _max_fps; }
        void set_max_fps(std::uint32_t max_fps);
        void reset_schedule() noexcept { _deadline = {}; }
        void wait();

    private:
        struct platform_timer;
        std::unique_ptr<platform_timer> _timer;
        std::uint32_t _max_fps{};
        std::chrono::nanoseconds _interval{};
        std::chrono::steady_clock::time_point _deadline{};
    };
}
