#pragma once
#include <triengine/common.h>
#include <triengine/camera.hh>
#include <triengine/infinite_grid_options.hh>
#include <triengine/lighting_options.hh>
#include <triengine/geometry/lineset_object.hh>
#include <triengine/geometry/pcd_object.hh>
#include <triengine/geometry/triangle_mesh_object.hh>
#include <triengine/geometry/skeleton_object.hh>
#include <triengine/core/gpu_resource_manager.hh>

#include <optional>
#include <memory>
#include <list>

namespace triengine
{
    struct scene_render_config
    {
        enum class skeleton_render_mode {
            skeleton_default = 0,
            skeleton_overlay,
            overlay_with_joint_axis,
        };

        bool show_wireframe{ false };
        bool show_object_normals{ false };
        bool show_origin_axis{ true };
        bool show_origin_xz_grid{ false };
        bool enable_anti_aliasing{ true };

        infinite_grid_options infgrid_opts;
        lighting_options light_opts;
        color4_f32 bg_color{ 0.05f, 0.05f, 0.05f, 1.0f };
        std::optional<float> pcd_point_size;
        skeleton_render_mode skeleton_mode{ skeleton_render_mode::skeleton_overlay };

        scene_render_config() = default;
    }; // struct

    // The `scene` owns and manages all the information (mesh, pcd, etc.) required for “scene composition” (including cameras)
    class scene
    {
    private:
        const uint32_t _id{ _create_unique_id() };
        std::weak_ptr<core::gpu_resource_manager> _gpu_rsrc_mgr;
        scene_render_config _render_config;
        camera _main_camera; // TODO: multi camera?
        std::list<std::shared_ptr<geometry::lineset_object>> _lineset_objects;
        std::list<std::shared_ptr<geometry::pcd_object>> _pcd_objects;
        std::list<std::shared_ptr<geometry::triangle_mesh_object>> _mesh_objects;
        std::list<std::shared_ptr<geometry::skeleton_object>> _skeleton_objects;

    public:

    private:
        static uint32_t _create_unique_id() {
            static std::atomic_uint32_t cnt_ = 0;
            const uint32_t new_id = cnt_++;
            return new_id;
        }

    public:
        scene(std::shared_ptr<core::gpu_resource_manager> gpu_rsrc_mgr)
            : _gpu_rsrc_mgr{ gpu_rsrc_mgr }
        {}
        
        uint32_t id() const noexcept { return _id; }

        const scene_render_config* get_render_config() const noexcept { return &_render_config; }
        scene_render_config* get_render_config() noexcept { return &_render_config; }

        const camera* get_camera() const noexcept { return &_main_camera; }
        camera* get_camera() noexcept { return &_main_camera; }

        const auto& get_lineset_geometries() const noexcept { return _lineset_objects; }
        auto& get_lineset_geometries() noexcept { return _lineset_objects; }

        const auto& get_pcd_geometries() const noexcept { return _pcd_objects; }
        auto& get_pcd_geometries() noexcept { return _pcd_objects; }

        const auto& get_mesh_geometries() const noexcept { return _mesh_objects; }
        auto& get_mesh_geometries() noexcept { return _mesh_objects; }

        const auto& get_skeleton_geometries() const noexcept { return _skeleton_objects; }
        auto& get_skeleton_geometries() noexcept { return _skeleton_objects; }

        /*
        texture_handle_t register_texture(...);
        texture_handle_t unregister_texture(...);
        */

        void add_geometry(std::shared_ptr<geometry::geometry_object_base> object_base); 
        void remove_geometry(std::shared_ptr<geometry::geometry_object_base> object_base);
        void clear_geometries();

    }; // class

} // namespace