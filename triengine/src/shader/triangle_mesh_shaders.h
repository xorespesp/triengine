#pragma once
#include "shader_version.h"

namespace triengine::shader
{
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Triangle Mesh Object Vertex Shader (using vertex colors for coloring)
    static const char* const kTriangleMeshVertModeVertexShader = R"(
        layout(location = 0) in vec3 vi_vertPos; // object-space vertex position
        layout(location = 1) in vec3 vi_vertNormal; // object-space vertex normal
        layout(location = 2) in vec3 vi_vertColor; // vertex color

        out VertOut
        {
            mat4 view; // view matrix (for view-space light calculation)
            vec3 fragPosInView; // view-space fragment position
            vec3 fragNormalInView; // view-space fragment normal
            vec3 fragColor; // fragment color
        } vo;

        uniform mat4 u_model; // model matrix
        uniform mat4 u_view; // view matrix
        uniform mat4 u_proj; // projection matrix

        void main()
        {
            // TODO: compute in cpp side
            const mat3 normalMatrixInView = mat3(transpose(inverse(u_view * u_model)));

            vo.view = u_view;
            vo.fragPosInView = vec3(u_view * u_model * vec4(vi_vertPos, 1.0));
            vo.fragNormalInView = normalMatrixInView * vi_vertNormal;
            vo.fragColor = vi_vertColor;

            gl_Position = u_proj * vec4(vo.fragPosInView, 1.0);
        }
    )";

    // Triangle Mesh Object Fragment Shader (using vertex colors for coloring)
    static const char* const kTriangleMeshVertModeFragmentShader = R"(
        struct VertexShadingMaterial
        {
            float shininess; // surface shininess scalar (>= 0)
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

        in VertOut
        {
            mat4 view; // view matrix (for view-space light calculation)
            vec3 fragPosInView; // view-space fragment position
            vec3 fragNormalInView; // view-space fragment normal
            vec3 fragColor; // fragment color
        } fi;

        out vec4 fo_fragColor;

        uniform DirLight u_dirLight;
        uniform PointLight u_pointLight;
        uniform VertexShadingMaterial u_material;

        // calculates the directional light in view-space
        vec3 calcDirLightInViewSpace(
            in DirLight light,
            in vec3 fragNormalInView,
            in vec3 eyeDirInView)
        {
            // Transform world-space light direction to view-space light direction
            // NOTE: `w = 0.0` is used for the direction vector (translation is ignored)
            const vec3 lightDirInView = normalize(vec3(fi.view * vec4(normalize(-light.direction), 0.0)));
            
            // ambient
            const vec3 ambient = light.ambientIntensity * light.color;
            
            // diffuse
            const float diff = max(dot(fragNormalInView, lightDirInView), 0.0);
            const vec3 diffuse = light.diffuseIntensity * diff * light.color;

            // specular
            vec3 specular;
            if (light.use_blinn) {
                // blinn-phong model
                const vec3 halfwayDirInView = normalize(lightDirInView + eyeDirInView);
                const float spec = pow(max(dot(fragNormalInView, halfwayDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * spec * light.color;
            } else {
                // phong model
                const vec3 reflectDirInView = reflect(-lightDirInView, fragNormalInView);
                const float spec = pow(max(dot(eyeDirInView, reflectDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * spec * light.color;
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
            const vec3 lightPosInView = vec3(fi.view * vec4(light.position, 1.0));

            // view-space light direction
            const vec3 lightDirInView = normalize(lightPosInView - fi.fragPosInView);

            // ambient
            vec3 ambient = light.ambientIntensity * light.color;

            // diffuse
            const float diff = max(dot(fragNormalInView, lightDirInView), 0.0);
            vec3 diffuse = light.diffuseIntensity * diff * light.color;

            // specular
            vec3 specular;
            if (light.use_blinn) {
                // blinn-phong model
                const vec3 halfwayDirInView = normalize(lightDirInView + eyeDirInView);
                const float spec = pow(max(dot(fragNormalInView, halfwayDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * spec * light.color;
            } else {
                // phong model
                const vec3 reflectDirInView = reflect(-lightDirInView, fragNormalInView);
                const float spec = pow(max(dot(eyeDirInView, reflectDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * spec * light.color;
            }

            // attenuation (ref: http://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation)
            const float distance = length(lightPosInView - fi.fragPosInView);
            const float attenuation = 1.0 / (light.Kc + light.Kl * distance + light.Kq * (distance * distance));    
            ambient *= attenuation;
            diffuse *= attenuation;
            specular *= attenuation;

            // combine results
            return (ambient + diffuse + specular);
        }

        void main()
        {
            if (u_dirLight.enabled || u_pointLight.enabled)
            {
                const vec3 fragNormalInView = normalize(fi.fragNormalInView); // view-space fragment normal
                const vec3 eyeDirInView = normalize(-fi.fragPosInView); // view-space eye(view) direction; 
                                                                        // the viewer is always at `(0,0,0)`, so eye(view) direction is: `(0,0,0) - Position == -Position`

                vec3 result = vec3(0.0);

                // phase 1: directional light
                if (u_dirLight.enabled) {
                    result += calcDirLightInViewSpace(u_dirLight, fragNormalInView, eyeDirInView);
                }

                // phase 2: point light
                if (u_pointLight.enabled) {
                    result += calcPointLightInViewSpace(u_pointLight, fragNormalInView, eyeDirInView);
                }

                fo_fragColor = vec4(result * fi.fragColor.rgb, 1.0/* alpha */);
            }
            else
            {
                fo_fragColor = vec4(fi.fragColor, 1.0/* alpha */);
            }
        }
    )";

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Triangle Mesh Object Vertex Shader (using texture uv coordinates for coloring)
    static const char* const kTriangleMeshTexModeVertexShader = R"(
        layout(location = 0) in vec3 vi_vertPos; // object-space vertex position
        layout(location = 1) in vec3 vi_vertNormal; // object-space vertex normal
        layout(location = 2) in vec2 vi_texCoords; // texture uv coordinate

        out VertOut
        {
            mat4 view; // view matrix (for view-space light calculation)
            vec3 fragPosInView; // view-space fragment position
            vec3 fragNormalInView; // view-space fragment normal
            vec2 texCoords; // texture uv coordinate
        } vo;

        uniform mat4 u_model; // model matrix
        uniform mat4 u_view; // view matrix
        uniform mat4 u_proj; // projection matrix

        void main()
        {
            // TODO: compute in cpp side
            const mat3 normalMatrixInView = mat3(transpose(inverse(u_view * u_model)));

            vo.view = u_view;
            vo.fragPosInView = vec3(u_view * u_model * vec4(vi_vertPos, 1.0));
            vo.fragNormalInView = normalMatrixInView * vi_vertNormal;
            vo.texCoords = vi_texCoords;

            gl_Position = u_proj * vec4(vo.fragPosInView, 1.0);
        }
    )";

    // Triangle Mesh Object Fragment Shader (using texture uv coordinates for coloring)
    static const char* const kTriangleMeshTexModeFragmentShader = R"(
        struct TextureShadingMaterial
        {
            sampler2D diffuse; // diffuse map
            sampler2D specular; // specular map
            float shininess; // surface shininess scalar (>= 0)
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

        in VertOut
        {
            mat4 view; // view matrix (for view-space light calculation)
            vec3 fragPosInView; // view-space fragment position
            vec3 fragNormalInView; // view-space fragment normal
            vec2 texCoords; // texture uv coordinate
        } fi;

        out vec4 fo_fragColor;

        uniform DirLight u_dirLight;
        uniform PointLight u_pointLight;
        uniform TextureShadingMaterial u_material;

        // calculates the directional light in view-space
        vec3 calcDirLightInViewSpace(
            in DirLight light,
            in vec3 fragNormalInView,
            in vec3 eyeDirInView)
        {
            // Transform world-space light direction to view-space light direction
            // NOTE: `w = 0.0` is used for the direction vector (translation is ignored)
            const vec3 lightDirInView = normalize(vec3(fi.view * vec4(normalize(-light.direction), 0.0)));
            
            // ambient
            const vec3 ambient = light.ambientIntensity * vec3(texture(u_material.diffuse, fi.texCoords)) * light.color;
            
            // diffuse
            const float diff = max(dot(fragNormalInView, lightDirInView), 0.0);
            const vec3 diffuse = light.diffuseIntensity * diff * vec3(texture(u_material.diffuse, fi.texCoords)) * light.color;

            // specular
            vec3 specular;
            if (light.use_blinn) {
                // blinn-phong model
                const vec3 halfwayDirInView = normalize(lightDirInView + eyeDirInView);
                const float spec = pow(max(dot(fragNormalInView, halfwayDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * spec * vec3(texture(u_material.specular, fi.texCoords)) * light.color;
            } else {
                // phong model
                const vec3 reflectDirInView = reflect(-lightDirInView, fragNormalInView);
                const float spec = pow(max(dot(eyeDirInView, reflectDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * spec * vec3(texture(u_material.specular, fi.texCoords)) * light.color;
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
            const vec3 lightPosInView = vec3(fi.view * vec4(light.position, 1.0));

            // view-space light direction
            const vec3 lightDirInView = normalize(lightPosInView - fi.fragPosInView);

            // ambient
            vec3 ambient = light.ambientIntensity * vec3(texture(u_material.diffuse, fi.texCoords)) * light.color;

            // diffuse
            const float diff = max(dot(fragNormalInView, lightDirInView), 0.0);
            vec3 diffuse = light.diffuseIntensity * diff * vec3(texture(u_material.diffuse, fi.texCoords)) * light.color;

            // specular
            vec3 specular;
            if (light.use_blinn) {
                // blinn-phong model
                const vec3 halfwayDirInView = normalize(lightDirInView + eyeDirInView);
                const float spec = pow(max(dot(fragNormalInView, halfwayDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * spec * vec3(texture(u_material.specular, fi.texCoords)) * light.color;
            } else {
                // phong model
                const vec3 reflectDirInView = reflect(-lightDirInView, fragNormalInView);
                const float spec = pow(max(dot(eyeDirInView, reflectDirInView), 0.0), u_material.shininess);
                specular = light.specularIntensity * spec * vec3(texture(u_material.specular, fi.texCoords)) * light.color;
            }

            // attenuation (ref: http://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation)
            const float distance = length(lightPosInView - fi.fragPosInView);
            const float attenuation = 1.0 / (light.Kc + light.Kl * distance + light.Kq * (distance * distance));    
            ambient *= attenuation;
            diffuse *= attenuation;
            specular *= attenuation;

            // combine results
            return (ambient + diffuse + specular);
        }

        void main()
        {
            if (u_dirLight.enabled || u_pointLight.enabled)
            {
                const vec3 fragNormalInView = normalize(fi.fragNormalInView); // view-space fragment normal
                const vec3 eyeDirInView = normalize(-fi.fragPosInView); // view-space eye(view) direction; 
                                                                        // the viewer is always at `(0,0,0)`, so eye(view) direction is: `(0,0,0) - Position == -Position`

                vec3 result = vec3(0.0);

                // phase 1: directional light
                if (u_dirLight.enabled) {
                    result += calcDirLightInViewSpace(u_dirLight, fragNormalInView, eyeDirInView);
                }

                // phase 2: point light
                if (u_pointLight.enabled) {
                    result += calcPointLightInViewSpace(u_pointLight, fragNormalInView, eyeDirInView);
                }

                fo_fragColor = vec4(result, 1.0/* alpha */);
            }
            else
            {
                fo_fragColor = texture(u_material.diffuse, fi.texCoords);
            }
        }
    )";

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace