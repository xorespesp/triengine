#include "screen_demo_app.hh"

#include <xutl/debug/logger.hh>

#include <triengine/global_options.hh>

#include <cstdio>
#include <exception>

namespace
{
    void run_demo()
    {
        demo::screen_demo_app app;

        XUTL_INFO("Creating app..");
        app.create();

        XUTL_INFO("Running app..");
        app.run();

        XUTL_INFO("Destroying app..");
        app.destroy();
    }
} // namespace

int main(
    [[maybe_unused]] int argc,
    [[maybe_unused]] char** argv)
{
    _XUTL debug::logger::construct(_XUTL debug::logger::init_options()
        .set_logger_name("ex02-screen")
        .enable_stdout_logging(_XUTL debug::logger::level::trace)
        .enable_async_mode()
    );

    XUTL_TRACE("Build: {}, {}", __DATE__, __TIME__);
    XUTL_TRACE("----- {}() ENTER", __func__);

    int retval{ -1 };

    try
    {
        // Bridge triengine's internal logs into the example logger so that
        // texture create/update traces are visible on stdout.
        triengine::global_options::instance()->get_logger().set_log_level(triengine::utility::log_level::trace);
        const auto engine_log_subscription = triengine::global_options::instance()->get_logger().subscribe(
            [](triengine::utility::log_level lv, std::string_view msg_sv)
            {
                _XUTL debug::logger::instance()->log(
                    [lv]() -> _XUTL debug::logger::level {
                        switch (lv) {
                        case triengine::utility::log_level::trace: return _XUTL debug::logger::level::trace;
                        case triengine::utility::log_level::debug: return _XUTL debug::logger::level::debug;
                        case triengine::utility::log_level::info: return _XUTL debug::logger::level::info;
                        case triengine::utility::log_level::warn: return _XUTL debug::logger::level::warn;
                        case triengine::utility::log_level::error: return _XUTL debug::logger::level::error;
                        case triengine::utility::log_level::critical: return _XUTL debug::logger::level::critical;
                        default: return _XUTL debug::logger::level::warn;
                        }
                    }(), msg_sv);
            });

        run_demo();

        retval = 0;
    }
    catch (const std::exception& e)
    {
        XUTL_ERROR("----- {}() EXCEPTION -> {}", __func__, e.what());
    }
    catch (...)
    {
        XUTL_CRITICAL("----- {}() UNKNOWN EXCEPTION", __func__);
    }

    XUTL_TRACE("----- {}() LEAVE", __func__);

    _XUTL debug::logger::destruct();
    return retval;
}
