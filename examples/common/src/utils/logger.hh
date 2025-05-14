#pragma once
#include "c_format.hh"
#include "codecvt.hh"
#include "mutex.hh"

#include <filesystem>

#if !defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
#  define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#endif // ^^^ SPDLOG_WCHAR_TO_UTF8_SUPPORT ^^^
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/fmt/ostr.h> // support for user defined types
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/callback_sink.h>

// Concat two arguments.
#define __PP_CONCAT(X, Y) X ## Y
#define _PP_CONCAT(X, Y) __PP_CONCAT(X, Y)

// Expand argument.
#define _PP_EXPAND(X) X

// Returns the 100th argument.
#define _PP_ARG100(_,\
   _100,_99,_98,_97,_96,_95,_94,_93,_92,_91,_90,_89,_88,_87,_86,_85,_84,_83,_82,_81, \
   _80,_79,_78,_77,_76,_75,_74,_73,_72,_71,_70,_69,_68,_67,_66,_65,_64,_63,_62,_61, \
   _60,_59,_58,_57,_56,_55,_54,_53,_52,_51,_50,_49,_48,_47,_46,_45,_44,_43,_42,_41, \
   _40,_39,_38,_37,_36,_35,_34,_33,_32,_31,_30,_29,_28,_27,_26,_25,_24,_23,_22,_21, \
   _20,_19,_18,_17,_16,_15,_14,_13,_12,_11,_10,_9,_8,_7,_6,_5,_4,_3,_2,_1,...) _1

// Returns whether __VA_ARGS__ has a comma (up to 100 arguments).
// Note: MSVC does not expand __VA_ARGS__ like most other compilers, so an extra step(expansion) is necessary.
// Ref: https://stackoverflow.com/a/66556553
#define _PP_HAS_COMMA(...) _PP_EXPAND(_PP_ARG100(__VA_ARGS__, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ,1, \
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0))

#define _CURRENT_SOURCE_LOC()        ::utils::logger::src_loc{ __FILE__, __LINE__, __func__ }
#define _CALL_LOGGER0(LV, STR)       ::utils::logger::instance().print(_CURRENT_SOURCE_LOC(), LV, STR)
#define _CALL_LOGGER1(LV, FSTR, ...) ::utils::logger::instance().printf(_CURRENT_SOURCE_LOC(), LV, FSTR, __VA_ARGS__)

#define LOG_TRACE(...)    _PP_CONCAT(_CALL_LOGGER, _PP_HAS_COMMA(__VA_ARGS__))(::utils::logger::level::trace, __VA_ARGS__)
#define LOG_DEBUG(...)    _PP_CONCAT(_CALL_LOGGER, _PP_HAS_COMMA(__VA_ARGS__))(::utils::logger::level::debug, __VA_ARGS__)
#define LOG_INFO(...)     _PP_CONCAT(_CALL_LOGGER, _PP_HAS_COMMA(__VA_ARGS__))(::utils::logger::level::info, __VA_ARGS__)
#define LOG_WARN(...)     _PP_CONCAT(_CALL_LOGGER, _PP_HAS_COMMA(__VA_ARGS__))(::utils::logger::level::warn, __VA_ARGS__)
#define LOG_ERROR(...)    _PP_CONCAT(_CALL_LOGGER, _PP_HAS_COMMA(__VA_ARGS__))(::utils::logger::level::error, __VA_ARGS__)
#define LOG_CRITICAL(...) _PP_CONCAT(_CALL_LOGGER, _PP_HAS_COMMA(__VA_ARGS__))(::utils::logger::level::critical, __VA_ARGS__)

namespace utils
{
    // https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
    class logger final {
    public:
        enum class level {
            trace = SPDLOG_LEVEL_TRACE,
            debug = SPDLOG_LEVEL_DEBUG,
            info = SPDLOG_LEVEL_INFO,
            warn = SPDLOG_LEVEL_WARN,
            error = SPDLOG_LEVEL_ERROR,
            critical = SPDLOG_LEVEL_CRITICAL,
        };

        struct src_loc {
            constexpr src_loc() = default;
            constexpr src_loc(const char* filename_in, int line_in, const char* funcname_in)
                : filename{ filename_in }
                , line{ line_in }
                , funcname{ funcname_in }
            {}

            constexpr bool empty() const noexcept { return line == 0; }

            const char* filename{ nullptr };
            int line{ 0 };
            const char* funcname{ nullptr };
        };

        class init_option {
        private:
            std::string _logger_name{ "logger" };
            level _logger_level{ level::trace };
            bool _is_async_logger{ false };
            size_t _asnyc_logger_q_size{ 0 }, _async_logger_thread_count{ 0 };
            std::vector<spdlog::sink_ptr> _logger_sinks{};

