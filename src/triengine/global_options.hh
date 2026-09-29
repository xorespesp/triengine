#pragma once
#include <triengine/utility/singleton.hh>
#include <triengine/utility/logger.hh>

#include <filesystem>
#include <memory>

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
        struct impl;
        std::unique_ptr<impl> _imp;
    }; // class

} // namespace
