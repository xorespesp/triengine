// Texture-Shaded Unlit Mesh Object Vertex Shader (forward transparent pass)

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
layout(location = 0) in vec3 vsi_vertPos; // object-space vertex position
layout(location = 2) in vec2 vsi_texCoords; // texture uv coordinate (location 1 normal is unused in unlit)

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out VS_OUT
{
    vec3 fragPosInView; // view-space fragment position (for WBOIT weight)
    vec2 texCoords; // texture uv coordinate
} vso;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform mat4 u_model; // model matrix
uniform mat4 u_view; // view matrix
uniform mat4 u_proj; // projection matrix

void main()
{
    vso.fragPosInView = vec3(u_view * u_model * vec4(vsi_vertPos, 1.0));
    vso.texCoords = vsi_texCoords;
    gl_Position = u_proj * vec4(vso.fragPosInView, 1.0);
}
