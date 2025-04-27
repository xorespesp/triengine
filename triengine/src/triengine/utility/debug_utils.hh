#pragma once
#include <triengine/common.h>
#include <triengine/utility/string_utils.hh>

#define _TRIENGINE_CURRENT_SOURCE_LOC() ::triengine::utility::debug::source_loc{ __FILE__, __LINE__, __func__ }

#define _TRIENGINE_PANIC0(X)      ::triengine::utility::debug::panic_impl(_TRIENGINE_CURRENT_SOURCE_LOC(), X)
#define _TRIENGINE_PANIC1(X, ...) ::triengine::utility::debug::panicf_impl(_TRIENGINE_CURRENT_SOURCE_LOC(), X, __VA_ARGS__)
#define TRIENGINE_PANIC(...) _TRIENGINE_PP_CONCAT(_TRIENGINE_PANIC, _TRIENGINE_PP_HAS_COMMA(__VA_ARGS__))(__VA_ARGS__)

#if defined(TRIENGINE_DEBUG_MODE)
#  define _TRIENGINE_TRACE0(X)      ::triengine::utility::debug::trace_impl(_TRIENGINE_CURRENT_SOURCE_LOC(), X)
#  define _TRIENGINE_TRACE1(X, ...) ::triengine::utility::debug::tracef_impl(_TRIENGINE_CURRENT_SOURCE_LOC(), X, __VA_ARGS__)
#  define TRIENGINE_TRACE(...) _TRIENGINE_PP_CONCAT(_TRIENGINE_TRACE, _TRIENGINE_PP_HAS_COMMA(__VA_ARGS__))(__VA_ARGS__)
#  define TRIENGINE_ASSERT(X) ::triengine::utility::debug::assert_impl(_TRIENGINE_CURRENT_SOURCE_LOC(), #X, X)
#else  // ^^^ TRIENGINE_DEBUG_MODE ^^^ / vvv !TRIENGINE_DEBUG_MODE vvv
#  define TRIENGINE_TRACE(X)
#  define TRIENGINE_ASSERT(X)
#endif // ^^^ !TRIENGINE_DEBUG_MODE ^^^

namespace triengine::utility::debug
{
    // `std::source_location`(since C++20) like object
    struct source_loc
    {
        constexpr source_loc() = default;

        template <std::size_t N, std::size_t M>
        constexpr source_loc(const char(&filepath_)[N], int line_, const char(&funcname_)[M])
            : filepath{ filepath_, N - 1 }
            , line{ line_ }
            , funcname{ funcname_, M - 1 }
        {}

        constexpr bool empty() const noexcept { return filepath.empty(); }
        constexpr std::string_view filename() const {
            return filepath.substr(filepath.find_last_of("/\\") + 1); // split filename
        }

        std::string_view filepath;
        int line{};
        std::string_view funcname;
    };

    [[noreturn]] void panic_impl(
        const source_loc& src_loc,
        std::string_view msg_sv
    );

    template <typename... _Args>
    [[noreturn]] static inline void panicf_impl(
        const source_loc& src_loc,
        const char* const c_fmt,
        _Args&&... args)
    {
        panic_impl(src_loc, utility::string::c_format(c_fmt, std::forward<_Args>(args)...));
    }

    template <typename... _Args>
    [[noreturn]] static inline void panicf_impl(
        const source_loc& src_loc,
        const std::string& c_fmt,
        _Args&&... args)
    {
        panic_impl(src_loc, utility::string::c_format(c_fmt, std::forward<_Args>(args)...));
    }

    void trace_impl(
        const source_loc& src_loc,
        std::string_view msg_sv
    );

    template <typename... _Args>
    static inline void tracef_impl(
        const source_loc& src_loc,
        const char* const c_fmt,
        _Args&&... args)
    {
        trace_impl(src_loc, utility::string::c_format(c_fmt, std::forward<_Args>(args)...));
    }

    template <typename... _Args>
    static inline void tracef_impl(
        const source_loc& src_loc,
        const std::string& c_fmt,
        _Args&&... args)
    {
        trace_impl(src_loc, utility::string::c_format(c_fmt, std::forward<_Args>(args)...));
    }

    void assert_impl(
        const source_loc& src_loc,
        std::string_view cond_expr_sv,
        bool cond_expr_res
    );

} // namespace