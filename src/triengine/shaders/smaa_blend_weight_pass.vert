// SMAA Blending Weight Calculation Pass Vertex Shader

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
layout (location = 0) in vec3 vsi_vertPos;
layout (location = 1) in vec2 vsi_texCoord;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out VS_OUT {
    vec2 texCoord;
    vec4 smaaRTMetrics;
    vec2 pixCoord;
    vec4 offsets[3];
} vso;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform vec4 u_smaaRTMetrics; // `vec4(1.0 / screen_width_pixels, 1.0 / screen_height_pixels, screen_width_pixels, screen_height_pixels)`

#define SMAA_INCLUDE_PS 0
#define SMAA_INCLUDE_VS 1
#define SMAA_GLSL_4 1
#define SMAA_PRESET_ULTRA
#define SMAA_RT_METRICS u_smaaRTMetrics
#include "includes/SMAA.hlsl"

void main()
{
	vso.texCoord = vsi_texCoord;
    vso.smaaRTMetrics = u_smaaRTMetrics;

    SMAABlendingWeightCalculationVS(vsi_texCoord, vso.pixCoord, vso.offsets);

	gl_Position = vec4(vsi_vertPos, 1.0f);
}