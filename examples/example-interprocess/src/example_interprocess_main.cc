#pragma once
#include <cxlib/utils/logger.hh>
#include <cxlib/utils/path_utils.hh>

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

    const std::filesystem::path curr_image_dir_path{ _CXLIB utils::get_current_module_image_path().parent_path() };
    const bool run_as_renderer_mode = argc > 1 && std::string_view{ argv[1] } == "--renderer-mode";

    _CXLIB utils::logger::instance().init(_CXLIB utils::logger::init_options()
        .set_logger_name(run_as_renderer_mode ? "example-interproc-srv" : "example-interproc-cli")
        .enable_stdout_logging(_CXLIB utils::logger::level::trace, "%^| %n(%P) | --%L-- | %s:%# | %v%$")
        .enable_async_mode()
    );

    CXLIB_TRACE("Build: {}, {}", __DATE__, __TIME__);
    CXLIB_TRACE("----- {}() ENTER", __func__);

    try
    {
        triengine::global_options::instance()->set_resource_directory(curr_image_dir_path / "../resources");
        triengine::global_options::instance()->get_logger().set_log_level(triengine::utility::log_level::trace);
        triengine::global_options::instance()->get_logger().register_print_callback(
            [](triengine::utility::log_level lv, std::string_view msg_sv)
        {
            _CXLIB utils::logger::instance().log(
                [lv]() -> _CXLIB utils::logger::level {
                switch (lv) {
                case triengine::utility::log_level::trace: return _CXLIB utils::logger::level::trace;
                case triengine::utility::log_level::debug: return _CXLIB utils::logger::level::debug;
                case triengine::utility::log_level::info: return _CXLIB utils::logger::level::info;
                case triengine::utility::log_level::warn: return _CXLIB utils::logger::level::warn;
                case triengine::utility::log_level::error: return _CXLIB utils::logger::level::error;
                case triengine::utility::log_level::critical: return _CXLIB utils::logger::level::critical;
                default: return _CXLIB utils::logger::level::warn;
                }
            }(), msg_sv);
        });

        if (run_as_renderer_mode)
        {
            CXLIB_INFO("Running in renderer mode... (Press ^C to interrupt)");

            auto server = std::make_shared<renderer_process>();

            std::thread sigint_handler([server]() {
                CXLIB_DEBUG("SIGINT handler thread started...");

                static std::promise<void> sigint_prm;
                ::signal(SIGINT, [](int) {
                    CXLIB_WARN("SIGINT received, shutting down renderer...");
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
            CXLIB_INFO("Running in viewer mode...");

            auto client = std::make_shared<viewer_process>();
            client->run();
        }

        CXLIB_INFO("All done.");

        retval = 0;
    }
    catch (const std::exception& e)
    {
        CXLIB_ERROR("----- {}() EXCEPTION -> {}", __func__, e.what());
    }
    catch (...)
    {
        CXLIB_CRITICAL("----- {}() UNKNOWN EXCEPTION", __func__);
#if defined(_DEBUG)
        ::puts("\nPress <ENTER> to debug application ...");
        static_cast<void>(::getchar());
        std::exception_ptr eptr = std::current_exception();
        ::__debugbreak();
        std::rethrow_exception(eptr);
#endif // ^^^ _DEBUG ^^^
    }

    CXLIB_TRACE("----- {}() LEAVE", __func__);
    
    system("pause");
    _CXLIB utils::logger::instance().deinit();
    return retval;
}