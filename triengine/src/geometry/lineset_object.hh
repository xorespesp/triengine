#pragma once
#include "geometry_object_base.hh"

#include <vector>
#include <memory>

namespace triengine::geometry
{
    // Refs:
    // https://github.com/isl-org/Open3D/blob/main/cpp/open3d/geometry/LineSet.h

    class lineset_object
        : public geometry_object_base
    {
    public:
        std::vector<vec3_f32> line_points; /// Points coordinates. (line vertices)
        std::vector<vec2_i32> line_indices;  /// Lines denoted by the index of points forming the line. (line indices)
        std::vector<color3_f32> line_colors; /// RGB colors of lines.

    public:
        lineset_object()
            : geometry_object_base{ geometry_object_type::lineset }
        {}

        void paint_uniform_color(const color3_f32& color) {
            line_colors.clear();
            line_colors.resize(line_points.size(), color);
        }

    public:
        static void create_xz_plane(
            lineset_object& object/* out */,
            const float plane_size/* width and height; Unit: [m] */,
            const int num_of_grids
        );

        static std::shared_ptr<lineset_object> create_xz_plane(
            const float plane_size/* width and height; Unit: [m] */,
            const int num_of_grids)
        {
            auto new_object = std::make_shared<lineset_object>();
            create_xz_plane(*new_object, plane_size, num_of_grids);
            return new_object;
        }

        static void create_cube(
            lineset_object& object/* out */,
            const vec3_f32& cube_center,
            const vec3_f32& cube_size
        );

        static std::shared_ptr<lineset_object> create_cube(
            const vec3_f32& cube_center,
            const vec3_f32& cube_size)
        {
            auto new_object = std::make_shared<lineset_object>();
            create_cube(*new_object, cube_center, cube_size);
            return new_object;
        }

    }; // class

} // namespace