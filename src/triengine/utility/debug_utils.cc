#pragma once
#include "debug_utils.hh"

#include <triengine/global_options.hh>

#if defined (_TRIENGINE_PLATFORM_WIN32)
#  include <Windows.h>
#endif // ^^^ _TRIENGINE_PLATFORM_WIN32 ^^^

namespace triengine::utility
{
    namespace
    {
#if defined (_TRIENGINE_PLATFORM_WIN32)
        /**
            * Refs:
            *   Windows Kits/10/Source/10.0.20348.0/ucrt/internal/winapi_thunks.cpp
            *   Windows Kits/10/Source/10.0.20348.0/ucrt/misc/dbgrpt.cpp
            *   Windows Kits/10/Source/10.0.20348.0/ucrt/misc/crtmbox.cpp
            */

        enum class dbgrpt_mbox_type {
            error, warn, info
        };

        void show_dbgrpt_mbox(
            const dbgrpt_mbox_type mbox_type,
            const std::string& mbox_msg)
        {
            const std::string mbox_title{ [mbox_type]() {
                using namespace std::string_literals;
                switch (mbox_type) {
                case dbgrpt_mbox_type::error: return "ERROR REPORT"s;
                case dbgrpt_mbox_type::warn:  return "WARNING REPORT"s;
                case dbgrpt_mbox_type::info:  return "INFORMATION REPORT"s;
                default: return ""s;
                }
            }() };

            const uint32_t mbox_flags{ [mbox_type]() {
                uint32_t flags = MB_TASKMODAL | MB_SETFOREGROUND | MB_ABORTRETRYIGNORE;
                switch (mbox_type) {
                case dbgrpt_mbox_type::error: flags |= MB_ICONERROR; break;
                case dbgrpt_mbox_type::warn:  flags |= MB_ICONWARNING; break;
                case dbgrpt_mbox_type::info:  flags |= MB_ICONINFORMATION; break;
                default: break;
                }
                return flags;
            }() };

            switch (::MessageBoxA(
                HWND_DESKTOP,
                mbox_msg.c_str(),
                mbox_title.c_str(),
                mbox_flags
            )) {
            case IDABORT: { ::TerminateProcess(::GetCurrentProcess(), 1); break; }
            case IDRETRY: { ::__debugbreak(); break; }
            case IDIGNORE:
            default: { break; }
            }
        }
#endif // ^^^ _TRIENGINE_PLATFORM_WIN32 ^^^
    } // namespace

    void panic_impl(
        const source_loc& src_loc,
        const std::string_view msg_sv)
    {
        const auto src_filename = src_loc.filename();
        const auto msg = utility::string::c_format(""
            "Panic occurred at <%.*s:%d@%.*s> : %.*s"
            , static_cast<int>(src_filename.size())
            , src_filename.data()
            , src_loc.line
            , static_cast<int>(src_loc.funcname.size())
            , src_loc.funcname.data()
            , static_cast<int>(msg_sv.size())
            , msg_sv.data()
        );

        global_options::instance()->get_logger().print(src_loc, log_level::critical, msg);

#if defined (_TRIENGINE_PLATFORM_WIN32) && defined(TRIENGINE_DEBUG_MODE)
        show_dbgrpt_mbox(dbgrpt_mbox_type::error, msg);
#endif // ^^^ _TRIENGINE_PLATFORM_WIN32 && TRIENGINE_DEBUG_MODE ^^^

        throw std::runtime_error{ msg };
    }

    void assert_impl(
        const source_loc& src_loc,
        const std::string_view cond_expr_sv,
        const bool cond_expr_res)
    {
        if (cond_expr_res) {
            return;
        }

#if defined (_TRIENGINE_PLATFORM_WIN32)
        const auto src_filename = src_loc.filename();
        const auto msg = utility::string::c_format(""
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
            , ::GetCurrentThreadId()
        );

        global_options::instance()->get_logger().print(src_loc, log_level::critical, msg);
        show_dbgrpt_mbox(dbgrpt_mbox_type::error, msg);
#else // ^^^ _TRIENGINE_PLATFORM_WIN32 ^^^ / vvv !_TRIENGINE_PLATFORM_WIN32 vvv
#  error Not implemented
#endif // ^^^ !_TRIENGINE_PLATFORM_WIN32 ^^^
    }

} // namespace