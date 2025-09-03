#pragma once
#include <cxlib/cxlib_defs.h>

#include <filesystem>
#include <mutex>

#if defined(_CXLIB_PLATFORM_WIN32)
#  include <Windows.h>
#endif // ^^^ _CXLIB_PLATFORM_WIN32 ^^^

#if defined(_CXLIB_HAS_SPDLOG)
#  if !defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
#    define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#  endif // ^^^ SPDLOG_WCHAR_TO_UTF8_SUPPORT ^^^
#  include <spdlog/spdlog.h>
#  include <spdlog/async.h>
#  include <spdlog/fmt/ostr.h> // support for user defined types
#  include <spdlog/sinks/msvc_sink.h>
#  include <spdlog/sinks/stdout_color_sinks.h>
#  include <spdlog/sinks/basic_file_sink.h>
#  include <spdlog/sinks/callback_sink.h>
#else // ^^^ _CXLIB_HAS_SPDLOG / !_CXLIB_HAS_SPDLOG ^^^
#  error "spdlog required"
#endif // ^^^ !_CXLIB_HAS_SPDLOG ^^^

#define _CXLIB_CURRENT_SOURCE_LOC()        _CXLIB utils::source_loc{ __FILE__, __LINE__, __func__ }
#define _CXLIB_CALL_LOGGER0(LV, STR)       _CXLIB utils::logger::instance().log(_CXLIB_CURRENT_SOURCE_LOC(), LV, STR)
#define _CXLIB_CALL_LOGGER1(LV, FSTR, ...) _CXLIB utils::logger::instance().log_fmt(_CXLIB_CURRENT_SOURCE_LOC(), LV, FSTR, __VA_ARGS__)

#define CXLIB_TRACE(...)    _CXLIB_PP_CONCAT(_CXLIB_CALL_LOGGER, _CXLIB_PP_HAS_COMMA(__VA_ARGS__))(_CXLIB utils::logger::level::trace, __VA_ARGS__)
#define CXLIB_DEBUG(...)    _CXLIB_PP_CONCAT(_CXLIB_CALL_LOGGER, _CXLIB_PP_HAS_COMMA(__VA_ARGS__))(_CXLIB utils::logger::level::debug, __VA_ARGS__)
#define CXLIB_INFO(...)     _CXLIB_PP_CONCAT(_CXLIB_CALL_LOGGER, _CXLIB_PP_HAS_COMMA(__VA_ARGS__))(_CXLIB utils::logger::level::info, __VA_ARGS__)
#define CXLIB_WARN(...)     _CXLIB_PP_CONCAT(_CXLIB_CALL_LOGGER, _CXLIB_PP_HAS_COMMA(__VA_ARGS__))(_CXLIB utils::logger::level::warn, __VA_ARGS__)
#define CXLIB_ERROR(...)    _CXLIB_PP_CONCAT(_CXLIB_CALL_LOGGER, _CXLIB_PP_HAS_COMMA(__VA_ARGS__))(_CXLIB utils::logger::level::error, __VA_ARGS__)
#define CXLIB_CRITICAL(...) _CXLIB_PP_CONCAT(_CXLIB_CALL_LOGGER, _CXLIB_PP_HAS_COMMA(__VA_ARGS__))(_CXLIB utils::logger::level::critical, __VA_ARGS__)

_CXLIB_NAMESPACE_BEGIN
namespace utils
{
    // `std::source_location`(since C++20) like object
    class source_loc
    {
    public:
        std::string_view filepath;
        int line{};
        std::string_view funcname;

    public:
        constexpr source_loc() = default;

        template <std::size_t N, std::size_t M>
        constexpr source_loc(const char(&filepath_)[N], int line_, const char(&funcname_)[M])
            : filepath{ filepath_, N - 1 }
            , line{ line_ }
            , funcname{ funcname_, M - 1 }
        {}

        constexpr bool empty() const noexcept {
            return filepath.empty();
        }

        constexpr std::string_view filename() const {
            return filepath.substr(filepath.find_last_of("/\\") + 1); // split filename
        }

#if defined(_CXLIB_HAS_SPDLOG)
        operator spdlog::source_loc() const {
            return !this->empty()
                ? spdlog::source_loc{ this->filename().data(), line, this->funcname.data() }
                : spdlog::source_loc{};
        }
#endif // ^^^ _CXLIB_HAS_SPDLOG ^^^
    };

    class logger final
    {
    public:
        enum class level
        {
            trace = SPDLOG_LEVEL_TRACE,
            debug = SPDLOG_LEVEL_DEBUG,
            info = SPDLOG_LEVEL_INFO,
            warn = SPDLOG_LEVEL_WARN,
            error = SPDLOG_LEVEL_ERROR,
            critical = SPDLOG_LEVEL_CRITICAL,
        };

