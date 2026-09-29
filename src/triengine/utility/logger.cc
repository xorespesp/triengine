#include "logger.hh"

#include <triengine/common.h>
#include <triengine/utility/spin_lock.hh>
#include <triengine/utility/debug_utils.hh>

#include <memory>
#include <utility>
#include <vector>

namespace triengine::utility
{
    static_assert(static_cast<int>(log_level::trace)    == TRIENGINE_LOG_LEVEL_TRACE,    "log level mismatch");
    static_assert(static_cast<int>(log_level::debug)    == TRIENGINE_LOG_LEVEL_DEBUG,    "log level mismatch");
    static_assert(static_cast<int>(log_level::info)     == TRIENGINE_LOG_LEVEL_INFO,     "log level mismatch");
    static_assert(static_cast<int>(log_level::warn)     == TRIENGINE_LOG_LEVEL_WARN,     "log level mismatch");
    static_assert(static_cast<int>(log_level::error)    == TRIENGINE_LOG_LEVEL_ERROR,    "log level mismatch");
    static_assert(static_cast<int>(log_level::critical) == TRIENGINE_LOG_LEVEL_CRITICAL, "log level mismatch");

    struct logger::impl
    {
        struct sink_t
        {
            log_callback_id_t id{ kInvalidLogCallbackId };
            log_callback_type cb;
        };

        using sink_list_t = std::vector<sink_t>;

        mutable utility::spin_lock _lock;

        // Copy-on-write: printing takes a snapshot of this list, so callbacks run unlocked while
        // registration/removal swaps in a fresh list.
        std::shared_ptr<const sink_list_t> _sinks;

        log_callback_id_t _next_id{ kInvalidLogCallbackId + 1 };
        std::atomic<log_level> _active_log_lv{ log_level::info };

        impl() = default;

        // NOTE: the two helpers below must be called with `_lock` held.

        log_callback_id_t add_sink(log_callback_type cb)
        {
            auto new_sinks = (_sinks != nullptr)
                ? std::make_shared<sink_list_t>(*_sinks)
                : std::make_shared<sink_list_t>();

            const log_callback_id_t new_id = _next_id++;
            new_sinks->push_back(sink_t{ new_id, std::move(cb) });
            _sinks = std::move(new_sinks);

            return new_id;
        }

        void remove_sink(const log_callback_id_t id)
        {
            if (id == kInvalidLogCallbackId || _sinks == nullptr) { return; }

            auto new_sinks = std::make_shared<sink_list_t>();
            new_sinks->reserve(_sinks->size());
            for (const sink_t& sink : *_sinks) {
                if (sink.id != id) { new_sinks->push_back(sink); }
            }
            _sinks = std::move(new_sinks);
        }
    };

    logger::subscription::subscription(
        std::weak_ptr<impl> imp,
        const log_callback_id_t id) noexcept
        : _imp{ std::move(imp) }
        , _id{ id }
    {}

    logger::subscription::~subscription()
    {
        this->unsubscribe();
    }

    logger::subscription::subscription(subscription&& rhs) noexcept
        : _imp{ std::move(rhs._imp) } // a moved-from weak_ptr is left empty
        , _id{ std::exchange(rhs._id, kInvalidLogCallbackId) }
    {}

    logger::subscription& logger::subscription::operator=(subscription&& rhs) noexcept
    {
        if (this != &rhs)
        {
            this->unsubscribe();

            _imp = std::move(rhs._imp);
            _id = std::exchange(rhs._id, kInvalidLogCallbackId);
        }
        return *this;
    }

    void logger::subscription::unsubscribe() noexcept
    {
        if (_id == kInvalidLogCallbackId) { return; }

        // an expired logger has taken its sinks with it, so there is nothing left to remove
        if (const std::shared_ptr<impl> imp = _imp.lock()) {
            std::scoped_lock lk{ imp->_lock };
            imp->remove_sink(_id);
        }

        _imp.reset();
        _id = kInvalidLogCallbackId;
    }

    void logger::subscription::detach() noexcept
    {
        // the sink stays in the logger; dropping what we need to remove it is the whole point
        _imp.reset();
        _id = kInvalidLogCallbackId;
    }

    bool logger::subscription::is_valid() const noexcept
    {
        return (_id != kInvalidLogCallbackId) && !_imp.expired();
    }

    logger::logger() : _imp{ std::make_shared<impl>() }
    {}

    logger::~logger()
    {}

    logger::subscription logger::subscribe(log_callback_type cb)
    {
        if (!cb) { return subscription{}; }

        std::scoped_lock lk{ _imp->_lock };
        return subscription{ _imp, _imp->add_sink(std::move(cb)) };
    }

    log_level logger::get_log_level() const
    {
        return _imp->_active_log_lv.load();
    }

    void logger::set_log_level(log_level lv)
    {
        _imp->_active_log_lv = lv;
    }

    void logger::_log_impl(
        const source_loc& src_loc,
        const log_level lv,
        const std::string_view msg_sv)
    {
        std::shared_ptr<const impl::sink_list_t> sinks; {
            std::scoped_lock lk{ _imp->_lock };
            if (!_imp->_sinks || _imp->_sinks->empty()) { return; }
            sinks = _imp->_sinks;
        }

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

        for (const impl::sink_t& sink : *sinks) {
            sink.cb(lv, msg);
        }
    }

} // namespace
