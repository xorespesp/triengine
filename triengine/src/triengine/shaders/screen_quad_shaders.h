#pragma once
#include <triengine/shaders/shader_version.h>

namespace triengine::shaders
{
	//
	// Screen-Quad Shader
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


	//
	// Screen-Quad Shader, with HDR & tone-mapping
	//

	static const char* const kHDRScreenQuadVertexShader = R"glsl(
		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
		layout (location = 0) in vec3 vi_vertPos;
		layout (location = 1) in vec2 vi_texCoord;

		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
		out VS_OUT {
			vec2 texCoord;
		} vo;

		void main()
		{
			vo.texCoord = vi_texCoord;
			gl_Position = vec4(vi_vertPos, 1.0f);
		}
    )glsl";

	static const char* const kHDRScreenQuadFragmentShader = R"glsl(
		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
		in VS_OUT {
			vec2 texCoord;
		} fi;

		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
		layout (location = 0) out vec4 fo_frag;

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
		uniform sampler2D u_screenTexture; // screen image texture
		uniform float u_exposure;

		void main()
		{
			vec4 screenColor = texture(u_screenTexture, fi.texCoord).rgba; //vec4(texture(u_screenTexture, fi.texCoord).rgb, 1.0f);
	
			// tone mapping
			screenColor.rgb = vec3(1.0) - exp(-screenColor.rgb * u_exposure);

			// gamma correct
			const float invGamma = 1.0 / 2.2;
			screenColor.rgb = pow(screenColor.rgb, vec3(invGamma));

			fo_frag = screenColor.rgba;
		}
    )glsl";

} // namespace