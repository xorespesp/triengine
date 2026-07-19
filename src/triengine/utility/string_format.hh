#pragma once
#include <cstdio>
#include <utility>
#include <string>
#include <string_view>
#include <algorithm>

namespace triengine::utility::string
{
    template <typename... Args>
    static inline std::string c_format(
        const char* const c_fmt,
        Args&&... args)
    {
        if (!c_fmt) {
            throw std::invalid_argument{ "c-format string cannot be null" };
        }

        const int needed_sz = std::snprintf(nullptr, 0, c_fmt, std::forward<Args>(args)...);
        if (needed_sz < 0) {
            throw std::runtime_error{ "Failed to determine c-format string buffer size" };
        }

        std::string result(static_cast<size_t>(needed_sz + 1/* null terminator */), '\0');
        const int written_sz = std::snprintf(result.data(), result.size(), c_fmt, std::forward<Args>(args)...);
        if (written_sz < 0 || written_sz > needed_sz) {
            throw std::runtime_error{ "Failed to write c-format string to buffer" };
        }
        result.pop_back(); // trim null terminator

        return result;
    }

    template <typename... Args>
    static inline std::string c_format(
        const std::string& c_fmt,
        Args&&... args)
    {
        return c_format(c_fmt.c_str(), std::forward<Args>(args)...);
    }

    /// lightweight (c-style) format string builder
    template <size_t N>
    class format_string_builder {
    private:
        std::array<char, N> _buff{ '\0' };
        size_t _curr_pos{ 0 };

        static_assert(N > 1, "!!");

    public:
        format_string_builder() = default;

        size_t max_size() const noexcept {
            return _buff.size();
        }

        size_t size() const noexcept {
            return _curr_pos;
        }
        
        bool empty() const noexcept {
            return _curr_pos == 0;
        }

        std::string_view view() const noexcept {
            return std::string_view{ _buff.data(), _curr_pos };
        }

        const char* c_str() const noexcept {
            return _buff.data();
        }

        size_t append(std::string_view sv) noexcept
        {
            const size_t written = std::min(sv.size(), /* available */_buff.size() - _curr_pos - 1/* '\0' */);
            std::memcpy(_buff.data() + _curr_pos, sv.data(), written);
            _curr_pos += written;
            _buff[_curr_pos] = '\0'; // make sure null-terminated
            return written;
        }

        template <typename... _Args>
        size_t appendf(const char* const c_fmt, _Args&&... args) noexcept
        {
            const int written = std::snprintf(
                _buff.data() + _curr_pos,
                _buff.size() - _curr_pos,
                c_fmt,
                std::forward<_Args>(args)...
            );

            if (written <= 0) {
                return 0;
            }

            _curr_pos = std::clamp<size_t>(_curr_pos + static_cast<size_t>(written), 0, _buff.size() - 1/* '\0' */);
            _buff[_curr_pos] = '\0'; // make sure null-terminated
            return static_cast<size_t>(written);
        }

        void clear() noexcept {
            _curr_pos = 0;
            _buff[_curr_pos] = '\0'; // make sure null-terminated
        }
    };

} // namespace