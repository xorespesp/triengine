#include "lineset_object.hh"

namespace triengine::geometry
{
    void lineset_object::create_xz_plane(
        lineset_object& object, 
        const float plane_size, 
        const int num_of_grids)
    {
        auto& points = object.line_points;
        auto& lines = object.line_indices;

        points.clear();
        points.reserve((num_of_grids + 1) * 2);

        lines.clear();
        lines.reserve(num_of_grids * 4);

        const float
            grid_interval = plane_size / static_cast<float>(num_of_grids),
            center_offset = plane_size / 2.0f;

        for (int row = 0; row <= num_of_grids; ++row) {
            for (int col = 0; col <= num_of_grids; ++col) {
                points.emplace_back(
                    /* x */static_cast<float>(col) * grid_interval - center_offset,
                    /* y */0.0f,
                    /* z */static_cast<float>(row) * grid_interval - center_offset
                );

                if (col < num_of_grids) {
                    lines.emplace_back(
                        row * (num_of_grids + 1) + col,
                        row * (num_of_grids + 1) + col + 1
                    );
                }

                if (row < num_of_grids) {
                    lines.emplace_back(
                        row * (num_of_grids + 1) + col,
                        (row + 1) * (num_of_grids + 1) + col
                    );
                }
            }
        }
    }

    void lineset_object::create_cube(
        lineset_object& object,
        const vec3_f32& cube_center,
        const vec3_f32& cube_size)
    {
        const float
            cube_half_size_x = cube_size.x() * 0.5f,
            cube_half_size_y = cube_size.y() * 0.5f,
            cube_half_size_z = cube_size.z() * 0.5f;

        object.line_points = {
            // [index 0] front_top_left
            { (cube_center.x() - cube_half_size_x), (cube_center.y() + cube_half_size_y), (cube_center.z() - cube_half_size_z)},
            // [index 1] front_top_rght
            { (cube_center.x() + cube_half_size_x), (cube_center.y() + cube_half_size_y), (cube_center.z() - cube_half_size_z)},
            // [index 2] front_bottom_left
            { (cube_center.x() - cube_half_size_x), (cube_center.y() - cube_half_size_y), (cube_center.z() - cube_half_size_z)},
            // [index 3] front_bottom_rght
            { (cube_center.x() + cube_half_size_x), (cube_center.y() - cube_half_size_y), (cube_center.z() - cube_half_size_z)},
            // [index 4] back_top_left
            { (cube_center.x() - cube_half_size_x), (cube_center.y() + cube_half_size_y), (cube_center.z() + cube_half_size_z)},
            // [index 5] back_top_rght
            { (cube_center.x() + cube_half_size_x), (cube_center.y() + cube_half_size_y), (cube_center.z() + cube_half_size_z)},
            // [index 6] back_bottom_left
            { (cube_center.x() - cube_half_size_x), (cube_center.y() - cube_half_size_y), (cube_center.z() + cube_half_size_z)},
            // [index 7] back_bottom_rght
            { (cube_center.x() + cube_half_size_x), (cube_center.y() - cube_half_size_y), (cube_center.z() + cube_half_size_z)},
        };

        object.line_indices = {
            { 0, 1 },
            { 2, 3 },
            { 0, 2 },
            { 1, 3 },
            { 4, 5 },
            { 6, 7 },
            { 4, 6 },
            { 5, 7 },
            { 0, 4 },
            { 1, 5 },
            { 2, 6 },
            { 3, 7 }
        };
    }


} // namespace