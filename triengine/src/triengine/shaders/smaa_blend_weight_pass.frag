// SMAA Blending Weight Calculation Pass Fragment Shader

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT {
    vec2 texCoord;
    vec4 smaaRTMetrics;
    vec2 pixCoord;
    vec4 offsets[3];
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out vec4 fso_fragColor;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform vec4 u_subsampleIndices;

#define SMAA_INCLUDE_PS 1
#define SMAA_INCLUDE_VS 0
#define SMAA_GLSL_4 1
#define SMAA_PRESET_ULTRA
#define SMAA_RT_METRICS fsi.smaaRTMetrics
#include <SMAA.hlsl>

layout(binding = 0) uniform SMAATexture2D(u_edgesTex);
layout(binding = 1) uniform SMAATexture2D(u_areaTex);
layout(binding = 2) uniform SMAATexture2D(u_searchTex);

void main()
{
    fso_fragColor = SMAABlendingWeightCalculationPS(
        fsi.texCoord, 
        fsi.pixCoord, 
        fsi.offsets, 
        u_edgesTex, 
        u_areaTex, 
        u_searchTex, 
        u_subsampleIndices // Just pass zero for SMAA 1x, see @SUBSAMPLE_INDICES.
    );
}