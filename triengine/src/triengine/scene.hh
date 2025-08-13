#pragma once
#include <triengine/common.h>
#include <triengine/camera.hh>
#include <triengine/infinite_plane_options.hh>
#include <triengine/lighting_options.hh>
#include <triengine/core/gpu_resource_manager.hh>
#include <triengine/geometry/lineset_object.hh>
#include <triengine/geometry/pcd_object.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/geometry/skeleton_object.hh>
#include <triengine/utility/noncopyable.hh>

#include <optional>
#include <memory>
#include <list>

namespace triengine
{
    enum class skeleton_render_mode
    {
        skeleton_default = 0,
        skeleton_overlay,
        overlay_with_joint_axis,
    };

    struct scene_render_config
    {
        bool show_wireframe{ false };
        bool show_object_normals{ false };
        bool show_origin_xz_grid{ false };
        bool enable_anti_aliasing{ true };

        infinite_plane_options inf_plane_opts;
        lighting_options light_opts;
        color4_f32 bg_color{ 0.020f, 0.020f, 0.020f, 1.0f };
        std::optional<float> pcd_point_size;
        skeleton_render_mode skeleton_mode{ skeleton_render_mode::skeleton_default };

        scene_render_config() = default;
    }; // struct

    using scene_id_t = uint32_t;

    // The `scene` owns and manages all the information (mesh, pcd, etc.) required for “scene composition” (including cameras)
    class scene
        : utility::noncopyable
    {
    private:
        const scene_id_t _id; // unique scene id
        std::string _name; // scene name
        std::weak_ptr<core::gpu_resource_manager> _gpu_rsrc_mgr;

        std::unordered_map<camera_type, std::unique_ptr<abstract_camera>> _camera_map; // list of available cameras in the scene
        abstract_camera* _active_camera_ptr{ nullptr };

        std::list<std::shared_ptr<geometry::lineset_object>> _lineset_geometries;
        std::list<std::shared_ptr<geometry::pcd_object>> _pcd_geometries;
        std::list<std::shared_ptr<geometry::mesh_object>> _mesh_geometries;
        std::list<std::shared_ptr<geometry::skeleton_object>> _skeleton_geometries;

        scene_render_config _render_config;

    public:
        scene(std::shared_ptr<core::gpu_resource_manager> gpu_rsrc_mgr);
        
        scene_id_t get_id() const noexcept { return _id; }

        const std::string& get_name() const noexcept { return _name; }
        void set_name(std::string name) {
            _name = std::move(name);
        }

        const scene_render_config* get_render_config() const noexcept { return &_render_config; }
        scene_render_config* get_render_config() noexcept { return &_render_config; }

        const abstract_camera* get_camera() const noexcept {
            TRIENGINE_ASSERT(_active_camera_ptr != nullptr);
            return _active_camera_ptr;
        }

        abstract_camera* get_camera() noexcept {
            TRIENGINE_ASSERT(_active_camera_ptr != nullptr);
            return _active_camera_ptr;
        }

        void switch_camera_type(camera_type cam_type) {
            auto it = _camera_map.find(cam_type);
            if (it != _camera_map.end()) {
                _active_camera_ptr = it->second.get();
            } else {
                TRIENGINE_PANIC("Camera type %d not avaiable in scene", static_cast<int>(cam_type));
            }
        }

        const auto& get_lineset_geometries() const noexcept { return _lineset_geometries; }
        auto& get_lineset_geometries() noexcept { return _lineset_geometries; }

        const auto& get_pcd_geometries() const noexcept { return _pcd_geometries; }
        auto& get_pcd_geometries() noexcept { return _pcd_geometries; }

        const auto& get_mesh_geometries() const noexcept { return _mesh_geometries; }
        auto& get_mesh_geometries() noexcept { return _mesh_geometries; }

        const auto& get_skeleton_geometries() const noexcept { return _skeleton_geometries; }
        auto& get_skeleton_geometries() noexcept { return _skeleton_geometries; }

        /*
        texture_handle_t register_texture(...);
        texture_handle_t unregister_texture(...);
        */

        void add_geometry(std::shared_ptr<geometry::geometry_object_base> geometry_object); 
        void remove_geometry(std::shared_ptr<geometry::geometry_object_base> geometry_object);
        void clear_geometries();

    }; // class

} // namespace