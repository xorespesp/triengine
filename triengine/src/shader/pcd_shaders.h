#pragma once
#include "shader_version.h"

namespace triengine::shader
{
    // Pointcloud Object Vertex Shader
    static const char* const kPcdVertexShader = R"(
		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
        layout(location = 0) in vec3 vsi_vertPos; // object-space vertex position
        layout(location = 1) in vec3 vsi_vertNormal; // object-space vertex normal
        layout(location = 2) in vec3 vsi_vertColor; // vertex color

		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
        out VS_OUT
        {
            mat4 view; // view matrix (for view-space light calculation)
            vec3 fragPosInView; // view-space fragment position
            vec3 fragNormalInView; // view-space fragment normal
            vec3 fragColor; // fragment color
        } vso;

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
        uniform mat4 u_model; // model matrix
        uniform mat4 u_view; // view matrix
        uniform mat4 u_proj; // projection matrix
        uniform mat3 u_nmv; // normal matrix in view-space; `mat3(transpose(inverse(u_view * u_model)))`

        void main()
        {
            vso.view = u_view;
            vso.fragPosInView = vec3(u_view * u_model * vec4(vsi_vertPos, 1.0));
            vso.fragNormalInView = u_nmv * vsi_vertNormal;
			vso.fragColor = vsi_vertColor;

            gl_Position = u_proj * vec4(vso.fragPosInView, 1.0);
        }
    )";


    // Pointcloud Object Fragment Shader (for solid object rendering)
    static const char* const kSolidPcdFragmentShader = R"(
        struct PcdMaterial
        {
            float ambient; // ambient intensity
            float diffuse; // diffuse intensity
            float specular; // specular intensity
            float shininess; // surface shininess scalar (>= 0)
            float alpha; // object transparency (WBOIT)
        };

        struct DirLight
        {
            bool enabled; // enable flag
            bool use_blinn; // use blinn-phong model

            vec3 color; // light color
            vec3 direction; // world-space light direction

            float ambientIntensity; // ambient intensity
            float diffuseIntensity; // diffuse intensity
            float specularIntensity; // specular intensity
        };

        struct PointLight
        {
            bool enabled; // enable flag
            bool use_blinn; // use blinn-phong model

            vec3 color; // light color
            vec3 position; // world-space light position

            float Kc; // attenuation (constant term)
            float Kl; // attenuation (linear term)
            float Kq; // attenuation (quadraatic term)

            float ambientIntensity; // ambient intensity
            float diffuseIntensity; // diffuse intensity
            float specularIntensity; // specular intensity
        };

		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
        in VS_OUT
        {
            mat4 view; // view matrix (for view-space light calculation)
            vec3 fragPosInView; // view-space fragment position
            vec3 fragNormalInView; // view-space fragment normal
            vec3 fragColor; // fragment color
        } fsi;

		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
        out vec4 fso_fragColor;

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
        uniform DirLight u_dirLight;
        uniform PointLight u_pointLight;
        uniform PcdMaterial u_material;

        // calculates the directional light in view-space
        vec3 calcDirLightInViewSpace(
            in DirLight light,
            in vec3 fragNormalInView,
            in vec3 eyeDirInView)
        {
            // Transform world-space light direction to view-space light direction
            // NOTE: `w = 0.0` is used for the direction vector (translation is ignored)
            const vec3 lightDirInView = normalize(vec3(fsi.view * vec4(normalize(-light.direction), 0.0)));

            // ambient
            const vec3 ambient = light.ambientIntensity * u_material.ambient * light.color;
            
            // diffuse
            const float diff = abs(dot(fragNormalInView, lightDirInView)); // NOTE: To handle the case where the light source is behind a point 
                                                                           //       in the opposite direction of the point's normal vector, we use `abs()` instead of `max()`.
            const vec3 diffuse = light.diffuseIntensity * u_material.diffuse * diff * light.color;

            // specular
            vec3 specular;
            if (light.use_blinn) {
                // blinn-phong model
                const vec3 halfwayDirInView = normalize(lightDirInView + eyeDirInView);
                const float spec = pow(max(dot(fragNormalInView, halfwayDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * u_material.specular * spec * light.color;
            } else {
                // phong model
                const vec3 reflectDirInView = reflect(-lightDirInView, fragNormalInView);
                const float spec = pow(max(dot(eyeDirInView, reflectDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * u_material.specular * spec * light.color;
            }

            // combine results
            return (ambient + diffuse + specular);
        }

        // calculates the point light in view-space
        vec3 calcPointLightInViewSpace(
            in PointLight light,
            in vec3 fragNormalInView,
            in vec3 eyeDirInView)
        {
            // Transform world-space light position to view-space light position
            // NOTE: `w = 1.0` is used for the position vector (translation is applied)
            const vec3 lightPosInView = vec3(fsi.view * vec4(light.position, 1.0));

            // view-space light direction
            const vec3 lightDirInView = normalize(lightPosInView - fsi.fragPosInView);

            // ambient
            vec3 ambient = light.ambientIntensity * u_material.ambient * light.color;

            // diffuse
            const float diff = abs(dot(fragNormalInView, lightDirInView)); // NOTE: To handle the case where the light source is behind a point 
                                                                           //       in the opposite direction of the point's normal vector, we use `abs()` instead of `max()`.
            vec3 diffuse = light.diffuseIntensity * u_material.diffuse * diff * light.color;

            // specular
            vec3 specular;
            if (light.use_blinn) {
                // blinn-phong model
                const vec3 halfwayDirInView = normalize(lightDirInView + eyeDirInView);
                const float spec = pow(max(dot(fragNormalInView, halfwayDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * u_material.specular * spec * light.color;
            } else {
                // phong model
                const vec3 reflectDirInView = reflect(-lightDirInView, fragNormalInView);
                const float spec = pow(max(dot(eyeDirInView, reflectDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * u_material.specular * spec * light.color;
            }

            // attenuation (ref: http://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation)
            const float distance = length(lightPosInView - fsi.fragPosInView);
            const float attenuation = 1.0 / (light.Kc + light.Kl * distance + light.Kq * (distance * distance));    
            ambient *= attenuation;
            diffuse *= attenuation;
            specular *= attenuation;

            // combine results
            return (ambient + diffuse + specular);
        }

        void main()
        {
            ////////////////////////////////////////////////////////////////////
            // Lighting pass
            ////////////////////////////////////////////////////////////////////

            vec3 resultColor = vec3(0.0);
            if (u_dirLight.enabled || u_pointLight.enabled)
            {
                const vec3 fragNormalInView = normalize(fsi.fragNormalInView); // view-space fragment normal
                const vec3 eyeDirInView = normalize(-fsi.fragPosInView); // view-space eye(view) direction;
                                                                        // the viewer is always at `(0,0,0)`, so eye(view) direction is: `(0,0,0) - Position == -Position`

                vec3 color = vec3(0.0);

                // phase 1: directional light
                if (u_dirLight.enabled) {
                    color += calcDirLightInViewSpace(u_dirLight, fragNormalInView, eyeDirInView);
                }

                // phase 2: point light
                if (u_pointLight.enabled) {
                    color += calcPointLightInViewSpace(u_pointLight, fragNormalInView, eyeDirInView);
                }

                resultColor = color * fsi.fragColor.rgb;
            }
            else
            {
                resultColor = fsi.fragColor.rgb;
            }

            ////////////////////////////////////////////////////////////////////
            // Output
            ////////////////////////////////////////////////////////////////////

            fso_fragColor = vec4(resultColor, 1.0/* alpha */);
        }
    )";


    // Pointcloud Object Fragment Shader (for transparent object rendering)
    static const char* const kTransparentPcdFragmentShader = R"(
        struct PcdMaterial
        {
            float ambient; // ambient intensity
            float diffuse; // diffuse intensity
            float specular; // specular intensity
            float shininess; // surface shininess scalar (>= 0)
            float alpha; // object transparency (WBOIT)
        };

        struct DirLight
        {
            bool enabled; // enable flag
            bool use_blinn; // use blinn-phong model

            vec3 color; // light color
            vec3 direction; // world-space light direction

            float ambientIntensity; // ambient intensity
            float diffuseIntensity; // diffuse intensity
            float specularIntensity; // specular intensity
        };

        struct PointLight
        {
            bool enabled; // enable flag
            bool use_blinn; // use blinn-phong model

            vec3 color; // light color
            vec3 position; // world-space light position

            float Kc; // attenuation (constant term)
            float Kl; // attenuation (linear term)
            float Kq; // attenuation (quadraatic term)

            float ambientIntensity; // ambient intensity
            float diffuseIntensity; // diffuse intensity
            float specularIntensity; // specular intensity
        };

		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
        in VS_OUT
        {
            mat4 view; // view matrix (for view-space light calculation)
            vec3 fragPosInView; // view-space fragment position
            vec3 fragNormalInView; // view-space fragment normal
            vec3 fragColor; // fragment color
        } fsi;

		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
        layout (location = 0) out vec4 fso_accum;
        layout (location = 1) out float fso_reveal;

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
        uniform DirLight u_dirLight;
        uniform PointLight u_pointLight;
        uniform PcdMaterial u_material;

        // calculates the directional light in view-space
        vec3 calcDirLightInViewSpace(
            in DirLight light,
            in vec3 fragNormalInView,
            in vec3 eyeDirInView)
        {
            // Transform world-space light direction to view-space light direction
            // NOTE: `w = 0.0` is used for the direction vector (translation is ignored)
            const vec3 lightDirInView = normalize(vec3(fsi.view * vec4(normalize(-light.direction), 0.0)));

            // ambient
            const vec3 ambient = light.ambientIntensity * u_material.ambient * light.color;
            
            // diffuse
            const float diff = abs(dot(fragNormalInView, lightDirInView)); // NOTE: To handle the case where the light source is behind a point 
                                                                           //       in the opposite direction of the point's normal vector, we use `abs()` instead of `max()`.
            const vec3 diffuse = light.diffuseIntensity * u_material.diffuse * diff * light.color;

            // specular
            vec3 specular;
            if (light.use_blinn) {
                // blinn-phong model
                const vec3 halfwayDirInView = normalize(lightDirInView + eyeDirInView);
                const float spec = pow(max(dot(fragNormalInView, halfwayDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * u_material.specular * spec * light.color;
            } else {
                // phong model
                const vec3 reflectDirInView = reflect(-lightDirInView, fragNormalInView);
                const float spec = pow(max(dot(eyeDirInView, reflectDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * u_material.specular * spec * light.color;
            }

            // combine results
            return (ambient + diffuse + specular);
        }

        // calculates the point light in view-space
        vec3 calcPointLightInViewSpace(
            in PointLight light,
            in vec3 fragNormalInView,
            in vec3 eyeDirInView)
        {
            // Transform world-space light position to view-space light position
            // NOTE: `w = 1.0` is used for the position vector (translation is applied)
            const vec3 lightPosInView = vec3(fsi.view * vec4(light.position, 1.0));

            // view-space light direction
            const vec3 lightDirInView = normalize(lightPosInView - fsi.fragPosInView);

            // ambient
            vec3 ambient = light.ambientIntensity * u_material.ambient * light.color;

            // diffuse
            const float diff = abs(dot(fragNormalInView, lightDirInView)); // NOTE: To handle the case where the light source is behind a point 
                                                                           //       in the opposite direction of the point's normal vector, we use `abs()` instead of `max()`.
            vec3 diffuse = light.diffuseIntensity * u_material.diffuse * diff * light.color;

            // specular
            vec3 specular;
            if (light.use_blinn) {
                // blinn-phong model
                const vec3 halfwayDirInView = normalize(lightDirInView + eyeDirInView);
                const float spec = pow(max(dot(fragNormalInView, halfwayDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * u_material.specular * spec * light.color;
            } else {
                // phong model
                const vec3 reflectDirInView = reflect(-lightDirInView, fragNormalInView);
                const float spec = pow(max(dot(eyeDirInView, reflectDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * u_material.specular * spec * light.color;
            }

            // attenuation (ref: http://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation)
            const float distance = length(lightPosInView - fsi.fragPosInView);
            const float attenuation = 1.0 / (light.Kc + light.Kl * distance + light.Kq * (distance * distance));    
            ambient *= attenuation;
            diffuse *= attenuation;
            specular *= attenuation;

            // combine results
            return (ambient + diffuse + specular);
        }

        void main()
        {
            ////////////////////////////////////////////////////////////////////
            // Lighting pass
            ////////////////////////////////////////////////////////////////////

            vec3 resultColor = vec3(0.0);
            if (u_dirLight.enabled || u_pointLight.enabled)
            {
                const vec3 fragNormalInView = normalize(fsi.fragNormalInView); // view-space fragment normal
                const vec3 eyeDirInView = normalize(-fsi.fragPosInView); // view-space eye(view) direction;
                                                                        // the viewer is always at `(0,0,0)`, so eye(view) direction is: `(0,0,0) - Position == -Position`

                vec3 color = vec3(0.0);

                // phase 1: directional light
                if (u_dirLight.enabled) {
                    color += calcDirLightInViewSpace(u_dirLight, fragNormalInView, eyeDirInView);
                }

                // phase 2: point light
                if (u_pointLight.enabled) {
                    color += calcPointLightInViewSpace(u_pointLight, fragNormalInView, eyeDirInView);
                }

                resultColor = color * fsi.fragColor.rgb;
            }
            else
            {
                resultColor = fsi.fragColor.rgb;
            }

            ////////////////////////////////////////////////////////////////////
            // WBOIT pass
            ////////////////////////////////////////////////////////////////////

	        const vec4 blendColor = vec4(resultColor, u_material.alpha);
            
	        // weight function
	        const float weight =
		        max(min(1.0, max(max(blendColor.r, blendColor.g), blendColor.b) * blendColor.a), blendColor.a) *
		        clamp(0.03 / (1e-5 + pow(gl_FragCoord.z / 200, 4.0)), 1e-2, 3e3);
                
	        // store pixel color accumulation
	        fso_accum = vec4(blendColor.rgb * blendColor.a, blendColor.a) * weight;
	            
	        // store pixel revealage threshold
	        fso_reveal = blendColor.a;
        }
    )";

} // namespace
