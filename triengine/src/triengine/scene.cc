#include "scene.hh"

namespace triengine
{
    void scene::add_geometry(std::shared_ptr<geometry::geometry_object_base> object_base)
    {
        auto gpu_rsrc_mgr = _gpu_rsrc_mgr.lock();
        if (!gpu_rsrc_mgr) {
            TRIENGINE_TRACE("failed to access gpu resource manager");
            return;
        }

        switch (object_base->get_type()) {
        case geometry::geometry_object_type::lineset: {
            auto object = std::static_pointer_cast<geometry::lineset_object>(object_base);
            _lineset_objects.push_back(std::static_pointer_cast<geometry::lineset_object>(object));
            break;
        }
        case geometry::geometry_object_type::pointcloud: {
            auto object = std::static_pointer_cast<geometry::pcd_object>(object_base);
            _pcd_objects.push_back(object);
            break;
        }
        case geometry::geometry_object_type::triangle_mesh: {
            auto object = std::static_pointer_cast<geometry::triangle_mesh_object>(object_base);
            _mesh_objects.push_back(object);
            gpu_rsrc_mgr->request_create_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::skeleton: {
            auto object = std::static_pointer_cast<geometry::skeleton_object>(object_base);
            _skeleton_objects.push_back(object);
            break;
        }
        }
    }

    void scene::remove_geometry(std::shared_ptr<geometry::geometry_object_base> object_base)
    {
        auto gpu_rsrc_mgr = _gpu_rsrc_mgr.lock();
        if (!gpu_rsrc_mgr) {
            TRIENGINE_TRACE("failed to access gpu resource manager");
            return;
        }

        switch (object_base->get_type()) {
        case geometry::geometry_object_type::lineset: {
            auto object = std::static_pointer_cast<geometry::lineset_object>(object_base);
            _lineset_objects.remove(object);
            break;
        }
        case geometry::geometry_object_type::pointcloud: {
            auto object = std::static_pointer_cast<geometry::pcd_object>(object_base);
            _pcd_objects.remove(object);
            break;
        }
        case geometry::geometry_object_type::triangle_mesh: {
            auto object = std::static_pointer_cast<geometry::triangle_mesh_object>(object_base);
            _mesh_objects.remove(object);
            gpu_rsrc_mgr->request_destroy_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::skeleton: {
            auto object = std::static_pointer_cast<geometry::skeleton_object>(object_base);
            _skeleton_objects.remove(object);
            break;
        }
        }
    }

    void scene::clear_geometries()
    {
        auto gpu_rsrc_mgr = _gpu_rsrc_mgr.lock();
        if (!gpu_rsrc_mgr) {
            TRIENGINE_TRACE("failed to access gpu resource manager");
            return;
        }

        for (auto& object : _lineset_objects) {
            //gpu_rsrc_mgr->request_destroy_geometry_resource(object);
        }
        _lineset_objects.clear();

        for (auto& object : _pcd_objects) {
            //gpu_rsrc_mgr->request_destroy_geometry_resource(object);
        }
        _pcd_objects.clear();
        
        for (auto& object : _mesh_objects) {
            gpu_rsrc_mgr->request_destroy_geometry_resource(object);
        }
        _mesh_objects.clear();
        
        for (auto& object : _skeleton_objects) {
            //gpu_rsrc_mgr->request_destroy_geometry_resource(object);
        }
        _skeleton_objects.clear();
    }

} // namespace