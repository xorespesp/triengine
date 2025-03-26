#pragma once
#include "shader_version.h"

namespace triengine::shaders
{
    // Lighting Source Object Vertex Shader
    static const char* const kLightSourceVertexShader = R"glsl(
		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
        layout (location = 0) in vec3 vsi_vertPos; // object-space vertex position

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
        uniform mat4 u_model; // model matrix
        uniform mat4 u_view; // view matrix
        uniform mat4 u_proj; // projection matrix

        void main()
        {
            gl_Position = u_proj * u_view * u_model * vec4(vsi_vertPos, 1.0);
        }
    )glsl";

    // Lighting Source Object Fragment Shader
    static const char* const kLightSourceFragmentShader = R"glsl(
		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
        out vec4 fso_fragColor;
            
		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
        uniform vec3 u_color;

        void main()
        {
            fso_fragColor = vec4(u_color, 1.0/* alpha */);
        }
    )glsl";

} // namespace