#pragma once

namespace triengine::shaders::includes
{
    static const char* const kPhongLightingShader = R"glsl(
        //#define USE_BLINN_PHONG_SHADING
        //#define USE_ABSOLUTE_DIFFUSE_IN_PHONG_SHADING

        vec3 calcPhongLightInViewSpace(
            in vec3 eyeDirInView/* eye direction in view space */,
            in vec3 fragNormalInView/* fragment normal in view space */,
            in vec3 lightDirInView/* light direction in view space */,
            in vec3 lightColor/* light color */,
            in vec3 ambientColor/* ambient material color */,
            in vec3 diffuseColor/* diffuse material color */,
            in vec3 specularColor/* specular material color */,
            in float shininess/* shininess material (specular exponent) */,
            in float ambientIntensity/* ambient intensity multiplier */,
            in float diffuseIntensity/* diffuse intensity multiplier */,
            in float specularIntensity/* specular intensity multiplier */)
        {
            // Normalize all vectors
            const vec3 N = normalize(fragNormalInView);
            const vec3 L = normalize(lightDirInView);
            const vec3 V = normalize(eyeDirInView);
            
            //
            // ambient component
            //

            const vec3 ambient = ambientColor * lightColor;
            
            //
            // diffuse component
            //
            
            const float diff = 
        #if defined(USE_ABSOLUTE_DIFFUSE_IN_PHONG_SHADING)
                // NOTE: To handle the case where the light source is behind a point 
                //       in the opposite direction of the point's normal vector, we use `abs()` instead of `max()`.
                abs(dot(N, L));
        #else  // ^^^ USE_ABSOLUTE_DIFFUSE_IN_PHONG_SHADING ^^^ / vvv !USE_ABSOLUTE_DIFFUSE_IN_PHONG_SHADING vvv
                // Standard Lambertian diffuse
                max(dot(N, L), 0.0);
        #endif // ^^^ !USE_ABSOLUTE_DIFFUSE_IN_PHONG_SHADING ^^^
            const vec3 diffuse = diffuseColor * diff * lightColor;
            
            //
            // specular component
            //
            
            const vec3 R = reflect(-L, N); // reflect vector
            const float spec = pow(max(dot(V, R), 0.0), shininess);
            const vec3 specular = specularColor * spec * lightColor;
            
            return 
                ambientIntensity * ambient + 
                diffuseIntensity * diffuse + 
                specularIntensity * specular;
        }

        vec3 calcBlinnPhongLightInViewSpace(
            in vec3 eyeDirInView/* eye direction in view space */,
            in vec3 fragNormalInView/* fragment normal in view space */,
            in vec3 lightDirInView/* light direction in view space */,
            in vec3 lightColor/* light color */,
            in vec3 ambientColor/* ambient material color */,
            in vec3 diffuseColor/* diffuse material color */,
            in vec3 specularColor/* specular material color */,
            in float shininess/* shininess material (specular exponent) */,
            in float ambientIntensity/* Ambient intensity multiplier */,
            in float diffuseIntensity/* Diffuse intensity multiplier */,
            in float specularIntensity/* Specular intensity multiplier */)
        {
            // Normalize all vectors
            const vec3 N = normalize(fragNormalInView);
            const vec3 L = normalize(lightDirInView);
            const vec3 V = normalize(eyeDirInView);
            
            //
            // ambient component
            //

            const vec3 ambient = ambientColor * lightColor;
            
            //
            // diffuse component
            //

            const float diff = 
        #if defined(USE_ABSOLUTE_DIFFUSE_IN_PHONG_SHADING)
                // NOTE: To handle the case where the light source is behind a point 
                //       in the opposite direction of the point's normal vector, we use `abs()` instead of `max()`.
                abs(dot(N, L));
        #else  // ^^^ USE_ABSOLUTE_DIFFUSE_IN_PHONG_SHADING ^^^ / vvv !USE_ABSOLUTE_DIFFUSE_IN_PHONG_SHADING vvv
                // Standard Lambertian diffuse
                max(dot(N, L), 0.0);
        #endif // ^^^ !USE_ABSOLUTE_DIFFUSE_IN_PHONG_SHADING ^^^
            const vec3 diffuse = diffuseColor * diff * lightColor;
            
            //
            // specular component
            //

            const vec3 H = normalize(L + V); // halfway vector
            const float spec = pow(max(dot(N, H), 0.0), shininess);
            const vec3 specular = specularColor * spec * lightColor;
            
            return 
                ambientIntensity * ambient + 
                diffuseIntensity * diffuse + 
                specularIntensity * specular;
        }

