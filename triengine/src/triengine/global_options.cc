#include "global_options.hh"

#include <triengine/utility/debug_utils.hh>

namespace triengine
{
    global_options::global_options()
    { }
    
    global_options::~global_options()
    { }
    
    void global_options::set_resource_directory(std::filesystem::path resource_dir_path)
    {
        resource_dir_path = std::filesystem::canonical(resource_dir_path);
        if (!std::filesystem::is_directory(resource_dir_path)) {
            TRIENGINE_PANIC("Invalid resource directory path");
        }

        std::scoped_lock lk{ _lock };
        _resource_dir_path = std::move(resource_dir_path);
    }

    std::filesystem::path global_options::get_resource_directory() const
    {
        std::scoped_lock lk{ _lock };
        return _resource_dir_path;
    }

} // namespace