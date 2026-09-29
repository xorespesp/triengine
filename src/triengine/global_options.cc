#include "global_options.hh"

#include <triengine/utility/spin_lock.hh>
#include <triengine/utility/debug_utils.hh>

namespace triengine
{
    struct global_options::impl
    {
        mutable utility::spin_lock _lock;
        std::filesystem::path _resource_dir_path;
        utility::logger _logger;

        impl() = default;
    };

    global_options::global_options() : _imp{ new impl{} }
    { }
    
    global_options::~global_options()
    { }
    
    void global_options::set_resource_directory(std::filesystem::path resource_dir_path)
    {
        resource_dir_path = std::filesystem::canonical(resource_dir_path);
        if (!std::filesystem::is_directory(resource_dir_path)) {
            TRIENGINE_PANIC("Invalid resource directory path");
        }

        std::scoped_lock lk{ _imp->_lock };
        _imp->_resource_dir_path = std::move(resource_dir_path);
    }

    std::filesystem::path global_options::get_resource_directory() const
    {
        std::scoped_lock lk{ _imp->_lock };
        return _imp->_resource_dir_path;
    }

    const utility::logger& global_options::get_logger() const
    {
        //std::scoped_lock lk{ _imp->_lock };
        return _imp->_logger; // logger is thread-safe
    }

    utility::logger& global_options::get_logger()
    {
        //std::scoped_lock lk{ _imp->_lock };
        return _imp->_logger; // logger is thread-safe
    }

} // namespace