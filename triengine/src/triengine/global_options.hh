#pragma once
#include <triengine/utility/singleton.hh>
#include <triengine/utility/spin_lock.hh>
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

    private:
        mutable utility::spin_lock _lock;
        std::filesystem::path _resource_dir_path;

    }; // class

} // namespace