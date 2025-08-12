// SMAA Neighbor Blending Pass Fragment Shader

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT {
    vec2 texCoord;
    vec4 smaaRTMetrics;
    vec4 offset;
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out vec4 fso_fragColor;

#define SMAA_INCLUDE_PS 1
#define SMAA_INCLUDE_VS 0
#define SMAA_GLSL_4 1
#define SMAA_PRESET_ULTRA
#define SMAA_RT_METRICS fsi.smaaRTMetrics
#include <SMAA.hlsl>

layout(binding = 0) uniform SMAATexture2D(u_colorTex);
layout(binding = 1) uniform SMAATexture2D(u_blendTex);

void main()
{
    fso_fragColor = SMAANeighborhoodBlendingPS(fsi.texCoord, fsi.offset, u_colorTex, u_blendTex);
}