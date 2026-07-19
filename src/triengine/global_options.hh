#pragma once
#include <triengine/utility/singleton.hh>
#include <triengine/utility/logger.hh>

#include <filesystem>

namespace triengine
{
    /**
     * Singleton class that manages global settings of the engine, etc.
     * Please be aware that access may occur from multiple engine instances due to the singleton nature.
     */
    class global_options final 
        : public utility::singleton_trait<global_options>
    {
    public:
        global_options();
        ~global_options();

        void set_resource_directory(std::filesystem::path resource_dir_path);
        std::filesystem::path get_resource_directory() const;

        const utility::logger& get_logger() const;
        utility::logger& get_logger();

    private:
        struct impl_t;
        std::unique_ptr<impl_t> _impl;
    }; // class

} // namespace

#define _TRIENGINE_CURRENT_SOURCE_LOC() ::triengine::utility::source_loc{ __FILE__, __LINE__, __func__ }

#if defined(TRIENGINE_DEBUG_MODE)
#  define _TRIENGINE_CALL_LOGGER0(LV, STR)       ::triengine::global_options::instance()->get_logger().print(_TRIENGINE_CURRENT_SOURCE_LOC(), LV, STR)
#  define _TRIENGINE_CALL_LOGGER1(LV, FSTR, ...) ::triengine::global_options::instance()->get_logger().printf(_TRIENGINE_CURRENT_SOURCE_LOC(), LV, FSTR, __VA_ARGS__)
#  define TRIENGINE_TRACE(...)    _TRIENGINE_PP_CONCAT(_TRIENGINE_CALL_LOGGER, _TRIENGINE_PP_HAS_COMMA(__VA_ARGS__))(::triengine::utility::log_level::trace, __VA_ARGS__)
#  define TRIENGINE_DEBUG(...)    _TRIENGINE_PP_CONCAT(_TRIENGINE_CALL_LOGGER, _TRIENGINE_PP_HAS_COMMA(__VA_ARGS__))(::triengine::utility::log_level::debug, __VA_ARGS__)
#  define TRIENGINE_INFO(...)     _TRIENGINE_PP_CONCAT(_TRIENGINE_CALL_LOGGER, _TRIENGINE_PP_HAS_COMMA(__VA_ARGS__))(::triengine::utility::log_level::info, __VA_ARGS__)
#  define TRIENGINE_WARN(...)     _TRIENGINE_PP_CONCAT(_TRIENGINE_CALL_LOGGER, _TRIENGINE_PP_HAS_COMMA(__VA_ARGS__))(::triengine::utility::log_level::warn, __VA_ARGS__)
#  define TRIENGINE_ERROR(...)    _TRIENGINE_PP_CONCAT(_TRIENGINE_CALL_LOGGER, _TRIENGINE_PP_HAS_COMMA(__VA_ARGS__))(::triengine::utility::log_level::error, __VA_ARGS__)
#  define TRIENGINE_CRITICAL(...) _TRIENGINE_PP_CONCAT(_TRIENGINE_CALL_LOGGER, _TRIENGINE_PP_HAS_COMMA(__VA_ARGS__))(::triengine::utility::log_level::critical, __VA_ARGS__)
#else  // ^^^ TRIENGINE_DEBUG_MODE ^^^ / vvv !TRIENGINE_DEBUG_MODE vvv
#  define TRIENGINE_TRACE(...)    
#  define TRIENGINE_DEBUG(...)    
#  define TRIENGINE_INFO(...)     
#  define TRIENGINE_WARN(...)     
#  define TRIENGINE_ERROR(...)    
#  define TRIENGINE_CRITICAL(...) 
#endif // ^^^ !TRIENGINE_DEBUG_MODE ^^^
