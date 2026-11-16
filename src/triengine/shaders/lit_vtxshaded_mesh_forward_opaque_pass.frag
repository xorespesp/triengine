// Vertex-Shaded Mesh Object Fragment Shader (forward opaque pass)

#define USE_BLINN_PHONG_SHADING 1
//#define DISABLE_TWO_SIDED_LIGHTING 1
#include "includes/phong_lighting.glsl"
#include "includes/WBOIT.glsl"
#include "includes/simple_fog.glsl"

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT
{
    vec3 fragPosInView; // view-space fragment position
    vec3 fragNormalInView; // view-space fragment normal
    vec3 fragColor; // fragment color
    flat uint objectIndex; // index of the object's record in `u_phongShadedObjects`
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out vec4 fso_fragColor;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////

// --- lights and fog ---
#include "includes/lighting_block.glsl" // u_dirLight, u_pointLight, u_simpleFog

// --- object records ---
#include "includes/phong_shaded_object_block.glsl" // u_phongShadedObjects

void main()
{
    PhongShadedObjectMaterial material = u_phongShadedObjects.elements[fsi.objectIndex].material;

    ////////////////////////////////////////////////////////////////////
    // Forward Lighting pass
    ////////////////////////////////////////////////////////////////////
    
    const vec3 fragNormalInView = normalize(fsi.fragNormalInView); // view-space fragment normal
    const vec3 eyeDirInView = normalize(-fsi.fragPosInView); // view-space eye(view) direction; 
                                                             // the viewer is always at `(0,0,0)`, so eye(view) direction is: `(0,0,0) - Position == -Position`
    
    const vec3 ambientColor = fsi.fragColor * material.ambientIntensity;
    const vec3 diffuseColor = fsi.fragColor * material.diffuseIntensity;
    const vec3 specularColor = vec3(1.0) * material.specularIntensity;
    
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
            material.shininess 
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
            material.shininess
        );
    }
    
    // If no any lights, fallback to base color
    if (!u_dirLight.enabled && !u_pointLight.enabled) {
        resultColor = fsi.fragColor;
    }
    
    // --- simple fog ---
    if (u_simpleFog.enabled)
    {
        ////////////////////////////////////////////////////////////////////
        // 1. calculate distance between vertex position and camera position(origin)
        const vec3 eyePosInView = vec3(0.0, 0.0, 0.0); // camera position in view space
        const float distToCamera = distance(eyePosInView, fsi.fragPosInView);
        
        // 2. calculate fog factor
        const float fogFactor = simpleFogExp2WithMinDist(
            distToCamera,
            u_simpleFog.density,
            u_simpleFog.startDist
        );

        // 3. apply fog (before tone mapping)
        // fog color is already in LDR, so blend with linear HDR color
        resultColor.rgb = mix(u_simpleFog.color, resultColor.rgb, fogFactor);
        ////////////////////////////////////////////////////////////////////
    }

    ////////////////////////////////////////////////////////////////////
    // Output
    ////////////////////////////////////////////////////////////////////

    fso_fragColor = vec4(resultColor, 1.0/* alpha */);
}