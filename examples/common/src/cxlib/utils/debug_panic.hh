#pragma once
#include <cxlib/cxlib_defs.h>
#include <cxlib/utils/logger.hh>
#include <cxlib/utils/string_format.hh>

#include <stdexcept>

#define _CXLIB_PANIC0(STR)       _CXLIB utils::debug::panic_impl(_CXLIB_CURRENT_SOURCE_LOC(), STR)
#define _CXLIB_PANIC1(FSTR, ...) _CXLIB utils::debug::panic_fmt_impl(_CXLIB_CURRENT_SOURCE_LOC(), FSTR, __VA_ARGS__)
#define CXLIB_PANIC(...) _CXLIB_PP_CONCAT(_CXLIB_PANIC, _CXLIB_PP_HAS_COMMA(__VA_ARGS__))(__VA_ARGS__)

_CXLIB_NAMESPACE_BEGIN
namespace utils::debug
{
    [[noreturn]] static inline void panic_impl(
        const source_loc& src_loc,
        const std::string_view msg_sv)
    {
        const auto src_filename = src_loc.filename();
        const auto msg = _CXLIB utils::string::c_format(""
            "Panic occurred at <%.*s:%d@%.*s> : %.*s"
            , static_cast<int>(src_filename.size())
            , src_filename.data()
            , src_loc.line
            , static_cast<int>(src_loc.funcname.size())
            , src_loc.funcname.data()
            , static_cast<int>(msg_sv.size())
            , msg_sv.data()
        );

        CXLIB_CRITICAL(msg);
        throw std::runtime_error{ msg };
    }

#if defined (_CXLIB_HAS_FMT) || defined (_CXLIB_HAS_SPDLOG)
    template <typename... _Args>
    [[noreturn]] static inline void panic_fmt_impl(
        const source_loc& src_loc,
        const std::string_view fmt,
        _Args&&... args)
    {
        panic_impl(src_loc, fmt::format(fmt, std::forward<_Args>(args)...));
    }
#endif // ^^^ _CXLIB_HAS_FMT || _CXLIB_HAS_SPDLOG ^^^

} // namespace
_CXLIB_NAMESPACE_END