        class init_options
        {
        private:
            std::string _logger_name{ "logger" };
            bool _is_async_logger{ false };
            size_t _async_logger_q_size{ 0 };
            size_t _async_logger_thread_count{ 0 };
            std::vector<spdlog::sink_ptr> _logger_sinks{};
            level _flush_on_level{ level::warn };
            std::chrono::seconds _flush_interval{ 3 };

        public:
            init_options() = default;

            init_options& set_logger_name(const std::string_view new_name)
            {
                _logger_name = new_name;
                return *this;
            }

            init_options& set_flush_on_level(const level flush_on_lv)
            {
                _flush_on_level = flush_on_lv;
                return *this;
            }

            init_options& set_flush_interval(const std::chrono::seconds interval)
            {
                _flush_interval = interval;
                return *this;
            }

            init_options& enable_async_mode(
                const size_t q_size = 16 * 1024,
                const size_t thread_count = 1)
            {
                _is_async_logger = true;
                _async_logger_q_size = q_size;
                _async_logger_thread_count = thread_count;
                return *this;
            }

            init_options& enable_file_logging(
                const std::filesystem::path& file_path,
                const level lv = level::trace,
                const std::string& pattern = "%Y-%m-%d %H:%M:%S.%f %z | %n | ---%L--- | TID %t | %s:%#@%! | %v")
            {
                auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path.string(), true);
                file_sink->set_level(static_cast<spdlog::level::level_enum>(lv));
                file_sink->set_pattern(pattern);

                _logger_sinks.push_back(file_sink);
                return *this;
            }

            init_options& enable_stdout_logging(
                const level lv = level::trace,
                const std::string& pattern = "%^%H:%M:%S.%f | %n | TID %t | %s:%# | %v%$")
            {
                auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                stdout_sink->set_level(static_cast<spdlog::level::level_enum>(lv));
                stdout_sink->set_pattern(pattern);

                _logger_sinks.push_back(stdout_sink);
                return *this;
            }

            init_options& enable_callback_logging(
                const level lv = level::trace,
                const std::string& pattern = "%H:%M:%S.%f | %n | TID %t | %s:%# | %v")
            {
                auto callback_sink = std::make_shared<spdlog::sinks::callback_sink_mt>(
                    [](const spdlog::details::log_msg& /*msg*/) {
                    // for example you can be notified by sending an email to yourself
                });
                callback_sink->set_level(static_cast<spdlog::level::level_enum>(lv));
                callback_sink->set_pattern(pattern);

                _logger_sinks.push_back(callback_sink);
                return *this;
            }

            friend class logger;
        };

    private:
        std::mutex _init_mtx;
        std::shared_ptr<spdlog::logger> _logger_impl;

    public:
        static logger& instance() {
            static logger inst_{};
            return inst_;
        }

    private:
        logger() {
#if defined(_CXLIB_PLATFORM_WIN32)
            ::SetConsoleOutputCP(CP_UTF8); // https://github.com/gabime/spdlog/issues/762
#endif // ^^^ _CXLIB_PLATFORM_WIN32 ^^^
        }

        ~logger() = default;
        logger(const logger&) = delete;
        logger& operator= (const logger&) = delete;

    public:
        bool is_inited() const noexcept {
            return this->_atomic_load_logger_ptr() != nullptr;
        }

        void init(init_options opts = init_options{})
        {
            std::scoped_lock lk{ _init_mtx };
            if (this->is_inited()) { return; }

            // Add default sink (DebugOutputString)
#if defined(_CXLIB_PLATFORM_WIN32) // Only add MSVC sink on Windows
            auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>(false);
            msvc_sink->set_pattern("%H:%M:%S.%f | %n | ---%L--- | TID %t | %s:%#@%! | %v");
            msvc_sink->set_level(spdlog::level::trace);
            opts._logger_sinks.push_back(msvc_sink);
#endif // _CXLIB_PLATFORM_WIN32

            std::shared_ptr<spdlog::logger> new_logger;

            if (opts._is_async_logger)
            {
                spdlog::init_thread_pool(opts._async_logger_q_size, opts._async_logger_thread_count);
                new_logger = std::make_shared<spdlog::async_logger>(
                    opts._logger_name,
                    opts._logger_sinks.begin(),
                    opts._logger_sinks.end(),
                    spdlog::thread_pool(),
                    spdlog::async_overflow_policy::block
                );
            }
            else
            {
                new_logger = std::make_shared<spdlog::logger>(
                    opts._logger_name,
                    opts._logger_sinks.begin(),
                    opts._logger_sinks.end()
                );
            }

            new_logger->set_level(spdlog::level::trace); // set default global level to trace
            new_logger->flush_on(static_cast<spdlog::level::level_enum>(opts._flush_on_level));

            // Periodically flush all *registered* loggers every 3 seconds:
            // Warning: only use if all your loggers are thread-safe ("_mt" loggers)
            // Pass std::chrono::seconds(0) to spdlog::flush_every to disable.
            spdlog::flush_every(opts._flush_interval);

            this->_atomic_store_logger_ptr(new_logger);
        }