        public:
            init_option() = default;

            init_option& set_logger_name(const std::string_view new_name) {
                _logger_name = new_name;
                return *this;
            }

            init_option& set_logger_level(const level lv) {
                _logger_level = lv;
                return *this;
            }

            init_option& enable_async_mode(const size_t q_size = 16 * 1024, const size_t thread_count = 1) {
                _is_async_logger = true;
                _asnyc_logger_q_size = q_size;
                _async_logger_thread_count = thread_count;
                return *this;
            }

            init_option& enable_file_logging(const std::filesystem::path& file_path, const level lv = level::trace) {
                auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path.string(), true);
                file_sink->set_pattern("%Y-%m-%d %H:%M:%S.%f %z | %n | ---%L--- | TID %t | %s:%#@%! | %v");
                file_sink->set_level(static_cast<spdlog::level::level_enum>(lv));

                _logger_sinks.push_back(file_sink);
                return *this;
            }

            init_option& enable_stdout_logging(const level lv = level::trace) {
                auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                stdout_sink->set_pattern("%^%H:%M:%S.%f | %n | TID %t | %s:%# | %v%$");
                stdout_sink->set_level(static_cast<spdlog::level::level_enum>(lv));

                _logger_sinks.push_back(stdout_sink);
                return *this;
            }

            init_option& enable_callback_logging(const level lv = level::trace) {
                auto callback_sink = std::make_shared<spdlog::sinks::callback_sink_mt>(
                    [](const spdlog::details::log_msg& /*msg*/) {
                        // for example you can be notified by sending an email to yourself
                    });
                callback_sink->set_pattern("%H:%M:%S.%f | %n | TID %t | %s:%# | %v");
                callback_sink->set_level(static_cast<spdlog::level::level_enum>(lv));

                _logger_sinks.push_back(callback_sink);
                return *this;
            }

            friend class logger;
        };

    private:
        utils::spin_lock _init_lock;
        std::atomic_bool _init_flag{ false };
        std::shared_ptr<spdlog::logger> _logger_inst;

    private:
        logger() = default;
        ~logger() = default;

        // make non-copyable
        logger(const logger&) = delete;
        logger& operator= (const logger&) = delete;

    public:
        static logger& instance() {
            static logger obj{};
            return obj;
        }

        bool is_inited() const noexcept {
            return _init_flag;
        }

        void init(init_option opt = init_option{})
        {
            std::scoped_lock lck{ _init_lock };
            if (this->is_inited()) { return; }

            // Add default sink (DebugOutputString)
            auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>(false);
            msvc_sink->set_pattern("%H:%M:%S.%f | %n | ---%L--- | thread %t | %s:%# | %v");
            msvc_sink->set_level(spdlog::level::trace);
            opt._logger_sinks.push_back(msvc_sink);

            if (opt._is_async_logger)
            {
                spdlog::init_thread_pool(opt._asnyc_logger_q_size, opt._async_logger_thread_count);
                _logger_inst = std::make_shared<spdlog::async_logger>(
                    opt._logger_name,
                    opt._logger_sinks.begin(),
                    opt._logger_sinks.end(),
                    spdlog::thread_pool(),
                    spdlog::async_overflow_policy::block
                );
            }
            else
            {
                _logger_inst = std::make_shared<spdlog::logger>(
                    opt._logger_name,
                    opt._logger_sinks.begin(),
                    opt._logger_sinks.end()
                );
            }

            _logger_inst->set_level(static_cast<spdlog::level::level_enum>(opt._logger_level));
            _logger_inst->flush_on(static_cast<spdlog::level::level_enum>(level::warn));

            // periodically flush all *registered* loggers every 3 seconds:
            // warning: only use if all your loggers are thread-safe ("_mt" loggers)
            spdlog::flush_every(std::chrono::seconds(3));

            _init_flag = true;
        }

        void deinit()
        {
            std::scoped_lock lck{ _init_lock };
            if (!this->is_inited()) { return; }

            // spdlog::shutdown()
            //     Release all spdlog resources, and drop all loggers in the registry.
            //     This is optional (only mandatory if using windows + async log).
            // 
            // NOTE:
            //     There is a bug in VS runtime that cause the application dead-lock when it exits.
            //     If you use async logging, please make sure to call spdlog::shutdown() before main() exit.
            //     http://stackoverflow.com/questions/10915233/stdthreadjoin-hangs-if-called-after-main-exits-when-using-vs2012
            spdlog::shutdown();
            _logger_inst.reset(); // `std::shared_ptr` is thread-unsafe; MUST call `reset()` after spdlog shutdown

            _init_flag = false;
        }

