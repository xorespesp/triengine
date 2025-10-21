// Texture-Shaded Mesh Object Vertex Shader (forward opaque pass)

#define USE_BLINN_PHONG_SHADING 1
//#define DISABLE_TWO_SIDED_LIGHTING 1
#include "includes/phong_lighting.glsl"
#include "includes/phong_material.glsl"
#include "includes/WBOIT.glsl"

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT
{
    vec3 fragPosInView; // view-space fragment position
    vec3 fragNormalInView; // view-space fragment normal
    vec2 texCoords; // texture uv coordinate
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out vec4 fso_fragColor;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////

// --- lights ---
uniform DirLight u_dirLight;
uniform PointLight u_pointLight;

// --- materials ---
layout(binding = 0) uniform sampler2D u_diffuseMap; // diffuse color map (RGB)
layout(binding = 1) uniform sampler2D u_specularMap; // specular color map (grayscale)
uniform PhongShadedObjectMaterial u_phongMaterial;
uniform float u_alpha; // object transparency (for WBOIT transparent pass)

void main()
{
    ////////////////////////////////////////////////////////////////////
    // Forward Lighting pass
    ////////////////////////////////////////////////////////////////////

    const vec3 fragNormalInView = normalize(fsi.fragNormalInView); // view-space fragment normal
    const vec3 eyeDirInView = normalize(-fsi.fragPosInView); // view-space eye(view) direction; 
                                                             // the viewer is always at `(0,0,0)`, so eye(view) direction is: `(0,0,0) - Position == -Position`
    
    const vec3 diffuseMapColor = texture(u_diffuseMap, fsi.texCoords).rgb;
    const float specularMapColor = texture(u_specularMap, fsi.texCoords).r;

    const vec3 ambientColor = diffuseMapColor * u_phongMaterial.ambientIntensity;
    const vec3 diffuseColor = diffuseMapColor * u_phongMaterial.diffuseIntensity;
    const vec3 specularColor = vec3(specularMapColor) * u_phongMaterial.specularIntensity;

    vec3 resultColor = vec3(0.0);
    
    // Apply directional light
    if (u_dirLight.enabled) {
        resultColor += calcDirLightInViewSpace(
            eyeDirInView,
            fragNormalInView, 
            u_dirLight,
            ambientColor,
            diffuseColor,
            specularColor,
            u_phongMaterial.shininess 
        );
    }

    // Apply point light
    if (u_pointLight.enabled) {
        resultColor += calcPointLightInViewSpace(
            eyeDirInView,
            fsi.fragPosInView,
            fragNormalInView, 
            u_pointLight,
            ambientColor,
            diffuseColor,
            specularColor,
            u_phongMaterial.shininess
        );
    }
    
    // If no any lights, fallback to base color
    if (!u_dirLight.enabled && !u_pointLight.enabled) {
        resultColor = diffuseMapColor;
    }
    
    ////////////////////////////////////////////////////////////////////
    // Output
    ////////////////////////////////////////////////////////////////////

    fso_fragColor = vec4(resultColor, 1.0/* alpha */);
}