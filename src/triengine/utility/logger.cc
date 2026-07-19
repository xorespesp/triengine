#include "logger.hh"

#include <triengine/utility/spin_lock.hh>

namespace triengine::utility
{
    struct logger::impl_t
    {
        mutable utility::spin_lock lock;
        print_callback_type print_cb;
        std::atomic<log_level> active_log_lv{ log_level::info };

        impl_t() = default;
    };

    logger::logger() : _impl{ new impl_t{} }
    {}

    logger::~logger()
    {}

    void logger::register_print_callback(print_callback_type cb)
    {
        std::scoped_lock lk{ _impl->lock };
        _impl->print_cb = std::move(cb);
    }

    void logger::reset_print_callback()
    {
        std::scoped_lock lk{ _impl->lock };
        _impl->print_cb = nullptr;
    }

    log_level logger::get_log_level() const
    {
        return _impl->active_log_lv.load();
    }

    void logger::set_log_level(log_level lv)
    {
        _impl->active_log_lv = lv;
    }

    void logger::_print_impl(
        const source_loc& src_loc, 
        const log_level lv, 
        const std::string_view msg_sv)
    {
        std::string msg;

        if (!src_loc.empty())
        {
            std::string_view src_filename = src_loc.filename();
            msg = utility::string::c_format(""
                "[%.*s:%d@%.*s] %.*s"
                , static_cast<int>(src_filename.size())
                , src_filename.data()
                , src_loc.line
                , static_cast<int>(src_loc.funcname.size())
                , src_loc.funcname.data()
                , static_cast<int>(msg_sv.size())
                , msg_sv.data()
            );
        }
        else
        {
            msg = utility::string::c_format(""
                "%.*s"
                , static_cast<int>(msg_sv.size())
                , msg_sv.data()
            );
        }

        std::scoped_lock lk{ _impl->lock };
        if (_impl->print_cb) {
            _impl->print_cb(lv, msg);
        }
    }

} // namespace