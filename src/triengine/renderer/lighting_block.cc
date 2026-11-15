#include "lighting_block.hh"
#include <triengine/renderer/renderer_base.hh>
#include <triengine/core/shader.hh>

namespace triengine::renderer
{
    namespace
    {
        using layout = lighting_uniform_buffer::layout;

        // The offsets of lighting_block.glsl
        static_assert(layout::offset_of<&lighting_block::u_pointLight>() == 64);
        static_assert(layout::offset_of<&lighting_block::u_simpleFog>() == 144);
        static_assert(layout::buffer_size_for(0) == 192);
    } // namespace

    lighting_block make_lighting_block(const render_context& render_ctx)
    {
        const directional_light_options& dir_light = render_ctx.light_opts->dir_light;
        const point_light_options& point_light = render_ctx.light_opts->point_light;
        const simple_fog_options& simple_fog = render_ctx.light_opts->simple_fog;

        // `w = 0` for a direction: the view matrix's translation does not apply.
        const vec3_f32 dir_light_direction_in_view =
            (render_ctx.view * (vec4_f32{} << (-dir_light.direction).normalized(), 0.0f).finished())
            .head<3>()
            .normalized();

        const vec3_f32 point_light_position_in_view = (render_ctx.view * point_light.position.homogeneous()).head<3>();

        return lighting_block{
            glsl_dir_light{
                dir_light.enabled,
                dir_light.color,
                dir_light_direction_in_view,
                dir_light.ambient_intensity,
                dir_light.diffuse_intensity,
                dir_light.specular_intensity,
            },
            glsl_point_light{
                point_light.enabled,
                point_light.color,
                point_light_position_in_view,
                point_light.Kc,
                point_light.Kl,
                point_light.Kq,
                point_light.ambient_intensity,
                point_light.diffuse_intensity,
                point_light.specular_intensity,
            },
            glsl_simple_fog{
                simple_fog.enabled,
                render_ctx.simple_fog_color,
                simple_fog.fog_density,
                simple_fog.fog_start_dist,
            },
        };
    }

    void verify_lighting_block(const core::shader_program& program)
    {
        layout::verify(program.id(), lighting_uniform_buffer::type(), lighting_block::glsl_block_name(), kLightingBlockBindingPointIndex);
    }

} // namespace
