#include "gpu_resource_manager.hh"

#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

#include <atomic>

namespace triengine::core
{
    namespace {
        texture_handle_t _create_unique_texture_handle() {
            static std::atomic<texture_handle_t> cnt_ = 0;
            return cnt_++; // TODO: overflow check?
        }
    } // namespace

    gpu_resource_manager::~gpu_resource_manager()
    {
        TRIENGINE_TRACE("Destroying all GPU resources...");
        // Cleanup all GPU resources
        _mesh_geometry_rsrc_map.clear();
        _pcd_geometry_rsrc_map.clear();
        _lineset_geometry_rsrc_map.clear();
        _tex2d_rsrc_map.clear();
    }

    void gpu_resource_manager::request_acquire_geometry_resource(
        std::shared_ptr<geometry::geometry_object_base> geometry_object) 
    {
        command_data cmd;
        cmd.cmd_type = command_type::acquire_geometry_resource;
        auto& cmd_data = cmd.cmd_data.emplace<acquire_geometry_resource_command_data>();
        cmd_data.obj_type = geometry_object->get_type();
        cmd_data.obj_id = geometry_object->get_id();

        std::scoped_lock lk{ _cmd_q_mtx };
        _cmd_q.emplace_back(std::move(cmd));
    }

    void gpu_resource_manager::request_release_geometry_resource(
        std::shared_ptr<geometry::geometry_object_base> geometry_object)
    {
        command_data cmd;
        cmd.cmd_type = command_type::release_geometry_resource;
        auto& cmd_data = cmd.cmd_data.emplace<release_geometry_resource_command_data>();
        cmd_data.obj_type = geometry_object->get_type();
        cmd_data.obj_id = geometry_object->get_id();
        
        std::scoped_lock lk{ _cmd_q_mtx };
        _cmd_q.emplace_back(std::move(cmd));
    }

    texture_handle_t gpu_resource_manager::request_create_texture_resource(
        const std::shared_ptr<image_buffer>& tex_image,
        const texture_params_t& tex_params)
    {
        const texture_handle_t new_tex_handle = _create_unique_texture_handle();

        TRIENGINE_TRACE(
            "Request to create texture resource: handle #%llX, size=(%d x %d), format=%d"
            , new_tex_handle
            , tex_image->width_pixels()
            , tex_image->height_pixels()
            , static_cast<int>(tex_image->format())
        );

        command_data cmd;
        cmd.cmd_type = command_type::create_texture_resource;
        auto& cmd_data = cmd.cmd_data.emplace<create_texture_resource_command_data>();
        cmd_data.tex_handle = new_tex_handle;
        cmd_data.tex_image = tex_image;
        cmd_data.tex_params = tex_params;

        std::scoped_lock lk{ _cmd_q_mtx };
        _cmd_q.emplace_back(std::move(cmd));
        return new_tex_handle;
    }

    void gpu_resource_manager::request_destroy_texture_resource(
        texture_handle_t tex_handle)
    {
        command_data cmd;
        cmd.cmd_type = command_type::destroy_texture_resource;
        auto& cmd_data = cmd.cmd_data.emplace<destroy_texture_resource_command_data>();
        cmd_data.tex_handle = tex_handle;

        std::scoped_lock lk{ _cmd_q_mtx };
        _cmd_q.emplace_back(std::move(cmd));
    }

    // NOTE: must be called in render thread
    void gpu_resource_manager::process_pending_requests()
    {
        std::deque<command_data> cmd_q; {
            std::scoped_lock lk{ _cmd_q_mtx };    
            if (_cmd_q.empty()) { return; }
            cmd_q.swap(_cmd_q); // swap queue (minimize lock time)
        }
        
        _mesh_geometry_rsrc_pool_alloc.collect_available_objects();
        _pcd_geometry_rsrc_pool_alloc.collect_available_objects();
        _lineset_geometry_rsrc_pool_alloc.collect_available_objects();

        for (const auto& cmd : cmd_q)
        {
            switch (cmd.cmd_type) {
            case command_type::acquire_geometry_resource:
                this->_handle_acquire_geometry_resource_command(
                    std::get<acquire_geometry_resource_command_data>(cmd.cmd_data)
                );
                break;
            case command_type::release_geometry_resource:
                this->_handle_release_geometry_resource_command(
                    std::get<release_geometry_resource_command_data>(cmd.cmd_data)
                );
                break;
            case command_type::create_texture_resource:
                this->_handle_create_texture_resource_command(
                    std::get<create_texture_resource_command_data>(cmd.cmd_data)
                );
                break;
            case command_type::destroy_texture_resource:
                this->_handle_destroy_texture_resource_command(
                    std::get<destroy_texture_resource_command_data>(cmd.cmd_data)
                );
                break;
            } // switch
        } // for
    }

