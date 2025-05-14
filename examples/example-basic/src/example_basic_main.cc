#include <utils/logger.hh>
#include <utils/path_utils.hh>
#include "basic_demo_app.hh"

#include <iostream>
#include <conio.h>
 
 void run_demo()
 {
     gui::basic_demo_app app;

     LOG_INFO("Creating app..");
     app.create();

     LOG_INFO("Running app..");
     app.run();

     LOG_INFO("Destroying app..");
     app.destroy();
 }

 int main(int argc, char** argv)
 {
     int retval = -1;

     const std::filesystem::path curr_image_dir_path{ utils::get_current_module_image_path().parent_path() };
     //::SetCurrentDirectoryW(curr_image_dir_path.c_str());
     ::SetConsoleOutputCP(CP_UTF8); // https://github.com/gabime/spdlog/issues/762

     utils::logger::instance().init(utils::logger::init_option()
         .set_logger_name("example-basic")
         .set_logger_level(utils::logger::level::trace)
         .enable_stdout_logging()
         .enable_async_mode()
     );

     LOG_TRACE("Build: " __DATE__ ", " __TIME__);
     LOG_TRACE("----- %s() ENTER", __func__);

     try
     {
         triengine::global_options::instance()->set_resource_directory(curr_image_dir_path / "../resources");
         triengine::global_options::instance()->get_logger().set_log_level(triengine::utility::log_level::trace);
         triengine::global_options::instance()->get_logger().register_print_callback(
             [](triengine::utility::log_level lv, std::string_view msg_sv)
             {
                 ::utils::logger::instance().print(
                     [lv]() -> ::utils::logger::level {
                         switch (lv) {
                         case triengine::utility::log_level::trace: return ::utils::logger::level::trace;
                         case triengine::utility::log_level::debug: return ::utils::logger::level::debug;
                         case triengine::utility::log_level::info: return ::utils::logger::level::info;
                         case triengine::utility::log_level::warn: return ::utils::logger::level::warn;
                         case triengine::utility::log_level::error: return ::utils::logger::level::error;
                         case triengine::utility::log_level::critical: return ::utils::logger::level::critical;
                         default: return ::utils::logger::level::warn;
                         }
                     }(), msg_sv);
             });

         run_demo();

         retval = 0;
     }
     catch (const std::exception& e)
     {
         LOG_ERROR("----- %s() EXCEPTION -> %s", __func__, e.what());
     }
     catch (...)
     {
         LOG_CRITICAL("----- %s() UNKNOWN EXCEPTION", __func__);
#if defined(_DEBUG)
         ::puts("\nPress any key to debug application ...");
         static_cast<void>(::_getch());
         std::exception_ptr eptr = std::current_exception();
         ::__debugbreak();
         std::rethrow_exception(eptr);
#endif // ^^^ _DEBUG ^^^
     }

     LOG_TRACE("----- %s() LEAVE", __func__);

     utils::logger::instance().deinit();
     return retval;
 }