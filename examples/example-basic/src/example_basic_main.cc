#include <utils/logger.hh>
#include <utils/path_utils.hh>
#include "basic_demo_app.hh"

#include <iostream>
#include <conio.h>
 
 void run_demo(
     const std::filesystem::path& triengine_resource_dir)
 {
     triengine::global_options::instance()->set_resource_directory(triengine_resource_dir);
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
         run_demo(
             curr_image_dir_path / "../resources"
         );

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