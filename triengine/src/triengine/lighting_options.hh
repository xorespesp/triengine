#pragma once
#include <triengine/common.h>
#include <triengine/core/shader.hh>
#include <triengine/utility/gl_utils.hh>

namespace triengine
{
    struct directional_light_options
    {
        bool enabled{ true }; // enable flag
        bool follow_camera{ true }; // use camera direction as light direction

        vec3_f32 direction{ 0.0f, -1.0f, 0.0f }; // world-space light direction
        color3_f32 color{ color3_f32::all(1.0f) }; // light color

        float ambient_intensity{ 0.2f }; // ambient intensity (global intensity)
        float diffuse_intensity{ 0.2f }; // diffuse intensity (global intensity)
        float specular_intensity{ 0.2f }; // specular intensity (global intensity)

        directional_light_options() = default;

        void apply_to_shader(
            core::shader_program& shader,
            const mat4_f32& view_mat) const
        {
#if defined (TRIENGINE_DEBUG_MODE)
            //{
            //    // Ref: https://stackoverflow.com/a/62663705
            //    GLint curr_shader_id{ -1 };
            //    GLCall(::glGetIntegerv(GL_CURRENT_PROGRAM, &curr_shader_id));
            //    TRIENGINE_ASSERT(shader.id() == static_cast<GLuint>(curr_shader_id));
            //}
#endif // ^^^ TRIENGINE_DEBUG_MODE ^^^

            shader.set_uniform_bool("u_dirLight.enabled", enabled);
            if (!enabled) { return; }

            shader.set_uniform_vec3("u_dirLight.color", color.to_eigen());

            // Transform world-space light direction to view-space light direction
            // NOTE: `w = 0.0` is used for the direction vector (translation is ignored)
            const vec3_f32 light_dir_in_view = 
                (view_mat * (vec4_f32{} << (-direction).normalized(), 0.0f).finished())
                .head<3>()
                .normalized(); // `normalize(vec3(viewMat * vec4(normalize(-light.direction), 0.0)))`

            shader.set_uniform_vec3("u_dirLight.directionInView", light_dir_in_view);
            shader.set_uniform_float("u_dirLight.ambientIntensity", ambient_intensity);
            shader.set_uniform_float("u_dirLight.diffuseIntensity", diffuse_intensity);
            shader.set_uniform_float("u_dirLight.specularIntensity", specular_intensity);
        }
    };

    struct point_light_options
    {
        bool enabled{ true }; // enable flag

        bool show_light_source{ true }; // show light source object (not used in shader)
        float light_source_color_intensity{ 32.0f }; // light source object color intensity (not used in shader)

        vec3_f32 position{ 0.0f, 1.5f, 0.0f }; // world-space light position
        color3_f32 color{ color3_f32::all(1.0f) }; // light color

        // Ref: https://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation
        float Kc{ 1.0f }; // attenuation (constant term)
        float Kl{ 0.14f }; // attenuation (linear term)
        float Kq{ 0.07f }; // attenuation (quadraatic term)

        float ambient_intensity{ 0.0f }; // ambient intensity (global intensity)
        float diffuse_intensity{ 0.85f }; // diffuse intensity (global intensity)
        float specular_intensity{ 0.7f }; // specular intensity (global intensity)

        point_light_options() = default;

        void apply_to_shader(
            core::shader_program& shader,
            const mat4_f32& view_mat) const
        {
#if defined (TRIENGINE_DEBUG_MODE)
            //{
            //    // Ref: https://stackoverflow.com/a/62663705
            //    GLint curr_shader_id{ -1 };
            //    GLCall(::glGetIntegerv(GL_CURRENT_PROGRAM, &curr_shader_id));
            //    TRIENGINE_ASSERT(shader.id() == static_cast<GLuint>(curr_shader_id));
            //}
#endif // ^^^ TRIENGINE_DEBUG_MODE ^^^

            shader.set_uniform_bool("u_pointLight.enabled", enabled);
            if (!enabled) { return; }

            shader.set_uniform_vec3("u_pointLight.color", color.to_eigen());

            // Transform world-space light position to view-space light position
            // NOTE: `w = 1.0` is used for the position vector (translation is applied)
            const vec3_f32 light_pos_in_view = 
                (view_mat * position.homogeneous())
                .head<3>(); // `vec3(viewMat * vec4(light.position, 1.0))`

            shader.set_uniform_vec3("u_pointLight.positionInView", light_pos_in_view);
            shader.set_uniform_float("u_pointLight.Kc", Kc);
            shader.set_uniform_float("u_pointLight.Kl", Kl);
            shader.set_uniform_float("u_pointLight.Kq", Kq);
            shader.set_uniform_float("u_pointLight.ambientIntensity", ambient_intensity);
            shader.set_uniform_float("u_pointLight.diffuseIntensity", diffuse_intensity);
            shader.set_uniform_float("u_pointLight.specularIntensity", specular_intensity);
        }
    };

    struct bloom_options
    {
        bool enabled{ true }; // enable flag
        float strength{ 0.04f }; // bloom strength
        float upsample_filter_radius{ 0.005f }; // upsample pass filter radius

        bloom_options() = default;
    };

    enum class tone_mapping_curve_type : GLuint/* GLSL subroutine function index */
    {
        reinherd = 0,
        uncharted2_filmic,
        aces_filmic,
    };

    struct hdr_options
    {
        bool enabled{ true }; // enable flag
        float exposure{ 0.3f }; // exposure value
        tone_mapping_curve_type tone_mapping_curve{ tone_mapping_curve_type::aces_filmic };

        hdr_options() = default;
    };

    struct lighting_options
    {
        directional_light_options dir_light;
        point_light_options point_light;
        bloom_options bloom;
        hdr_options hdr;

        lighting_options() = default;
    };

} // namespace