// Vertex-Shaded Unlit Mesh Object Fragment Shader (forward transparent pass)

#include "includes/WBOIT.glsl"

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT
{
    vec3 fragPosInView; // view-space fragment position (for WBOIT weight)
    vec3 fragColor; // fragment color
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
layout(location = 0) out vec4 fso_accum;
layout(location = 1) out float fso_reveal;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform float u_alpha; // object transparency (for WBOIT transparent pass)

void main()
{
    // Unlit: use the interpolated vertex color directly (no lighting applied).
    const vec3 baseColor = fsi.fragColor;

    ////////////////////////////////////////////////////////////////////
    // WBOIT pass
    ////////////////////////////////////////////////////////////////////

    const vec4 blendColor = vec4(baseColor, u_alpha);

    // calculate weight
    const float weight = computeWBOITWeight(blendColor, -fsi.fragPosInView.z);

    // store pixel color accumulation
    fso_accum = vec4(blendColor.rgb * blendColor.a, blendColor.a) * weight;

    // store pixel revealage threshold
    fso_reveal = blendColor.a;
}
