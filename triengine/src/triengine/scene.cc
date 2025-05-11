#include "scene.hh"

#include <atomic>

namespace triengine
{
    namespace {
        scene_id_t _create_unique_scene_id() {
            static std::atomic<scene_id_t> cnt_ = 0;
            return cnt_++; // TODO: overflow check?
        }
    } // namespace
    
    scene::scene(std::shared_ptr<core::gpu_resource_manager> gpu_rsrc_mgr)
        : _id{ _create_unique_scene_id() }
        , _name{ utility::string::c_format("scene #%X", _id) }
        , _gpu_rsrc_mgr{ gpu_rsrc_mgr }
    {}

    void scene::add_geometry(std::shared_ptr<geometry::geometry_object_base> geometry_object)
    {
        auto gpu_rsrc_mgr = _gpu_rsrc_mgr.lock();
        if (!gpu_rsrc_mgr) {
            TRIENGINE_TRACE("failed to access gpu resource manager");
            return;
        }

        switch (geometry_object->get_type()) {
        case geometry::geometry_object_type::lineset: {
            auto object = std::static_pointer_cast<geometry::lineset_object>(geometry_object);
            _lineset_geometries.push_back(std::static_pointer_cast<geometry::lineset_object>(object));
            gpu_rsrc_mgr->request_acquire_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::pointcloud: {
            auto object = std::static_pointer_cast<geometry::pcd_object>(geometry_object);
            _pcd_geometries.push_back(object);
            gpu_rsrc_mgr->request_acquire_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::triangle_mesh: {
            auto object = std::static_pointer_cast<geometry::triangle_mesh_object>(geometry_object);
            _mesh_geometries.push_back(object);
            gpu_rsrc_mgr->request_acquire_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::skeleton: {
            auto object = std::static_pointer_cast<geometry::skeleton_object>(geometry_object);
            _skeleton_geometries.push_back(object);
            //gpu_rsrc_mgr->request_acquire_geometry_resource(object);
            break;
        }
        }
    }

    void scene::remove_geometry(std::shared_ptr<geometry::geometry_object_base> geometry_object)
    {
        auto gpu_rsrc_mgr = _gpu_rsrc_mgr.lock();
        if (!gpu_rsrc_mgr) {
            TRIENGINE_TRACE("failed to access gpu resource manager");
            return;
        }

        switch (geometry_object->get_type()) {
        case geometry::geometry_object_type::lineset: {
            auto object = std::static_pointer_cast<geometry::lineset_object>(geometry_object);
            object->mark_dirty();
            _lineset_geometries.remove(object);
            gpu_rsrc_mgr->request_release_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::pointcloud: {
            auto object = std::static_pointer_cast<geometry::pcd_object>(geometry_object);
            object->mark_dirty();
            _pcd_geometries.remove(object);
            gpu_rsrc_mgr->request_release_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::triangle_mesh: {
            auto object = std::static_pointer_cast<geometry::triangle_mesh_object>(geometry_object);
            object->mark_dirty();
            _mesh_geometries.remove(object);
            gpu_rsrc_mgr->request_release_geometry_resource(object);
            break;
        }
        case geometry::geometry_object_type::skeleton: {
            auto object = std::static_pointer_cast<geometry::skeleton_object>(geometry_object);
            object->mark_dirty();
            _skeleton_geometries.remove(object);
            //gpu_rsrc_mgr->request_release_geometry_resource(object);
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

        for (auto& object : _lineset_geometries) {
            object->mark_dirty();
            gpu_rsrc_mgr->request_release_geometry_resource(object);
        }
        _lineset_geometries.clear();

        for (auto& object : _pcd_geometries) {
            object->mark_dirty();
            gpu_rsrc_mgr->request_release_geometry_resource(object);
        }
        _pcd_geometries.clear();
        
        for (auto& object : _mesh_geometries) {
            object->mark_dirty();
            gpu_rsrc_mgr->request_release_geometry_resource(object);
        }
        _mesh_geometries.clear();
        
        //for (auto& object : _skeleton_geometries) {
        //    object->mark_dirty();
        //    gpu_rsrc_mgr->request_release_geometry_resource(object);
        //}
        _skeleton_geometries.clear();
    }

} // namespace