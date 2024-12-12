#pragma once
#include "shader_version.h"

namespace triengine::shader
{
    // LearnOpenGL/src/4.advanced_opengl/9.3.geometry_shader_normals/9.3.normal_visualization.vs
    // LearnOpenGL/src/4.advanced_opengl/9.3.geometry_shader_normals/9.3.normal_visualization.gs
    // LearnOpenGL/src/4.advanced_opengl/9.3.geometry_shader_normals/9.3.normal_visualization.fs

    // Object Normal Visualization Vertex Shader
    static const char* const kObjectNormalVisVertexShader = R"(
        // NOTE: The vertex memory layout must be compatible with the layouts of other(pcd, triangle mesh, ...) shaders.
        layout (location = 0) in vec3 vi_vertPos; // object-space vertex position
        layout (location = 1) in vec3 vi_vertNormal; // object-space vertex normal

        out VS_OUT {
            vec3 normal;
        } vs_out;

        uniform mat4 u_model; // model matrix
        uniform mat4 u_view; // view matrix

        void main()
        {
            // TODO: compute in cpp side
            const mat3 normalMatrixInView = mat3(transpose(inverse(u_view * u_model)));
            
            // NOTE: In order to visualize the normal vectors with a constant length regardless of the object's scale, the normal vectors MUST be normalized.
            vs_out.normal = normalize(normalMatrixInView * vi_vertNormal);
            
            gl_Position = u_view * u_model * vec4(vi_vertPos, 1.0); 
        }
    )";

    // Object Normal Visualization Geometry Shader
    static const char* const kObjectNormalVisGeometryShader = R"(
        layout (triangles) in;
        layout (line_strip, max_vertices = 6) out;

        in VS_OUT {
            vec3 normal;
        } gs_in[];

        const float kNormalMagnitude = 0.03; // Unit: [m]

        uniform mat4 u_proj; // projection matrix

        void GenerateLine(int index)
        {
            gl_Position = u_proj * gl_in[index].gl_Position;
            EmitVertex();
            gl_Position = u_proj * (gl_in[index].gl_Position + (vec4(gs_in[index].normal, 0.0) * kNormalMagnitude));
            EmitVertex();
            EndPrimitive();
        }

        void main()
        {
            GenerateLine(0); // first vertex normal
            GenerateLine(1); // second vertex normal
            GenerateLine(2); // third vertex normal
        }
    )";

    // Object Normal Visualization Fragment Shader
    static const char* const kObjectNormalVisFragmentShader = R"(
        out vec4 FragColor;

        void main()
        {
            FragColor = vec4(0.0, 1.0, 0.0, 1.0); // RGBA
        }
    )";

} // namespace