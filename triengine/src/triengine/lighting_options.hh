#pragma once
#include <triengine/common.h>
#include <triengine/shader.hh>
#include <triengine/utility/gl_utils.hh>

namespace triengine
{
    struct lighting_options
    {
        struct directional_light_options
        {
            bool enabled{ true }; // enable flag
            bool use_blinn{ true }; // use blinn-phong model
            bool follow_camera{ true }; // use camera direction as light direction

            vec3_f32 direction{ 0.0f, 1.0f, 0.0f }; // world-space light direction
            color3_f32 color{ color3_f32::all(1.0f) }; // light color

            float ambientIntensity{ 0.2f }; // ambient intensity
            float diffuseIntensity{ 0.2f }; // diffuse intensity
            float specularIntensity{ 0.2f }; // specular intensity
        } dir_light;

        struct point_light_options
        {
            bool enabled{ true }; // enable flag
            bool use_blinn{ true }; // use blinn-phong model
            bool show_light_source{ true }; // show light source object (not used in shader)

            vec3_f32 position{ 0.0f, -1.5f, 0.0f }; // world-space light position
            color3_f32 color{ color3_f32::all(1.0f) }; // light color

            // Ref: https://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation
            float Kc{ 1.0f }; // attenuation (constant term)
            float Kl{ 0.14f }; // attenuation (linear term)
            float Kq{ 0.07f }; // attenuation (quadraatic term)

            float ambientIntensity{ 0.0f }; // ambient intensity
            float diffuseIntensity{ 0.85f }; // diffuse intensity
            float specularIntensity{ 0.7f }; // specular intensity
        } point_light;

        lighting_options() = default;
        ~lighting_options() = default;

        void apply_to_shader(shader_program& shader) const
        {
#if defined (TRIENGINE_DEBUG_MODE)
            {
                // Ref: https://stackoverflow.com/a/62663705
                GLint curr_shader_id{ -1 };
                GLCall(::glGetIntegerv(GL_CURRENT_PROGRAM, &curr_shader_id));
                TRIENGINE_ASSERT(shader.id() == static_cast<GLuint>(curr_shader_id));
            }
#endif // ^^^ TRIENGINE_DEBUG_MODE ^^^
            
            ///
            /// Directional Light
            ///
            
            shader.set_uniform_bool("u_dirLight.enabled", dir_light.enabled);
            shader.set_uniform_bool("u_dirLight.use_blinn", dir_light.use_blinn);

            shader.set_uniform_vec3("u_dirLight.color", dir_light.color.to_eigen());
            shader.set_uniform_vec3("u_dirLight.direction", dir_light.direction);

            shader.set_uniform_float("u_dirLight.ambientIntensity", dir_light.ambientIntensity);
            shader.set_uniform_float("u_dirLight.diffuseIntensity", dir_light.diffuseIntensity);
            shader.set_uniform_float("u_dirLight.specularIntensity", dir_light.specularIntensity);


            ///
            /// Point Light
            ///

            shader.set_uniform_bool("u_pointLight.enabled", point_light.enabled);
            shader.set_uniform_bool("u_pointLight.use_blinn", point_light.use_blinn);

            shader.set_uniform_vec3("u_pointLight.color", point_light.color.to_eigen());
            shader.set_uniform_vec3("u_pointLight.position", point_light.position);

            shader.set_uniform_float("u_pointLight.Kc", point_light.Kc);
            shader.set_uniform_float("u_pointLight.Kl", point_light.Kl);
            shader.set_uniform_float("u_pointLight.Kq", point_light.Kq);

            shader.set_uniform_float("u_pointLight.ambientIntensity", point_light.ambientIntensity);
            shader.set_uniform_float("u_pointLight.diffuseIntensity", point_light.diffuseIntensity);
            shader.set_uniform_float("u_pointLight.specularIntensity", point_light.specularIntensity);
        }
    };

} // namespace