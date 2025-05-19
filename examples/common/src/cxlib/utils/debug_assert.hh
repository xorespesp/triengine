#pragma once
#include <cxlib/cxlib_defs.h>
#include <cxlib/utils/logger.hh>
#include <cxlib/utils/string_format.hh>

#if defined (_CXLIB_PLATFORM_WIN32)
#  include <Windows.h>
#endif // ^^^ _CXLIB_PLATFORM_WIN32 ^^^

#define CXLIB_ASSERT(X) _CXLIB utils::debug::assert_impl(_CXLIB_CURRENT_SOURCE_LOC(), #X, X)

_CXLIB_NAMESPACE_BEGIN
namespace utils::debug
{
    static inline void assert_impl(
        const source_loc& src_loc,
        const std::string_view cond_expr_sv,
        const bool cond_expr_res)
    {
        if (cond_expr_res) {
            return;
        }

        uint32_t curr_tid{};
#if defined (_CXLIB_PLATFORM_WIN32)
        curr_tid = ::GetCurrentThreadId();
#else // ^^^ _CXLIB_PLATFORM_WIN32 ^^^ / vvv !_CXLIB_PLATFORM_WIN32 vvv
#  error "Not implmeneted"
#endif // ^^^ !_CXLIB_PLATFORM_WIN32 ^^^

        const auto src_filename = src_loc.filename();
        const auto msg = _CXLIB utils::string::c_format(""
            "Runtime assertion failed!\n\n"
            "File: %.*s\n"
            "Line: %d\n"
            "Function: %.*s\n"
            "Expression: %.*s\n"
            "Thread: 0x%X\n\n"
            "(Press <Retry> to debug the application)"
            , static_cast<int>(src_filename.size())
            , src_filename.data()
            , src_loc.line
            , static_cast<int>(src_loc.funcname.size())
            , src_loc.funcname.data()
            , static_cast<int>(cond_expr_sv.size())
            , cond_expr_sv.data()
            , curr_tid
        );

        CXLIB_ERROR(msg);

#if defined (_CXLIB_PLATFORM_WIN32)
        switch (::MessageBoxA(
            HWND_DESKTOP,
            msg.c_str(),
            "ERROR REPORT",
            MB_TASKMODAL | MB_SETFOREGROUND | MB_ABORTRETRYIGNORE | MB_ICONERROR
        )) {
        case IDABORT: { ::TerminateProcess(::GetCurrentProcess(), 1); break; }
        case IDRETRY: { ::__debugbreak(); break; }
        case IDIGNORE:
        default: { break; }
        }
#else // ^^^ _CXLIB_PLATFORM_WIN32 ^^^ / vvv !_CXLIB_PLATFORM_WIN32 vvv
#  error "Not implmeneted"
#endif // ^^^ !_CXLIB_PLATFORM_WIN32 ^^^
    }

} // namespace
_CXLIB_NAMESPACE_END