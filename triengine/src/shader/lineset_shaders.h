#pragma once
#include "shader_version.h"

namespace triengine::shader
{
    // Lineset Object Vertex Shader
    static const char* const kLinesetVertexShader = R"(
        layout(location = 0) in vec3 vi_vertPos; // object-space vertex position
        layout(location = 1) in vec3 vi_vertColor; // vertex color
        
        out VertOut
        {
            vec3 fragColor; // fragment color.
        } vo;

        uniform mat4 u_model; // model matrix
        uniform mat4 u_view; // view matrix
        uniform mat4 u_proj; // projection matrix

        void main()
        {
            gl_Position = u_proj * u_view * u_model * vec4(vi_vertPos, 1.0);
            vo.fragColor = vi_vertColor;
        }
    )";
    
    // Lineset Object Fragment Shader
    static const char* const kLinesetFragmentShader = R"(
        in VertOut
        {
            vec3 fragColor; // fragment color
        } fi;

        out vec4 fo_fragColor;

        void main()
        {
            fo_fragColor = vec4(fi.fragColor, 1.0/* alpha */);
        }
    )";

} // namespace