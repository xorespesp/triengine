// Vertex-Shaded Mesh Object Vertex Shader (forward opaque pass)

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
layout(location = 0) in vec3 vsi_vertPos; // object-space vertex position
layout(location = 1) in vec3 vsi_vertNormal; // object-space vertex normal
layout(location = 2) in vec3 vsi_vertColor; // vertex color

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out VS_OUT
{
    vec3 fragPosInView; // view-space fragment position
    vec3 fragNormalInView; // view-space fragment normal
    vec3 fragColor; // fragment color
} vso;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform mat4 u_model; // model matrix
uniform mat4 u_view; // view matrix
uniform mat4 u_proj; // projection matrix
uniform mat3 u_nmv; // normal matrix in view-space; `mat3(transpose(inverse(u_view * u_model)))`

void main()
{
    vso.fragPosInView = vec3(u_view * u_model * vec4(vsi_vertPos, 1.0));
    vso.fragNormalInView = u_nmv * vsi_vertNormal;
    vso.fragColor = vsi_vertColor;

    gl_Position = u_proj * vec4(vso.fragPosInView, 1.0);
}