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

    struct box_filtered_chess_plane_option_t
    {
        vec3_f32 grid_cell_color1{ 0.74f, 0.74f, 0.74f };
        vec3_f32 grid_cell_color2{ 0.90f, 0.90f, 0.90f };
        struct material_t {
            float ambient_intensity{ 0.3f }; /// ambient intensity; must be `>= 0`
            float diffuse_intensity{ 0.9f }; /// diffuse intensity; must be `>= 0`
            float specular_intensity{ 0.4f }; /// specular intensity; must be `>= 0`
            uint16_t shininess{ 64 }; /// object surface shininess scalar (must be `> 0`)
        } material;
    };

    struct box_filtered_grid_plane_option_t
    {
        vec3_f32 grid_line_color{ 0.74f, 0.74f, 0.74f };
        vec3_f32 grid_cell_color{ 0.90f, 0.90f, 0.90f };
        struct material_t {
            float ambient_intensity{ 0.3f }; /// ambient intensity; must be `>= 0`
            float diffuse_intensity{ 0.9f }; /// diffuse intensity; must be `>= 0`
            float specular_intensity{ 0.4f }; /// specular intensity; must be `>= 0`
            uint16_t shininess{ 64 }; /// object surface shininess scalar (must be `> 0`)
        } material;
    };

    struct transparent_grid_plane_option_t
    {
        vec3_f32 grid_line_color{ 0.0f, 1.0f, 0.0f };
    };

    struct infinite_plane_options
    {
        float max_view_distance{ 50.0f }; // Unit: [m]
        float grid_cell_size{ 0.50f }; // Unit: [m]
        std::variant<
            box_filtered_chess_plane_option_t,
            box_filtered_grid_plane_option_t,
            transparent_grid_plane_option_t
        > plane_option;
    };

} // namespace