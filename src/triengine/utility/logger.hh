#pragma once
#include <triengine/utility/noncopyable.hh>
#include <triengine/utility/string_format.hh>

#include <cstdint>
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
    private:
        struct impl_t;
        using log_callback_id_t = uint32_t;
        static constexpr log_callback_id_t kInvalidLogCallbackId{ 0 };

    public:
        using log_callback_type = std::function<void(log_level, const std::string&)>;

        // Keeps a log callback registered: the callback is removed once this subscription is
        // unsubscribed or destroyed, so it must be stored for as long as the callback should stay
        // alive. Outliving the logger is safe.
        class subscription {
        public:
            subscription() = default;
            ~subscription();
            subscription(subscription&& rhs) noexcept;
            subscription& operator=(subscription&& rhs) noexcept;
            subscription(const subscription&) = delete;
            subscription& operator=(const subscription&) = delete;

            // Removes the callback. Does nothing when this subscription holds none.
            void unsubscribe() noexcept;

            // Gives up managing the callback: it stays registered until the logger itself dies, and nothing can remove it afterwards.
            // Only for a callback whose captures stay valid until the process ends.
            void detach() noexcept;

            bool is_valid() const noexcept;
            explicit operator bool() const noexcept { return this->is_valid(); }

        private:
            friend class logger;

            subscription(std::weak_ptr<impl_t> impl, log_callback_id_t id) noexcept;

            std::weak_ptr<impl_t> _impl;
            log_callback_id_t _id{ kInvalidLogCallbackId };
        };

    public:
        logger();
        ~logger();

        log_level get_log_level() const;
        void set_log_level(log_level lv);

        // Subscribes a log callback, kept registered by the returned subscription.
        // Callbacks run in registration order, on the thread that logged, with the logger unlocked.
        [[nodiscard]] subscription subscribe(log_callback_type cb);

        void log(
            log_level lv,
            std::string_view msg_sv)
        {
            if (lv >= this->get_log_level()) {
                this->_log_impl(
                    source_loc{},
                    lv,
                    msg_sv
                );
            }
        }

        void log(
            const source_loc& src_loc,
            log_level lv,
            std::string_view msg_sv)
        {
            if (lv >= this->get_log_level()) {
                this->_log_impl(
                    src_loc,
                    lv,
                    msg_sv
                );
            }
        }

        template <typename... _Args>
        void log_fmt(
            log_level lv,
            const char* c_fmt,
            _Args&&... args)
        {
            if (lv >= this->get_log_level()) {
                this->_log_impl(
                    source_loc{},
                    lv,
                    utility::string::c_format(c_fmt, std::forward<_Args>(args)...)
                );
            }
        }

        template <typename... _Args>
        void log_fmt(
            const source_loc& src_loc,
            log_level lv,
            const char* c_fmt,
            _Args&&... args)
        {
            if (lv >= this->get_log_level()) {
                this->_log_impl(
                    src_loc,
                    lv,
                    utility::string::c_format(c_fmt, std::forward<_Args>(args)...)
                );
            }
        }

    private:
        void _log_impl(
            const source_loc& src_loc,
            log_level lv,
            std::string_view msg_sv
        );

    private:
        std::shared_ptr<impl_t> _impl;
    };

} // namespace