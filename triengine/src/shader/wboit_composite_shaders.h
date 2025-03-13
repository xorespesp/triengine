#pragma once
#include "shader_version.h"

namespace triengine::shader
{
	//
    // Weighted Blended Order-Independent Transparency (WBOIT) Composite Shader
	//

    static const char* const kWBOITCompositeVertexShader = R"(
		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
		layout (location = 0) in vec3 vsi_vertPos;

		void main()
		{
			gl_Position = vec4(vsi_vertPos, 1.0f);
		}
    )";

    static const char* const kWBOITCompositeFragmentShader = R"(
		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
		layout (location = 0) out vec4 fso_fragColor;

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
		layout (binding = 0) uniform sampler2D u_accum; // color accumulation buffer
		layout (binding = 1) uniform sampler2D u_reveal; // revealage threshold buffer

		const float EPSILON = 0.00001f;

		// calculate floating point numbers equality accurately
		bool cmp_f32(float a, float b) {
			return abs(a - b) <= (abs(a) < abs(b) ? abs(b) : abs(a)) * EPSILON;
		}

		// get the max value between three values
		float max3(vec3 v) {
			return max(max(v.x, v.y), v.z);
		}

		void main()
		{
			// fragment coordination
			const ivec2 fragCoords = ivec2(gl_FragCoord.xy);
	
			// fragment revealage
			const float revealage = texelFetch(u_reveal, fragCoords, 0).r;
	
			// save the blending and color texture fetch cost if there is not a transparent fragment
			if (cmp_f32(revealage, 1.0f)) {
				discard;
			}
 
			// fragment color
			vec4 accumulation = texelFetch(u_accum, fragCoords, 0);
			if (isinf(max3(abs(accumulation.rgb)))) {
				// suppress overflow
				accumulation.rgb = vec3(accumulation.a);
			}

			// prevent floating point precision bug
			const vec3 average_color = accumulation.rgb / max(accumulation.a, EPSILON);

			// blend pixels
			fso_fragColor = vec4(average_color, 1.0f - revealage);
		}
    )";

} // namespace