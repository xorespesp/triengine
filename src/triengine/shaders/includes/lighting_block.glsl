// Lights and fog of the frame, in view space, shared by every program that declares the block.
// C++: `renderer::lighting_block` (renderer/lighting_block.hh)
// Include it after any `#define` that configures phong_lighting.glsl.

#include "phong_lighting.glsl"
#include "simple_fog.glsl"

layout(std140, binding = 1) uniform LightingBlock
{
    DirLight u_dirLight;
    PointLight u_pointLight;
    SimpleFogOptions u_simpleFog;
};
