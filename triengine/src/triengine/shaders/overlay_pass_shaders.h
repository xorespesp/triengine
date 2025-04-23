#pragma once
#include <triengine/shaders/shader_version.h>

namespace triengine::shaders
{
	//
	// Overlay Rendering Composite Pass Shaders (for skeleton_renderer)
	//

	static const char* const kOverlayCompositePassVertexShader = R"glsl(
		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
		layout (location = 0) in vec3 vsi_vertPos;

		void main()
		{
			gl_Position = vec4(vsi_vertPos, 1.0f);
		}
    )glsl";

	static const char* const kOverlayCompositePassFragmentShader = R"glsl(
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
    )glsl";

} // namespace