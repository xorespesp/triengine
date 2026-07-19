#pragma once
#include <triengine/common.h>
#include <triengine/utility/string_format.hh>
#include <triengine/utility/logger.hh>

#define _TRIENGINE_PANIC0(X)      ::triengine::utility::panic_impl(_TRIENGINE_CURRENT_SOURCE_LOC(), X)
#define _TRIENGINE_PANIC1(X, ...) ::triengine::utility::panicf_impl(_TRIENGINE_CURRENT_SOURCE_LOC(), X, __VA_ARGS__)
#define TRIENGINE_PANIC(...) _TRIENGINE_PP_CONCAT(_TRIENGINE_PANIC, _TRIENGINE_PP_HAS_COMMA(__VA_ARGS__))(__VA_ARGS__)

#if defined(TRIENGINE_DEBUG_MODE)
#  define TRIENGINE_ASSERT(X) ::triengine::utility::assert_impl(_TRIENGINE_CURRENT_SOURCE_LOC(), #X, X)
#else  // ^^^ TRIENGINE_DEBUG_MODE ^^^ / vvv !TRIENGINE_DEBUG_MODE vvv
#  define TRIENGINE_ASSERT(X)
#endif // ^^^ !TRIENGINE_DEBUG_MODE ^^^

namespace triengine::utility
{
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

    void assert_impl(
        const source_loc& src_loc,
        std::string_view cond_expr_sv,
        bool cond_expr_res
    );

} // namespace