    // NOTE: must be called in render thread
    mesh_geometry_gpu_rsrc_ptr gpu_resource_manager::get_mesh_geometry_resource(
        const std::shared_ptr<geometry::mesh_object>& geometry_object) const
    {
        auto it = _mesh_geometry_rsrc_map.find(geometry_object->get_id());
        if (it == _mesh_geometry_rsrc_map.end()) {
            TRIENGINE_ERROR("Failed to get mesh gpu resource: %s", geometry_object->get_name().c_str());
            return nullptr; // No resources or not yet updated
        }
        return it->second;
    }

    // NOTE: must be called in render thread
    pcd_geometry_gpu_rsrc_ptr gpu_resource_manager::get_pcd_geometry_resource(
        const std::shared_ptr<geometry::pcd_object>& geometry_object) const
    {
        auto it = _pcd_geometry_rsrc_map.find(geometry_object->get_id());
        if (it == _pcd_geometry_rsrc_map.end()) {
            TRIENGINE_ERROR("Failed to get pcd gpu resource: %s", geometry_object->get_name().c_str());
            return nullptr; // No resources or not yet updated
        }
        return it->second;
    }

    // NOTE: must be called in render thread
    lineset_geometry_gpu_rsrc_ptr gpu_resource_manager::get_lineset_geometry_resource(
        const std::shared_ptr<geometry::lineset_object>& geometry_object) const
    {
        auto it = _lineset_geometry_rsrc_map.find(geometry_object->get_id());
        if (it == _lineset_geometry_rsrc_map.end()) {
            TRIENGINE_ERROR("Failed to get lineset gpu resource: %s", geometry_object->get_name().c_str());
            return nullptr; // No resources or not yet updated
        }
        return it->second;
    }

    // NOTE: must be called in render thread
    texture_2d_ptr gpu_resource_manager::get_texture_2d_resource(
        texture_handle_t tex_handle) const
    {
        auto it = _tex2d_rsrc_map.find(tex_handle);
        if (it == _tex2d_rsrc_map.end()) {
            TRIENGINE_ERROR("Failed to get texture_2d resource: handle #%llX", tex_handle);
            return nullptr; // No resources or not yet created
        }
        return it->second;
    }

    // NOTE: must be called in render thread
    void gpu_resource_manager::_handle_acquire_geometry_resource_command(
        const acquire_geometry_resource_command_data& cmd_data)
    {
        switch (cmd_data.obj_type) {
        case geometry::geometry_object_type::mesh: {
            const auto new_gpu_rsrc = _mesh_geometry_rsrc_pool_alloc.allocate();
            const auto [insert_it, success] = _mesh_geometry_rsrc_map.insert(
                { cmd_data.obj_id, new_gpu_rsrc }
            );

            if (success) {
                //TRIENGINE_TRACE("mesh_gpu_rsrc(#%llX) bound to geometry object #%llX"
                //    , new_gpu_rsrc->get_id()
                //    , cmd_data.obj_id
                //);
            } else {
                TRIENGINE_ERROR("Failed to bind mesh_gpu_rsrc to geometry object #%llX", cmd_data.obj_id);
            }

            break;
        }
        case geometry::geometry_object_type::pointcloud: {
            const auto new_gpu_rsrc = _pcd_geometry_rsrc_pool_alloc.allocate();
            const auto [insert_it, success] = _pcd_geometry_rsrc_map.insert(
                { cmd_data.obj_id, new_gpu_rsrc }
            );
    
            if (success) {
                //TRIENGINE_TRACE("pcd_gpu_rsrc(#%llX) bound to geometry object #%llX"
                //    , new_gpu_rsrc->get_id()
                //    , cmd_data.obj_id
                //);
            } else {
                TRIENGINE_ERROR("Failed to bind pcd_gpu_rsrc to geometry object #%llX", cmd_data.obj_id);
            }

            break;
        }
        case geometry::geometry_object_type::lineset: {
            const auto new_gpu_rsrc = _lineset_geometry_rsrc_pool_alloc.allocate();
            const auto [insert_it, success] = _lineset_geometry_rsrc_map.insert(
                { cmd_data.obj_id, new_gpu_rsrc }
            );

            if (success) {
                //TRIENGINE_TRACE("lineset_gpu_rsrc(#%llX) bound to geometry object #%llX"
                //    , new_gpu_rsrc->get_id()
                //    , cmd_data.obj_id
                //);
            } else {
                TRIENGINE_ERROR("Failed to bind lineset_gpu_rsrc to geometry object #%llX", cmd_data.obj_id);
            }

            break;
        }
        default: {
            TRIENGINE_PANIC("Failed to bind geometry resource (unsupported geometry type %d)"
                , static_cast<int>(cmd_data.obj_type)
            );
            break;
        }
        } // switch
    }

