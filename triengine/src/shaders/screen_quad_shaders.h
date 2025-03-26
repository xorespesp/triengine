#pragma once
#pragma once
#include "shader_version.h"

namespace triengine::shaders
{
	//
	// Screen-Quad Shader (Used in WBOIT)
	//

	static const char* const kScreenQuadVertexShader = R"glsl(
		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
		layout (location = 0) in vec3 vsi_vertPos;
		layout (location = 1) in vec2 vsi_texCoord;

		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
		out VS_OUT {
			vec2 texCoord;
		} vso;

		void main()
		{
			vso.texCoord = vsi_texCoord;

			gl_Position = vec4(vsi_vertPos, 1.0f);
		}
    )glsl";

	static const char* const kScreenQuadFragmentShader = R"glsl(
		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
		in VS_OUT {
			vec2 texCoord;
		} fsi;

		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
		layout (location = 0) out vec4 fso_fragColor;

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
		uniform sampler2D u_screenTexture; // screen image texture

		void main()
		{
			fso_fragColor = 
				texture(u_screenTexture, fsi.texCoord).rgba;
				//vec4(texture(u_screenTexture, fsi.texCoord).rgb, 1.0f);
		}
    )glsl";

} // namespace