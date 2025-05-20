#pragma once
#include <triengine/common.h>
#include <triengine/core/shader.hh>
#include <triengine/utility/gl_utils.hh>

#include <variant>

namespace triengine
{
    enum class infinite_plane_type
    {
        transparent_grid,
        box_filtered_grid,
        box_filtered_chess_grid,
    };

    struct transparent_grid_plane_option_t
    {
        vec3_f32 grid_line_color{ 0.8f, 0.8f, 0.8f };
    };

    struct box_filtered_grid_plane_option_t
    {
        vec3_f32 grid_line_color{ 0.82f, 0.82f, 0.82f };
        vec3_f32 grid_cell_color{ 0.90f, 0.90f, 0.90f };
    };

    struct box_filtered_chess_plane_option_t
    {
        vec3_f32 grid_cell_color1{ 0.82f, 0.82f, 0.82f };
        vec3_f32 grid_cell_color2{ 0.90f, 0.90f, 0.90f };
    };

    struct infinite_plane_options
    {
        float max_view_distance{ 50.0f }; // Unit: [m]
        float grid_cell_size{ 0.50f }; // Unit: [m]
        std::variant<
            transparent_grid_plane_option_t,
            box_filtered_grid_plane_option_t,
            box_filtered_chess_plane_option_t
        > plane_option;
    };

} // namespace