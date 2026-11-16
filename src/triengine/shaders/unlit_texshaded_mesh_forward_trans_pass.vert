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
    flat uint objectIndex; // index of the object's record in `u_phongShadedObjects`
} vso;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
#include "includes/camera_block.glsl" // u_view, u_proj
#include "includes/phong_shaded_object_block.glsl" // u_phongShadedObjects

void main()
{
    mat4 model = u_phongShadedObjects.elements[gl_BaseInstance].model;

    vso.fragPosInView = vec3(u_view * (model * vec4(vsi_vertPos, 1.0)));
    vso.texCoords = vsi_texCoords;
    vso.objectIndex = gl_BaseInstance;
    gl_Position = u_proj * vec4(vso.fragPosInView, 1.0);
}
