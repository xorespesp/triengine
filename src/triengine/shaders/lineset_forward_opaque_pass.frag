// Lineset Object Fragment Shader (forward opaque pass)
#include "includes/simple_fog.glsl"

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT
{
    vec3 fragPosInView; // view-space fragment position
    vec3 fragColor; // fragment color
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out vec4 fso_fragColor;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////

// --- simple fog ---
uniform SimpleFogOptions u_simpleFog;

void main()
{
    vec3 resultColor = fsi.fragColor;

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

    fso_fragColor = vec4(resultColor, 1.0/* alpha */);
}