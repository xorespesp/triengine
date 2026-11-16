// Texture-Shaded Mesh Object Vertex Shader (forward opaque pass)

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
layout(location = 0) in vec3 vsi_vertPos; // object-space vertex position
layout(location = 1) in vec3 vsi_vertNormal; // object-space vertex normal
layout(location = 2) in vec2 vsi_texCoords; // texture uv coordinate

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out VS_OUT
{
    vec3 fragPosInView; // view-space fragment position
    vec3 fragNormalInView; // view-space fragment normal
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
    PhongShadedObjectRecord object = u_phongShadedObjects.elements[gl_BaseInstance];

    vso.fragPosInView = vec3(u_view * (object.model * vec4(vsi_vertPos, 1.0)));
    vso.fragNormalInView = mat3(u_view) * (object.normalMatrixInWorld * vsi_vertNormal);
    vso.texCoords = vsi_texCoords;
    vso.objectIndex = gl_BaseInstance;

    gl_Position = u_proj * vec4(vso.fragPosInView, 1.0);
}