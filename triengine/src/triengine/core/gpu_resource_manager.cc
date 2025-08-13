#include "gpu_resource_manager.hh"

#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

namespace triengine::core
{
    void gpu_resource_manager::request_acquire_geometry_resource(
        std::shared_ptr<geometry::geometry_object_base> object) 
    {
        command_data cmd;
        cmd.cmd_type = command_type::acquire_geometry_resource;
        cmd.obj_type = object->get_type();
        cmd.obj_id = object->get_id();

        std::scoped_lock lk{ _cmd_q_mtx };
        _cmd_q.emplace_back(std::move(cmd));
    }

    void gpu_resource_manager::request_release_geometry_resource(
        std::shared_ptr<geometry::geometry_object_base> object)
    {
        command_data cmd;
        cmd.cmd_type = command_type::release_geometry_resource;
        cmd.obj_type = object->get_type();
        cmd.obj_id = object->get_id();
        
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
        
        _mesh_rsrc_pool_alloc.collect_available_objects();
        _pcd_rsrc_pool_alloc.collect_available_objects();
        _lineset_rsrc_pool_alloc.collect_available_objects();

        for (const auto& cmd : cmd_q)
        {
            if (cmd.cmd_type == command_type::acquire_geometry_resource)
            {
                this->_acquire_geometry_resource(cmd.obj_type, cmd.obj_id);
            }
            else if (cmd.cmd_type == command_type::release_geometry_resource)
            {
                this->_release_geometry_resource(cmd.obj_type, cmd.obj_id);
            }
        } // for
    }

    // NOTE: must be called in render thread
    mesh_gpu_rsrc_ptr gpu_resource_manager::get_mesh_resource(
        const std::shared_ptr<geometry::mesh_object>& object) const
    {
        auto it = _mesh_rsrc_map.find(object->get_id());
        if (it == _mesh_rsrc_map.end()) {
            TRIENGINE_ERROR("Failed to get mesh gpu resource: %s", object->get_name().c_str());
            return nullptr; // No resources or not yet updated
        }
        return it->second;
    }

    // NOTE: must be called in render thread
    pcd_gpu_rsrc_ptr gpu_resource_manager::get_pcd_resource(
        const std::shared_ptr<geometry::pcd_object>& object) const
    {
        auto it = _pcd_rsrc_map.find(object->get_id());
        if (it == _pcd_rsrc_map.end()) {
            TRIENGINE_ERROR("Failed to get pcd gpu resource: %s", object->get_name().c_str());
            return nullptr; // No resources or not yet updated
        }
        return it->second;
    }

    // NOTE: must be called in render thread
    lineset_gpu_rsrc_ptr gpu_resource_manager::get_lineset_resource(
        const std::shared_ptr<geometry::lineset_object>& object) const
    {
        auto it = _lineset_rsrc_map.find(object->get_id());
        if (it == _lineset_rsrc_map.end()) {
            TRIENGINE_ERROR("Failed to get lineset gpu resource: %s", object->get_name().c_str());
            return nullptr; // No resources or not yet updated
        }
        return it->second;
    }

    // NOTE: must be called in render thread
    void gpu_resource_manager::_acquire_geometry_resource(
        const geometry::geometry_object_type obj_type,
        const geometry::geometry_object_id_t obj_id)
    {
        switch (obj_type) {
        case geometry::geometry_object_type::mesh: {
            const auto new_gpu_rsrc = _mesh_rsrc_pool_alloc.allocate();
            const auto [insert_it, success] = _mesh_rsrc_map.insert(
                { obj_id, new_gpu_rsrc }
            );

            if (success) {
                //TRIENGINE_TRACE("mesh_gpu_rsrc(#%llX) bound to geometry object #%llX"
                //    , new_gpu_rsrc->get_id()
                //    , obj_id
                //);
            } else {
                TRIENGINE_ERROR("Failed to bind mesh_gpu_rsrc to geometry object #%llX", obj_id);
            }

            break;
        }
        case geometry::geometry_object_type::pointcloud: {
            const auto new_gpu_rsrc = _pcd_rsrc_pool_alloc.allocate();
            const auto [insert_it, success] = _pcd_rsrc_map.insert(
                { obj_id, new_gpu_rsrc }
            );
    
            if (success) {
                //TRIENGINE_TRACE("pcd_gpu_rsrc(#%llX) bound to geometry object #%llX"
                //    , new_gpu_rsrc->get_id()
                //    , obj_id
                //);
            } else {
                TRIENGINE_ERROR("Failed to bind pcd_gpu_rsrc to geometry object #%llX", obj_id);
            }

            break;
        }
        case geometry::geometry_object_type::lineset: {
            const auto new_gpu_rsrc = _lineset_rsrc_pool_alloc.allocate();
            const auto [insert_it, success] = _lineset_rsrc_map.insert(
                { obj_id, new_gpu_rsrc }
            );

            if (success) {
                //TRIENGINE_TRACE("lineset_gpu_rsrc(#%llX) bound to geometry object #%llX"
                //    , new_gpu_rsrc->get_id()
                //    , obj_id
                //);
            } else {
                TRIENGINE_ERROR("Failed to bind lineset_gpu_rsrc to geometry object #%llX", obj_id);
            }

            break;
        }
        default: {
            TRIENGINE_PANIC("Failed to bind geometry resource (unsupported geometry type %d)"
                , static_cast<int>(obj_type)
            );
            break;
        }
        } // switch
    }

    // NOTE: must be called in render thread
    void gpu_resource_manager::_release_geometry_resource(
        const geometry::geometry_object_type obj_type,
        const geometry::geometry_object_id_t obj_id)
    {
        switch (obj_type) {
        case geometry::geometry_object_type::mesh: {
            if (const auto it = _mesh_rsrc_map.find(obj_id);
                it != _mesh_rsrc_map.end()) {
                //TRIENGINE_TRACE("Releasing mesh_gpu_rsrc(#%llX) bound to geometry object #%llX"
                //    , it->second->get_id()
                //    , obj_id
                //);
                _mesh_rsrc_pool_alloc.deallocate(it->second);
                _mesh_rsrc_map.erase(it);
            }    
            break;
        }
        case geometry::geometry_object_type::pointcloud: {
            if (const auto it = _pcd_rsrc_map.find(obj_id);
                it != _pcd_rsrc_map.end()) {
                //TRIENGINE_TRACE("Releasing pcd_gpu_rsrc(#%llX) bound to geometry object #%llX"
                //    , it->second->get_id()
                //    , obj_id
                //);
                _pcd_rsrc_pool_alloc.deallocate(it->second);
                _pcd_rsrc_map.erase(it);
            }
            break;
        }
        case geometry::geometry_object_type::lineset: {
            if (const auto it = _lineset_rsrc_map.find(obj_id);
                it != _lineset_rsrc_map.end()) {
                //TRIENGINE_TRACE("Releasing lineset_gpu_rsrc(#%llX) bound to geometry object #%llX"
                //    , it->second->get_id()
                //    , obj_id
                //);
                _lineset_rsrc_pool_alloc.deallocate(it->second);
                _lineset_rsrc_map.erase(it);
            }
            break;
        }
        default: {
            TRIENGINE_PANIC("Failed to release geometry resource (unsupported geometry type %d)"
                , static_cast<int>(obj_type)
            );
            break;
        }
        } // switch
    }

} // namespace