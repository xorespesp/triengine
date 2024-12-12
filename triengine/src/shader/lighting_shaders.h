#pragma once
#include "shader_version.h"

namespace triengine::shader
{
    // Lighting Source Object Vertex Shader
    static const char* const kLightSourceVertexShader = R"(
        layout (location = 0) in vec3 vi_vertPos; // object-space vertex position

        uniform mat4 u_model; // model matrix
        uniform mat4 u_view; // view matrix
        uniform mat4 u_proj; // projection matrix

        void main()
        {
            gl_Position = u_proj * u_view * u_model * vec4(vi_vertPos, 1.0);
        }
    )";

    // Lighting Source Object Fragment Shader
    static const char* const kLightSourceFragmentShader = R"(
        out vec4 fo_fragColor;
            
        uniform vec3 u_color;

        void main()
        {
            fo_fragColor = vec4(u_color, 1.0/* alpha */);
        }
    )";

} // namespace