        void set_level(const level lv)
        {
            _logger_inst->set_level(static_cast<spdlog::level::level_enum>(lv));
        }

        void print(
            const level lv,
            const std::string_view msg_sv)
        {
            if (!this->is_inited()) { return; }

            _logger_inst->log(
                static_cast<spdlog::level::level_enum>(lv),
                msg_sv
            );
        }

        void print(
            const src_loc src,
            const level lv,
            const std::string_view msg_sv)
        {
            if (!this->is_inited()) { return; }

            _logger_inst->log(
                spdlog::source_loc{ src.filename, src.line, src.funcname },
                static_cast<spdlog::level::level_enum>(lv),
                msg_sv
            );
        }

        template <typename... _Args>
        void printf(
            const level lv,
            const std::string& c_fmt,
            _Args&&... args)
        {
            if (!this->is_inited()) { return; }

            auto msg = utils::string::c_format(c_fmt, std::forward<_Args>(args)...);
            _logger_inst->log(
                static_cast<spdlog::level::level_enum>(lv),
                msg
            );
        }

        template <typename... _Args>
        void printf(
            const src_loc src,
            const level lv,
            const std::string& c_fmt,
            _Args&&... args)
        {
            if (!this->is_inited()) { return; }

            auto msg = utils::string::c_format(c_fmt, std::forward<_Args>(args)...);
            _logger_inst->log(
                spdlog::source_loc{ src.filename, src.line, src.funcname },
                static_cast<spdlog::level::level_enum>(lv),
                msg
            );
        }

        void print(
            const level lv,
            const std::wstring_view wmsg_sv)
        {
            if (!this->is_inited()) { return; }

            _logger_inst->log(
                static_cast<spdlog::level::level_enum>(lv),
#if defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
                wmsg_sv
#else  // ^^^ SPDLOG_WCHAR_TO_UTF8_SUPPORT ^^^ / vvv !SPDLOG_WCHAR_TO_UTF8_SUPPORT vvv
                utils::string::utf16_to_utf8(wmsg_sv)
#endif // ^^^ !SPDLOG_WCHAR_TO_UTF8_SUPPORT ^^^
            );
        }

        void print(
            const src_loc src,
            const level lv,
            const std::wstring_view wmsg_sv)
        {
            if (!this->is_inited()) { return; }

            _logger_inst->log(
                spdlog::source_loc{ src.filename, src.line, src.funcname },
                static_cast<spdlog::level::level_enum>(lv),
#if defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
                wmsg_sv
#else  // ^^^ SPDLOG_WCHAR_TO_UTF8_SUPPORT ^^^ / vvv !SPDLOG_WCHAR_TO_UTF8_SUPPORT vvv
                utils::string::utf16_to_utf8(wmsg_sv)
#endif // ^^^ !SPDLOG_WCHAR_TO_UTF8_SUPPORT ^^^
            );
        }

        template <typename... _Args>
        void printf(
            const level lv,
            const std::wstring& wfmt,
            _Args&&... args)
        {
            if (!this->is_inited()) { return; }

            auto wmsg = utils::string::c_wformat(wfmt, std::forward<_Args>(args)...);
            _logger_inst->log(
                static_cast<spdlog::level::level_enum>(lv),
#if defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
                wmsg
#else  // ^^^ SPDLOG_WCHAR_TO_UTF8_SUPPORT ^^^ / vvv !SPDLOG_WCHAR_TO_UTF8_SUPPORT vvv
                utils::string::utf16_to_utf8(wmsg)
#endif // ^^^ !SPDLOG_WCHAR_TO_UTF8_SUPPORT ^^^
            );
        }

        template <typename... _Args>
        void printf(
            const src_loc src,
            const level lv,
            const std::wstring& c_wfmt,
            _Args&&... args)
        {
            if (!this->is_inited()) { return; }

            auto wmsg = utils::string::c_wformat(c_wfmt, std::forward<_Args>(args)...);
            _logger_inst->log(
                spdlog::source_loc{ src.filename, src.line, src.funcname },
                static_cast<spdlog::level::level_enum>(lv),
#if defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
                wmsg
#else  // ^^^ SPDLOG_WCHAR_TO_UTF8_SUPPORT ^^^ / vvv !SPDLOG_WCHAR_TO_UTF8_SUPPORT vvv
                utils::string::utf16_to_utf8(wmsg)
#endif // ^^^ !SPDLOG_WCHAR_TO_UTF8_SUPPORT ^^^
            );
        }

    }; // class
} // namespace