#pragma once
#include <triengine/global_options.hh>
#include <triengine/utility/preprocessor.hh>
#include <triengine/utility/string_format.hh>
#include <triengine/utility/logger.hh>

// Compile-time floor of the log macros: anything below `TRIENGINE_ACTIVE_LOG_LEVEL` expands to nothing, 
// so its arguments are not even evaluated. `logger::set_log_level()` filters at runtime on top of this.
#define TRIENGINE_LOG_LEVEL_TRACE    0
#define TRIENGINE_LOG_LEVEL_DEBUG    1
#define TRIENGINE_LOG_LEVEL_INFO     2
#define TRIENGINE_LOG_LEVEL_WARN     3
#define TRIENGINE_LOG_LEVEL_ERROR    4
#define TRIENGINE_LOG_LEVEL_CRITICAL 5
#define TRIENGINE_LOG_LEVEL_OFF      6

#if !defined(TRIENGINE_ACTIVE_LOG_LEVEL)
#  if defined(TRIENGINE_DEBUG_MODE)
#    define TRIENGINE_ACTIVE_LOG_LEVEL TRIENGINE_LOG_LEVEL_TRACE
#  else
#    define TRIENGINE_ACTIVE_LOG_LEVEL TRIENGINE_LOG_LEVEL_INFO
#  endif
#endif

#define _TRIENGINE_CALL_LOGGER0(LV, STR)       ::triengine::global_options::instance()->get_logger().log(_TRIENGINE_CURRENT_SOURCE_LOC(), LV, STR)
#define _TRIENGINE_CALL_LOGGER1(LV, FSTR, ...) ::triengine::global_options::instance()->get_logger().log_fmt(_TRIENGINE_CURRENT_SOURCE_LOC(), LV, FSTR, __VA_ARGS__)
#define _TRIENGINE_LOG(LV, ...) _TRIENGINE_PP_CONCAT(_TRIENGINE_CALL_LOGGER, _TRIENGINE_PP_HAS_COMMA(__VA_ARGS__))(LV, __VA_ARGS__)

#if TRIENGINE_ACTIVE_LOG_LEVEL <= TRIENGINE_LOG_LEVEL_TRACE
#  define TRIENGINE_TRACE(...)    _TRIENGINE_LOG(::triengine::utility::log_level::trace, __VA_ARGS__)
#else
#  define TRIENGINE_TRACE(...)    ((void)0)
#endif

#if TRIENGINE_ACTIVE_LOG_LEVEL <= TRIENGINE_LOG_LEVEL_DEBUG
#  define TRIENGINE_DEBUG(...)    _TRIENGINE_LOG(::triengine::utility::log_level::debug, __VA_ARGS__)
#else
#  define TRIENGINE_DEBUG(...)    ((void)0)
#endif

#if TRIENGINE_ACTIVE_LOG_LEVEL <= TRIENGINE_LOG_LEVEL_INFO
#  define TRIENGINE_INFO(...)     _TRIENGINE_LOG(::triengine::utility::log_level::info, __VA_ARGS__)
#else
#  define TRIENGINE_INFO(...)     ((void)0)
#endif

#if TRIENGINE_ACTIVE_LOG_LEVEL <= TRIENGINE_LOG_LEVEL_WARN
#  define TRIENGINE_WARN(...)     _TRIENGINE_LOG(::triengine::utility::log_level::warn, __VA_ARGS__)
#else
#  define TRIENGINE_WARN(...)     ((void)0)
#endif

#if TRIENGINE_ACTIVE_LOG_LEVEL <= TRIENGINE_LOG_LEVEL_ERROR
#  define TRIENGINE_ERROR(...)    _TRIENGINE_LOG(::triengine::utility::log_level::error, __VA_ARGS__)
#else
#  define TRIENGINE_ERROR(...)    ((void)0)
#endif

#if TRIENGINE_ACTIVE_LOG_LEVEL <= TRIENGINE_LOG_LEVEL_CRITICAL
#  define TRIENGINE_CRITICAL(...) _TRIENGINE_LOG(::triengine::utility::log_level::critical, __VA_ARGS__)
#else
#  define TRIENGINE_CRITICAL(...) ((void)0)
#endif

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