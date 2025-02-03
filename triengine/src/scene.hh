#pragma once
#include "common.h"
#include "camera.hh"
#include "infinite_grid_options.hh"
#include "lighting_options.hh"
#include "geometry/lineset_object.hh"
#include "geometry/pcd_object.hh"
#include "geometry/triangle_mesh_object.hh"
#include "geometry/skeleton_object.hh"

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
        camera _main_camera; // TODO: multi camera?

    public:
        scene_render_config render_config;
        std::list<std::shared_ptr<geometry::lineset_object>> lineset_objects;
        std::list<std::shared_ptr<geometry::pcd_object>> pcd_objects;
        std::list<std::shared_ptr<geometry::triangle_mesh_object>> mesh_objects;
        std::list<std::shared_ptr<geometry::skeleton_object>> skeleton_objects;

    private:
        static uint32_t _create_unique_id() {
            static std::atomic_uint32_t cnt_ = 0;
            const uint32_t new_id = cnt_++;
            return new_id;
        }

    public:
        scene() = default;
        
        uint32_t id() const noexcept { return _id; }

        const camera* get_camera() const noexcept { return &_main_camera; }
        camera* get_camera() noexcept { return &_main_camera; }

        void add_object(std::shared_ptr<geometry::geometry_object_base> object) {
            switch (object->get_type()) {
            case geometry::geometry_object_type::lineset:
                lineset_objects.push_back(std::static_pointer_cast<geometry::lineset_object>(object));
                break;
            case geometry::geometry_object_type::pointcloud:
                pcd_objects.push_back(std::static_pointer_cast<geometry::pcd_object>(object));
                break;
            case geometry::geometry_object_type::triangle_mesh:
                mesh_objects.push_back(std::static_pointer_cast<geometry::triangle_mesh_object>(object));
                break;
            case geometry::geometry_object_type::skeleton:
                skeleton_objects.push_back(std::static_pointer_cast<geometry::skeleton_object>(object));
                break;
            }
        }

        void remove_object(std::shared_ptr<geometry::geometry_object_base> object) {
            switch (object->get_type()) {
            case geometry::geometry_object_type::lineset:
                lineset_objects.remove(std::static_pointer_cast<geometry::lineset_object>(object));
                break;
            case geometry::geometry_object_type::pointcloud:
                pcd_objects.remove(std::static_pointer_cast<geometry::pcd_object>(object));
                break;
            case geometry::geometry_object_type::triangle_mesh:
                mesh_objects.remove(std::static_pointer_cast<geometry::triangle_mesh_object>(object));
                break;
            case geometry::geometry_object_type::skeleton:
                skeleton_objects.remove(std::static_pointer_cast<geometry::skeleton_object>(object));
                break;
            }
        }

        void clear_objects() {
            lineset_objects.clear();
            pcd_objects.clear();
            mesh_objects.clear();
            skeleton_objects.clear();
        }

    }; // class

} // namespace