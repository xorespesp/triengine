// Overlay Rendering Composite Pass Fragment Shader (for skeleton_renderer)

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