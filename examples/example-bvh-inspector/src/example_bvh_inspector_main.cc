#include "bvh_inspector_app.hh"

#include <iostream>
 
 void run_demo()
 {
     demo::bvh_inspector_app app;

     CXLIB_INFO("Creating app..");
     app.create();

     CXLIB_INFO("Running app..");
     app.run();

     CXLIB_INFO("Destroying app..");
     app.destroy();
 }

 int main(
     [[maybe_unused]] int argc, 
     [[maybe_unused]] char** argv)
 {
     _CXLIB utils::logger::instance().init(_CXLIB utils::logger::init_options()
         .set_logger_name("example-bvh-inspector")
         .enable_stdout_logging(_CXLIB utils::logger::level::trace)
         .enable_async_mode()
     );

     CXLIB_TRACE("Build: {}, {}", __DATE__, __TIME__);
     CXLIB_TRACE("----- {}() ENTER", __func__);

     int retval{ -1 };

     try
     {
         //if (const auto resources_dir = _CXLIB utils::get_current_module_image_path().parent_path() / "../resources";
         //    std::filesystem::is_directory(resources_dir)) {
         //    triengine::global_options::instance()->set_resource_directory(resources_dir);
         //}

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

         run_demo();

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

     _CXLIB utils::logger::instance().deinit();
     return retval;
 }