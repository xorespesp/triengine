#pragma once
#include <cxlib/cxlib_defs.h>

#if defined(_CXLIB_HAS_FMT)
#  include <fmt/format.h>
#elif defined(_CXLIB_HAS_SPDLOG) // ^^^ _CXLIB_HAS_FMT ^^^ / vvv _CXLIB_HAS_SPDLOG vvv
#  include <spdlog/fmt/fmt.h>
#endif // ^^^ _CXLIB_HAS_SPDLOG ^^^

#include <cstdio>
#include <cwchar>
#include <utility>
#include <string>
#include <string_view>
#include <algorithm>
#include <stdexcept>

_CXLIB_NAMESPACE_BEGIN
namespace utils::string
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

    template <typename... Args>
    static inline std::wstring c_wformat(
        const wchar_t* const c_fmt,
        Args&&... args)
    {
        if (!c_fmt) {
            throw std::invalid_argument{ "c-format string cannot be null" };
        }

        const int needed_sz = std::swprintf(nullptr, 0, c_fmt, std::forward<Args>(args)...);
        if (needed_sz < 0) {
            throw std::runtime_error{ "Failed to determine c-format string buffer size" };
        }

        std::wstring result(static_cast<size_t>(needed_sz + 1/* null terminator */), L'\0');
        const int written_sz = std::swprintf(result.data(), result.size(), c_fmt, std::forward<Args>(args)...);
        if (written_sz < 0 || written_sz > needed_sz) {
            throw std::runtime_error{ "Failed to write c-format string to buffer" };
        }
        result.pop_back(); // trim null terminator

        return result;
    }

    template <typename... Args>
    static inline std::wstring c_wformat(
        const std::wstring& c_fmt,
        Args&&... args)
    {
        return c_wformat(c_fmt.c_str(), std::forward<Args>(args)...);
    }

} // namespace
_CXLIB_NAMESPACE_END