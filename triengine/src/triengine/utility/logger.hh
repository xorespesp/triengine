#pragma once
#include <triengine/utility/noncopyable.hh>
#include <triengine/utility/string_format.hh>

#include <functional>
#include <memory>

namespace triengine::utility
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
        {
        }

        constexpr bool empty() const noexcept { return filepath.empty(); }
        constexpr std::string_view filename() const {
            return filepath.substr(filepath.find_last_of("/\\") + 1); // split filename
        }

        std::string_view filepath;
        int line{};
        std::string_view funcname;
    };

    enum class log_level
    {
        trace = 0,
        debug,
        info,
        warn,
        error,
        critical,
    };

    class logger : utility::noncopyable
    {
    public:
        using print_callback_type = std::function<void(log_level, std::string_view)>;

    public:
        logger();
        ~logger();
        
        log_level get_log_level() const;
        void set_log_level(log_level lv);

        void register_print_callback(print_callback_type cb);
        void reset_print_callback();

        void print(
            log_level lv,
            std::string_view msg_sv)
        {
            if (lv >= this->get_log_level()) {
                this->_print_impl(
                    source_loc{},
                    lv,
                    msg_sv
                );
            }
        }

        void print(
            const source_loc& src_loc,
            log_level lv,
            std::string_view msg_sv)
        {
            if (lv >= this->get_log_level()) {
                this->_print_impl(
                    src_loc,
                    lv,
                    msg_sv
                );
            }
        }

        template <typename... _Args>
        void printf(
            log_level lv,
            const char* c_fmt,
            _Args&&... args)
        {
            if (lv >= this->get_log_level()) {
                this->_print_impl(
                    source_loc{},
                    lv,
                    utility::string::c_format(c_fmt, std::forward<_Args>(args)...)
                );
            }
        }

        template <typename... _Args>
        void printf(
            const source_loc& src_loc,
            log_level lv,
            const char* c_fmt,
            _Args&&... args)
        {
            if (lv >= this->get_log_level()) {
                this->_print_impl(
                    src_loc,
                    lv,
                    utility::string::c_format(c_fmt, std::forward<_Args>(args)...)
                );
            }
        }

    private:
        void _print_impl(
            const source_loc& src_loc,
            log_level lv,
            std::string_view msg_sv
        );

    private:
        struct impl_t;
        std::unique_ptr<impl_t> _impl;
    };

} // namespace