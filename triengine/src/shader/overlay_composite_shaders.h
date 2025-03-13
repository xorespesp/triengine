#pragma once
#include "shader_version.h"

namespace triengine::shader
{
	//
	// Overlay Rendering Composite Shader (for skeleton_renderer)
	//

	static const char* const kOverlayCompositeVertexShader = R"(
		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
		layout (location = 0) in vec3 vsi_vertPos;

		void main()
		{
			gl_Position = vec4(vsi_vertPos, 1.0f);
		}
    )";

	static const char* const kOverlayCompositeFragmentShader = R"(
		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
		layout (location = 0) out vec4 fso_fragColor;

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
		layout (binding = 0) uniform sampler2D u_srcFrame; // source color buffer

		void main()
		{
			// fragment coordination
			const ivec2 fragCoords = ivec2(gl_FragCoord.xy);
			
			// fetch source texture fragment (without interpolation)
			vec4 srcFragment = texelFetch(u_srcFrame, fragCoords, 0);

			// blend pixels
			fso_fragColor = srcFragment;
		}
    )";

} // namespace