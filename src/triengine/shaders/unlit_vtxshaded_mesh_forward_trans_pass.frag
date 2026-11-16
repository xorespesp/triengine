// Vertex-Shaded Unlit Mesh Object Fragment Shader (forward transparent pass)

#include "includes/WBOIT.glsl"

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT
{
    vec3 fragPosInView; // view-space fragment position (for WBOIT weight)
    vec3 fragColor; // fragment color
    flat uint objectIndex; // index of the object's record in `u_phongShadedObjects`
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
layout(location = 0) out vec4 fso_accum;
layout(location = 1) out float fso_reveal;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
#include "includes/phong_shaded_object_block.glsl" // u_phongShadedObjects

void main()
{
    float alpha = u_phongShadedObjects.elements[fsi.objectIndex].alpha;

    // Unlit: use the interpolated vertex color directly (no lighting applied).
    const vec3 baseColor = fsi.fragColor;

    ////////////////////////////////////////////////////////////////////
    // WBOIT pass
    ////////////////////////////////////////////////////////////////////

    const vec4 blendColor = vec4(baseColor, alpha);

    // calculate weight
    const float weight = computeWBOITWeight(blendColor, -fsi.fragPosInView.z);

    // store pixel color accumulation
    fso_accum = vec4(blendColor.rgb * blendColor.a, blendColor.a) * weight;

    // store pixel revealage threshold
    fso_reveal = blendColor.a;
}
