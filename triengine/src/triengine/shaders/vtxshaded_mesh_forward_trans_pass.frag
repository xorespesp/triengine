// Vertex-Shaded Mesh Object Fragment Shader (forward transparent pass)

#define USE_BLINN_PHONG_SHADING 1
//#define DISABLE_TWO_SIDED_LIGHTING 1
#include "includes/phong_lighting.glsl"
#include "includes/phong_material.glsl"
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
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
layout (location = 0) out vec4 fso_accum;
layout (location = 1) out float fso_reveal;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////

// --- lights ---
uniform DirLight u_dirLight;
uniform PointLight u_pointLight;

// --- materials ---
uniform PhongShadedObjectMaterial u_phongMaterial;
uniform float u_alpha; // object transparency (WBOIT)

// --- simple fog ---
uniform SimpleFogOptions u_simpleFog;

void main()
{
    ////////////////////////////////////////////////////////////////////
    // Forward Lighting pass
    ////////////////////////////////////////////////////////////////////
    
    const vec3 fragNormalInView = normalize(fsi.fragNormalInView); // view-space fragment normal
    const vec3 eyeDirInView = normalize(-fsi.fragPosInView); // view-space eye(view) direction; 
                                                             // the viewer is always at `(0,0,0)`, so eye(view) direction is: `(0,0,0) - Position == -Position`
    
    const vec3 ambientColor = fsi.fragColor * u_phongMaterial.ambientIntensity;
    const vec3 diffuseColor = fsi.fragColor * u_phongMaterial.diffuseIntensity;
    const vec3 specularColor = vec3(1.0) * u_phongMaterial.specularIntensity;
    
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
    // WBOIT pass
    ////////////////////////////////////////////////////////////////////

	const vec4 blendColor = vec4(resultColor, u_alpha);

	// calculate weight
    const float weight = computeWBOITWeight(blendColor, gl_FragCoord.z);
    
	// store pixel color accumulation
	fso_accum = vec4(blendColor.rgb * blendColor.a, blendColor.a) * weight;
	
	// store pixel revealage threshold
	fso_reveal = blendColor.a;
}