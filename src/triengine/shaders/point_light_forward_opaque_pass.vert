// Point-Light Source Object Vertex Shader (forward opaque pass)

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
layout (location = 0) in vec3 vsi_vertPos; // object-space vertex position

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform mat4 u_model; // model matrix
#include "includes/camera_block.glsl" // u_view, u_proj

void main()
{
    gl_Position = u_proj * u_view * u_model * vec4(vsi_vertPos, 1.0);
}