    // NOTE: must be called in render thread
    void gpu_resource_manager::_handle_release_geometry_resource_command(
        const release_geometry_resource_command_data& cmd_data)
    {
        switch (cmd_data.obj_type) {
        case geometry::geometry_object_type::mesh: {
            if (const auto it = _mesh_geometry_rsrc_map.find(cmd_data.obj_id);
                it != _mesh_geometry_rsrc_map.end()) {
                //TRIENGINE_TRACE("Releasing mesh_gpu_rsrc(#%llX) bound to geometry object #%llX"
                //    , it->second->get_id()
                //    , cmd_data.obj_id
                //);
                _mesh_geometry_rsrc_pool_alloc.deallocate(it->second);
                _mesh_geometry_rsrc_map.erase(it);
            }    
            break;
        }
        case geometry::geometry_object_type::pointcloud: {
            if (const auto it = _pcd_geometry_rsrc_map.find(cmd_data.obj_id);
                it != _pcd_geometry_rsrc_map.end()) {
                //TRIENGINE_TRACE("Releasing pcd_gpu_rsrc(#%llX) bound to geometry object #%llX"
                //    , it->second->get_id()
                //    , cmd_data.obj_id
                //);
                _pcd_geometry_rsrc_pool_alloc.deallocate(it->second);
                _pcd_geometry_rsrc_map.erase(it);
            }
            break;
        }
        case geometry::geometry_object_type::lineset: {
            if (const auto it = _lineset_geometry_rsrc_map.find(cmd_data.obj_id);
                it != _lineset_geometry_rsrc_map.end()) {
                //TRIENGINE_TRACE("Releasing lineset_gpu_rsrc(#%llX) bound to geometry object #%llX"
                //    , it->second->get_id()
                //    , cmd_data.obj_id
                //);
                _lineset_geometry_rsrc_pool_alloc.deallocate(it->second);
                _lineset_geometry_rsrc_map.erase(it);
            }
            break;
        }
        default: {
            TRIENGINE_PANIC("Failed to release geometry resource (unsupported geometry type %d)"
                , static_cast<int>(cmd_data.obj_type)
            );
            break;
        }
        } // switch
    }

    void gpu_resource_manager::_handle_create_texture_resource_command(
        const create_texture_resource_command_data& cmd_data)
    {
        if (!cmd_data.tex_image || cmd_data.tex_image->empty()) {
            TRIENGINE_PANIC("Invalid image provided for texture handle #%llX", cmd_data.tex_handle);
        }

        auto new_tex2d_rsrc = std::make_shared<texture_2d>(
            *cmd_data.tex_image,
            cmd_data.tex_params
        );

        const auto [
            insert_it, 
            success
        ] = _tex2d_rsrc_map.insert({ cmd_data.tex_handle, new_tex2d_rsrc });

        if (!success) {
            TRIENGINE_PANIC("Failed to create texture_2d for texture handle #%llX", cmd_data.tex_handle);
        }

        TRIENGINE_TRACE("texture_2d(#%llX) created for texture handle #%llX"
            , new_tex2d_rsrc->id()
            , cmd_data.tex_handle
        );
    }

    void gpu_resource_manager::_handle_destroy_texture_resource_command(
        const destroy_texture_resource_command_data& cmd_data)
    {
        TRIENGINE_TRACE("Destroying texture_2d for texture handle #%llX", cmd_data.tex_handle);

        auto it = _tex2d_rsrc_map.find(cmd_data.tex_handle);
        if (it == _tex2d_rsrc_map.end()) {
            TRIENGINE_WARN("Texture handle #%llX not found in texture resource map", cmd_data.tex_handle);
            return;
        }

        _tex2d_rsrc_map.erase(it);
    }

} // namespace