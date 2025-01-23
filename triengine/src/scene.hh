#pragma once
#include "common.h"
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
    struct scene_config
    {
        enum class view_layout_mode {
            one_view = 0,
            two_views,
            three_views,
        };

        enum class skeleton_render_mode {
            skeleton_default = 0,
            skeleton_overlay,
            overlay_with_joint_axis,
        };

        view_layout_mode view_layout{ view_layout_mode::one_view };
        bool show_wireframe{ false };
        bool show_object_normals{ false };
        bool show_origin_axis{ true };
        bool show_origin_xz_grid{ false };
        infinite_grid_options infgrid_opts;
        lighting_options light_opts;
        color4_f32 bg_color{ 0.05f, 0.05f, 0.05f, 1.0f };
        std::optional<float> pcd_point_size;
        skeleton_render_mode skeleton_mode{ skeleton_render_mode::skeleton_overlay };

        scene_config() = default;
    }; // struct

    // scene은 "무엇을 그리는가"에 대한 정보만 관리
    // 장면 구성에 필요한 모든 객체(mesh, pcd 등)을 소유하고 관리
    // 조명 등에 대한 정보도 같이 포함
    // 단, 카메라 정보는 관리하지 않는다.
    class scene
    {
    public:
        scene() = default;

        scene_config scn_config;
        std::list<std::shared_ptr<geometry::lineset_object>> lineset_objects;
        std::list<std::shared_ptr<geometry::pcd_object>> pcd_objects;
        std::list<std::shared_ptr<geometry::triangle_mesh_object>> mesh_objects;
        std::list<std::shared_ptr<geometry::skeleton_object>> skeleton_objects;

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