#pragma once
#include <functional>
#include <optional>
#include <mutex>

namespace triengine::utility
{
    /// @brief Thread-safe class to report progress through an optional callback function, converting counts to percentages.
    ///
    /// This class takes a non-zero total value (`uint64_t`) during construction.
    /// An optional callback (`std::function<bool(float)>`) can be set via `set_progress_cb`.
    /// The callback accepts a percentage (0.0f to 100.0f) and returns `true` to continue
    /// or `false` to abort the tracked operation.
    ///
    /// The class handles the conversion from absolute current values (`uint64_t`) to percentages (`float`).
    /// Internal operations are protected by a mutex for thread safety when accessed from multiple threads.
    /// Importantly, the progress callback is invoked *after* the internal lock is released
    /// to prevent potential deadlocks or performance issues caused by long-running callbacks.
    ///
    /// @note Construction with a `total_value` of zero is disallowed and will throw `std::invalid_argument`.
    class progress_reporter
    {
    public:
        using value_type = uint64_t;

    public:
        explicit progress_reporter(value_type total_value) 
            : _total_value{ total_value }
        {
            if (!_total_value) {
                throw std::invalid_argument{ "Total value cannot be zero." };
            }
        }

        value_type get_total_value() const {
            std::scoped_lock lk{ _mtx };
            return _total_value;
        }

        value_type get_current_value() const {
            std::scoped_lock lk{ _mtx };
            return _current_value;
        }

        void set_progress_cb(std::function<bool(float)> progress_cb) {
            std::scoped_lock lk{ _mtx };
            _progress_cb = std::move(progress_cb);
        }

        bool update(value_type current_value) {
            float percent{ 0.0f };
            std::function<bool(float)> cb_copy; {
                std::scoped_lock lk{ _mtx };
                _current_value = current_value;
                if (_progress_cb) {
                    cb_copy = _progress_cb;
                    percent = (current_value < _total_value)
                        ? static_cast<float>(static_cast<double>(current_value) * 100.0 / _total_value)
                        : 100.0f;
                }
            }

            if (cb_copy) {
                return cb_copy(percent);
            }
        
            return true;
        }

        void finish() {
            std::function<bool(float)> cb_copy; {
                std::scoped_lock lk{ _mtx };
                _current_value = _total_value;
                if (_progress_cb) {
                    cb_copy = _progress_cb;
                }
            }
            if (cb_copy) {
                cb_copy(100.0f);
            }
        }

    private:
        std::function<bool(float)> _progress_cb;
        const value_type _total_value{};
        value_type _current_value{};
        mutable std::mutex _mtx;
    };
    
} // namespace