        struct DirLight
        {
            bool enabled; // enable flag

            vec3 color; // light color
            vec3 direction; // world-space light direction

            float ambientIntensity; // ambient intensity
            float diffuseIntensity; // diffuse intensity
            float specularIntensity; // specular intensity
        };

        // calculates the directional light in view-space
        vec3 calcDirLightInViewSpace(
            in mat4 viewMat,
            in vec3 eyeDirInView,
            in vec3 fragNormalInView,
            in DirLight light,
            in vec3 ambientColor,
            in vec3 diffuseColor,
            in vec3 specularColor,
            in float shininess)
        {
            // Transform world-space light direction to view-space light direction
            // NOTE: `w = 0.0` is used for the direction vector (translation is ignored)
            const vec3 lightDirInView = normalize(vec3(viewMat * vec4(normalize(-light.direction), 0.0)));

        #if defined(USE_BLINN_PHONG_SHADING)
            return calcBlinnPhongLightInViewSpace(
                eyeDirInView,
                fragNormalInView,
                lightDirInView,
                light.color,
                ambientColor,
                diffuseColor,
                specularColor,
                shininess,
                light.ambientIntensity,
                light.diffuseIntensity,
                light.specularIntensity
            );
        #else // ^^^ USE_BLINN_PHONG_SHADING ^^^ / vvv !USE_BLINN_PHONG_SHADING vvv
            return calcPhongLightInViewSpace(
                eyeDirInView,
                fragNormalInView,
                lightDirInView,
                light.color,
                ambientColor,
                diffuseColor,
                specularColor,
                shininess,
                light.ambientIntensity,
                light.diffuseIntensity,
                light.specularIntensity
            );
        #endif // ^^^ !USE_BLINN_PHONG_SHADING ^^^
        }

        struct PointLight
        {
            bool enabled; // enable flag

            vec3 color; // light color
            vec3 position; // world-space light position

            float Kc; // attenuation (constant term)
            float Kl; // attenuation (linear term)
            float Kq; // attenuation (quadraatic term)

            float ambientIntensity; // ambient intensity
            float diffuseIntensity; // diffuse intensity
            float specularIntensity; // specular intensity
        };

        // calculates the point light in view-space
        vec3 calcPointLightInViewSpace(
            in mat4 viewMat,
            in vec3 eyeDirInView,
            in vec3 fragPosInView,
            in vec3 fragNormalInView,
            in PointLight light,
            in vec3 ambientColor,
            in vec3 diffuseColor,
            in vec3 specularColor,
            in float shininess)
        {
            // Transform world-space light position to view-space light position
            // NOTE: `w = 1.0` is used for the position vector (translation is applied)
            const vec3 lightPosInView = vec3(viewMat * vec4(light.position, 1.0));

            // Calculate view-space light direction
            const vec3 lightDirInView = normalize(lightPosInView - fragPosInView);

            // Calculate attenuation
            // (ref: http://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation)
            const float distance = length(lightPosInView - fragPosInView);
            const float attenuation = 1.0 / (light.Kc + light.Kl * distance + light.Kq * (distance * distance));

        #if defined(USE_BLINN_PHONG_SHADING)
            return calcBlinnPhongLightInViewSpace(
                eyeDirInView,
                fragNormalInView,
                lightDirInView,
                light.color,
                ambientColor,
                diffuseColor,
                specularColor,
                shininess,
                light.ambientIntensity * attenuation,
                light.diffuseIntensity * attenuation,
                light.specularIntensity * attenuation
            );
        #else // ^^^ USE_BLINN_PHONG_SHADING ^^^ / vvv !USE_BLINN_PHONG_SHADING vvv
            return calcPhongLightInViewSpace(
                eyeDirInView,
                fragNormalInView,
                lightDirInView,
                light.color,
                ambientColor,
                diffuseColor,
                specularColor,
                shininess,
                light.ambientIntensity * attenuation,
                light.diffuseIntensity * attenuation,
                light.specularIntensity * attenuation
            );
        #endif // ^^^ !USE_BLINN_PHONG_SHADING ^^^
        }
    )glsl";

} // namespace