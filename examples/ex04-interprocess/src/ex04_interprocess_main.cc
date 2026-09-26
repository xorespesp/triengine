#pragma once
#include <xutl/debug/logger.hh>
#include <xutl/string/path_utils.hh>

#include "renderer_process.hh"
#include "viewer_process.hh"

#include <memory>
#include <array>
#include <iostream>
#include <conio.h>
#include <signal.h>

int main(
    [[maybe_unused]] int argc,
    [[maybe_unused]] char** argv)
{
    int retval{ -1 };

    const std::filesystem::path curr_image_dir_path{ _XUTL string::get_current_module_image_path().parent_path() };
    const bool run_as_renderer_mode = argc > 1 && std::string_view{ argv[1] } == "--renderer-mode";

    _XUTL debug::logger::construct(_XUTL debug::logger::init_options()
        .set_logger_name(run_as_renderer_mode ? "ex04-interproc-srv" : "ex04-interproc-cli")
        .enable_stdout_logging(_XUTL debug::logger::level::trace, "%^| %n(%P) | --%L-- | %s:%# | %v%$")
        .enable_async_mode()
    );

    XUTL_TRACE("Build: {}, {}", __DATE__, __TIME__);
    XUTL_TRACE("----- {}() ENTER", __func__);

    try
    {
        triengine::global_options::instance()->set_resource_directory(curr_image_dir_path / "../resources");
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

        if (run_as_renderer_mode)
        {
            XUTL_INFO("Running in renderer mode... (Press ^C to interrupt)");

            auto server = std::make_shared<renderer_process>();

            std::thread sigint_handler([server]() {
                XUTL_DEBUG("SIGINT handler thread started...");

                static std::promise<void> sigint_prm;
                ::signal(SIGINT, [](int) {
                    XUTL_WARN("SIGINT received, shutting down renderer...");
                    sigint_prm.set_value();
                });
                sigint_prm.get_future().get();

                server->stop();
            });

            server->run();

            if (sigint_handler.joinable()) {
                sigint_handler.join();
            }
        }
        else
        {
            XUTL_INFO("Running in viewer mode...");

            auto client = std::make_shared<viewer_process>();
            client->run();
        }

        XUTL_INFO("All done.");

        retval = 0;
    }
    catch (const std::exception& e)
    {
        XUTL_ERROR("----- {}() EXCEPTION -> {}", __func__, e.what());
    }
    catch (...)
    {
        XUTL_CRITICAL("----- {}() UNKNOWN EXCEPTION", __func__);
#if defined(_DEBUG)
        ::puts("\nPress <ENTER> to debug application ...");
        static_cast<void>(::getchar());
        std::exception_ptr eptr = std::current_exception();
        ::__debugbreak();
        std::rethrow_exception(eptr);
#endif // ^^^ _DEBUG ^^^
    }

    XUTL_TRACE("----- {}() LEAVE", __func__);
    
    system("pause");
    _XUTL debug::logger::destruct();
    return retval;
}