        void deinit()
        {
            std::scoped_lock lk{ _init_mtx };

            auto logger_ptr = this->_atomic_load_logger_ptr();
            if (!logger_ptr) { return; }

            logger_ptr->flush();

            // spdlog::shutdown()
            //     Release all spdlog resources, and drop all loggers in the registry.
            //     This is optional (only mandatory if using windows + async log).
            // 
            // NOTE:
            //     There is a bug in VS runtime that cause the application dead-lock when it exits.
            //     If you use async logging, please make sure to call spdlog::shutdown() before main() exit.
            //     http://stackoverflow.com/questions/10915233/stdthreadjoin-hangs-if-called-after-main-exits-when-using-vs2012
            spdlog::shutdown();

            // reset thread safely
            this->_atomic_store_logger_ptr(std::shared_ptr<spdlog::logger>{});
        }

        void set_level(const level lv)
        {
            auto logger_ptr = this->_atomic_load_logger_ptr();
            if (!logger_ptr) { return; }

            logger_ptr->set_level(static_cast<spdlog::level::level_enum>(lv));
        }

        void log(
            const source_loc src_loc,
            const level lv,
            const std::string_view msg_sv)
        {
            auto logger_ptr = this->_atomic_load_logger_ptr();
            if (!logger_ptr) { return; }

            logger_ptr->log(
                static_cast<spdlog::source_loc>(src_loc),
                static_cast<spdlog::level::level_enum>(lv),
                msg_sv
            );
        }

        void log(
            const level lv,
            const std::string_view msg_sv)
        {
            this->log(source_loc{}, lv, msg_sv);
        }

        template <typename... _Args>
        void log_fmt(
            const source_loc src_loc,
            const level lv,
            const std::string_view fmt_sv,
            _Args&&... args)
        {
            auto logger_ptr = this->_atomic_load_logger_ptr();
            if (!logger_ptr) { return; }

            logger_ptr->log(
                static_cast<spdlog::source_loc>(src_loc),
                static_cast<spdlog::level::level_enum>(lv),
                fmt::runtime(fmt_sv),
                std::forward<_Args>(args)...
            );
        }

        template <typename... _Args>
        void log_fmt(
            const level lv,
            const std::string_view fmt_sv,
            _Args&&... args)
        {
            this->log_fmt(source_loc{}, lv, fmt_sv, std::forward<_Args>(args)...);
        }

#if defined(SPDLOG_WCHAR_TO_UTF8_SUPPORT)
        void log(
            const source_loc src_loc,
            const level lv,
            const std::wstring_view wmsg_sv)
        {
            auto logger_ptr = this->_atomic_load_logger_ptr();
            if (!logger_ptr) { return; }

            logger_ptr->log(
                static_cast<spdlog::source_loc>(src_loc),
                static_cast<spdlog::level::level_enum>(lv),
                wmsg_sv
            );
        }

        void log(
            const level lv,
            const std::wstring_view wmsg_sv)
        {
            this->log(source_loc{}, lv, wmsg_sv);
        }

        template <typename... _Args>
        void log_fmt(
            const source_loc src_loc,
            const level lv,
            const std::wstring_view wfmt_sv,
            _Args&&... args)
        {
            auto logger_ptr = this->_atomic_load_logger_ptr();
            if (!logger_ptr) { return; }

            logger_ptr->log(
                static_cast<spdlog::source_loc>(src_loc),
                static_cast<spdlog::level::level_enum>(lv),
                fmt::runtime(wfmt_sv),
                std::forward<_Args>(args)...
            );
        }

        template <typename... _Args>
        void log_fmt(
            const level lv,
            const std::wstring_view wfmt_sv,
            _Args&&... args)
        {
            this->log_fmt(source_loc{}, lv, wfmt_sv, std::forward<_Args>(args)...);
        }
#endif // ^^^ SPDLOG_WCHAR_TO_UTF8_SUPPORT ^^^

    private:
        // If multiple threads of execution access the same std::shared_ptr object without synchronization and 
        // any of those accesses uses a non-const member function of shared_ptr then a data race will occur 
        // unless all such access is performed through these functions, which are overloads of the corresponding atomic access functions 
        // (std::atomic_load, std::atomic_store, etc.).

        inline std::shared_ptr<spdlog::logger> _atomic_load_logger_ptr() const {
            auto logger_ptr = std::atomic_load(&_logger_impl); // NOTE: deprecated in C++20
            return logger_ptr;
        }

        inline void _atomic_store_logger_ptr(std::shared_ptr<spdlog::logger> new_logger) {
            std::atomic_store(&_logger_impl, new_logger); // NOTE: deprecated in C++20
        }

    }; // class

} // namespace
_CXLIB_NAMESPACE_END