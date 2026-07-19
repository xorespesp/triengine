// Object Normal Visualization Vertex Shader

////////////////////////////////////////////
// shader inputs
// NOTE: The vertex memory layout must be compatible with the layouts of other(pcd, triangle mesh, ...) shaders.
////////////////////////////////////////////
layout (location = 0) in vec3 vsi_vertPos; // object-space vertex position
layout (location = 1) in vec3 vsi_vertNormal; // object-space vertex normal

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out VS_OUT {
    vec3 vertex_color;
    vec4 normal_end_position_clip;
} vso;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform mat4 u_model; // model matrix
uniform mat4 u_view_proj; // view-projection matrix; `u_proj * u_view`
uniform mat3 u_nm; // normal matrix; `mat3(transpose(inverse(u_model)))`

const float kNormalMagnitude = 0.03; // Unit: [m]

void main()
{
    // Transform vertex position to world space
    const vec4 vertex_position_world = u_model * vec4(vsi_vertPos, 1.0);
            
    // Transform and normalize the normal to world space
    const vec3 normal_world = normalize(u_nm * vsi_vertNormal);

    // Calculate the end position of the normal in world space
    const vec4 normal_end_world = vertex_position_world + vec4(normal_world * kNormalMagnitude, 0.0);

    // Calculate normal color
    vso.vertex_color = (normal_world * 0.5) + 0.5;

    // Transform the normal end position to clip space
    vso.normal_end_position_clip = u_view_proj * normal_end_world;

    // Transform vertex position to clip space
    // and set gl_Position for the vertex shader pipeline
    gl_Position = u_view_proj * vertex_position_world;
}