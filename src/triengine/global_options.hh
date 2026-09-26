#pragma once
#include <triengine/common.h>
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

#define _TRIENGINE_CALL_LOGGER0(LV, STR)       ::triengine::global_options::instance()->get_logger().log(_TRIENGINE_CURRENT_SOURCE_LOC(), LV, STR)
#define _TRIENGINE_CALL_LOGGER1(LV, FSTR, ...) ::triengine::global_options::instance()->get_logger().log_fmt(_TRIENGINE_CURRENT_SOURCE_LOC(), LV, FSTR, __VA_ARGS__)
#define _TRIENGINE_LOG(LV, ...) _TRIENGINE_PP_CONCAT(_TRIENGINE_CALL_LOGGER, _TRIENGINE_PP_HAS_COMMA(__VA_ARGS__))(LV, __VA_ARGS__)

#if TRIENGINE_ACTIVE_LOG_LEVEL <= TRIENGINE_LOG_LEVEL_TRACE
#  define TRIENGINE_TRACE(...)    _TRIENGINE_LOG(::triengine::utility::log_level::trace, __VA_ARGS__)
#else
#  define TRIENGINE_TRACE(...)    ((void)0)
#endif

#if TRIENGINE_ACTIVE_LOG_LEVEL <= TRIENGINE_LOG_LEVEL_DEBUG
#  define TRIENGINE_DEBUG(...)    _TRIENGINE_LOG(::triengine::utility::log_level::debug, __VA_ARGS__)
#else
#  define TRIENGINE_DEBUG(...)    ((void)0)
#endif

#if TRIENGINE_ACTIVE_LOG_LEVEL <= TRIENGINE_LOG_LEVEL_INFO
#  define TRIENGINE_INFO(...)     _TRIENGINE_LOG(::triengine::utility::log_level::info, __VA_ARGS__)
#else
#  define TRIENGINE_INFO(...)     ((void)0)
#endif

#if TRIENGINE_ACTIVE_LOG_LEVEL <= TRIENGINE_LOG_LEVEL_WARN
#  define TRIENGINE_WARN(...)     _TRIENGINE_LOG(::triengine::utility::log_level::warn, __VA_ARGS__)
#else
#  define TRIENGINE_WARN(...)     ((void)0)
#endif

#if TRIENGINE_ACTIVE_LOG_LEVEL <= TRIENGINE_LOG_LEVEL_ERROR
#  define TRIENGINE_ERROR(...)    _TRIENGINE_LOG(::triengine::utility::log_level::error, __VA_ARGS__)
#else
#  define TRIENGINE_ERROR(...)    ((void)0)
#endif

#if TRIENGINE_ACTIVE_LOG_LEVEL <= TRIENGINE_LOG_LEVEL_CRITICAL
#  define TRIENGINE_CRITICAL(...) _TRIENGINE_LOG(::triengine::utility::log_level::critical, __VA_ARGS__)
#else
#  define TRIENGINE_CRITICAL(...) ((void)0)
#endif
