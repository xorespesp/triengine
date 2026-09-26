#include "logger.hh"

#include <triengine/utility/spin_lock.hh>
#include <triengine/utility/debug_utils.hh>

#include <memory>
#include <utility>
#include <vector>

namespace triengine::utility
{
    struct logger::impl_t
    {
        struct sink_t
        {
            log_callback_id_t id{ kInvalidLogCallbackId };
            log_callback_type cb;
        };

        using sink_list_t = std::vector<sink_t>;

        mutable utility::spin_lock lock;

        // Copy-on-write: printing takes a snapshot of this list, so callbacks run unlocked while
        // registration/removal swaps in a fresh list.
        std::shared_ptr<const sink_list_t> sinks;

        log_callback_id_t next_id{ kInvalidLogCallbackId + 1 };
        std::atomic<log_level> active_log_lv{ log_level::info };

        impl_t() = default;

        // NOTE: the two helpers below must be called with `lock` held.

        log_callback_id_t add_sink(log_callback_type cb)
        {
            auto new_sinks = (sinks != nullptr)
                ? std::make_shared<sink_list_t>(*sinks)
                : std::make_shared<sink_list_t>();

            const log_callback_id_t new_id = next_id++;
            new_sinks->push_back(sink_t{ new_id, std::move(cb) });
            sinks = std::move(new_sinks);

            return new_id;
        }

        void remove_sink(const log_callback_id_t id)
        {
            if (id == kInvalidLogCallbackId || sinks == nullptr) { return; }

            auto new_sinks = std::make_shared<sink_list_t>();
            new_sinks->reserve(sinks->size());
            for (const sink_t& sink : *sinks) {
                if (sink.id != id) { new_sinks->push_back(sink); }
            }
            sinks = std::move(new_sinks);
        }
    };

    logger::subscription::subscription(
        std::weak_ptr<impl_t> impl,
        const log_callback_id_t id) noexcept
        : _impl{ std::move(impl) }
        , _id{ id }
    {}

    logger::subscription::~subscription()
    {
        this->unsubscribe();
    }

    logger::subscription::subscription(subscription&& rhs) noexcept
        : _impl{ std::move(rhs._impl) } // a moved-from weak_ptr is left empty
        , _id{ std::exchange(rhs._id, kInvalidLogCallbackId) }
    {}

    logger::subscription& logger::subscription::operator=(subscription&& rhs) noexcept
    {
        if (this != &rhs)
        {
            this->unsubscribe();

            _impl = std::move(rhs._impl);
            _id = std::exchange(rhs._id, kInvalidLogCallbackId);
        }
        return *this;
    }

    void logger::subscription::unsubscribe() noexcept
    {
        if (_id == kInvalidLogCallbackId) { return; }

        // an expired logger has taken its sinks with it, so there is nothing left to remove
        if (const std::shared_ptr<impl_t> impl = _impl.lock()) {
            std::scoped_lock lk{ impl->lock };
            impl->remove_sink(_id);
        }

        _impl.reset();
        _id = kInvalidLogCallbackId;
    }

    void logger::subscription::detach() noexcept
    {
        // the sink stays in the logger; dropping what we need to remove it is the whole point
        _impl.reset();
        _id = kInvalidLogCallbackId;
    }

    bool logger::subscription::is_valid() const noexcept
    {
        return (_id != kInvalidLogCallbackId) && !_impl.expired();
    }

    logger::logger() : _impl{ std::make_shared<impl_t>() }
    {}

    logger::~logger()
    {}

    logger::subscription logger::subscribe(log_callback_type cb)
    {
        if (!cb) { return subscription{}; }

        std::scoped_lock lk{ _impl->lock };
        return subscription{ _impl, _impl->add_sink(std::move(cb)) };
    }

    log_level logger::get_log_level() const
    {
        return _impl->active_log_lv.load();
    }

    void logger::set_log_level(log_level lv)
    {
        _impl->active_log_lv = lv;
    }

    void logger::_log_impl(
        const source_loc& src_loc,
        const log_level lv,
        const std::string_view msg_sv)
    {
        std::shared_ptr<const impl_t::sink_list_t> sinks; {
            std::scoped_lock lk{ _impl->lock };
            if (!_impl->sinks || _impl->sinks->empty()) { return; }
            sinks = _impl->sinks;
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

        for (const impl_t::sink_t& sink : *sinks) {
            sink.cb(lv, msg);
        }
    }

} // namespace
