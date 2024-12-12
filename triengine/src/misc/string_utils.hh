#pragma once
#include "../common.h"
#include <string>
#include <cstdio>

namespace triengine::misc::string
{
    template <typename... Args>
    static inline std::string c_format(
        const char* const c_fmt,
        Args&&... args)
    {
        if (!c_fmt) {
            throw std::runtime_error{ "invalid argument" };
        }

        const int needed_sz = std::snprintf(nullptr, 0, c_fmt, std::forward<Args>(args)...);
        if (needed_sz < 0) {
            throw std::runtime_error{ "formatting error" };
        }

        std::string result(static_cast<size_t>(needed_sz + 1/* null terminator */), '\0');
        const int written_sz = std::snprintf(result.data(), result.size(), c_fmt, std::forward<Args>(args)...);
        if (written_sz < 0 || written_sz > needed_sz) {
            throw std::runtime_error{ "formatting error" };
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

} // namespace