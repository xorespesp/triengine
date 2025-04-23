#pragma once
#include <triengine/shaders/shader_version.h>

namespace triengine::shaders
{
    // Lineset Object Vertex Shader
    static const char* const kLinesetVertexShader = R"glsl(
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
            vec3 fragColor; // fragment color.
        } vso;

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
        uniform mat4 u_model; // model matrix
        uniform mat4 u_view; // view matrix
        uniform mat4 u_proj; // projection matrix

        void main()
        {
            gl_Position = u_proj * u_view * u_model * vec4(vsi_vertPos, 1.0);
            vso.fragColor = vsi_vertColor;
        }
    )glsl";
    
    // Lineset Object Fragment Shader
    static const char* const kLinesetFragmentShader = R"glsl(
		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
        in VS_OUT
        {
            vec3 fragColor; // fragment color
        } fsi;

		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
        out vec4 fso_fragColor;

        void main()
        {
            fso_fragColor = vec4(fsi.fragColor, 1.0/* alpha */);
        }
    )glsl";

} // namespace