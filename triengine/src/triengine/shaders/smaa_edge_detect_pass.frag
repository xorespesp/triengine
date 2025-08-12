// SMAA Edge Detection Pass Fragment Shader

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT {
    vec2 texCoord;
    vec4 smaaRTMetrics;
    vec4 offsets[3];
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out vec2 fso_fragColor;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform sampler2D u_colorTex;

#define SMAA_INCLUDE_PS 1
#define SMAA_INCLUDE_VS 0
#define SMAA_GLSL_4 1
#define SMAA_PRESET_ULTRA
#define SMAA_RT_METRICS fsi.smaaRTMetrics
#include <SMAA.hlsl>

void main()
{
    fso_fragColor = SMAAColorEdgeDetectionPS(fsi.texCoord, fsi.offsets, u_colorTex); // vec2 type
}