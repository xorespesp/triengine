// Lineset Object Vertex Shader (forward opaque pass)

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
layout(location = 0) in vec3 vsi_vertPos; // object-space vertex position
layout(location = 1) in vec3 vsi_vertColor; // vertex color
        
////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out VS_OUT
{
    vec3 fragPosInView; // view-space fragment position
    vec3 fragColor; // fragment color.
} vso;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform mat4 u_model; // model matrix
#include "includes/camera_block.glsl" // u_view, u_proj

void main()
{
    vso.fragPosInView = vec3(u_view * u_model * vec4(vsi_vertPos, 1.0));
    vso.fragColor = vsi_vertColor;
    gl_Position = u_proj * vec4(vso.fragPosInView, 1.0);
}