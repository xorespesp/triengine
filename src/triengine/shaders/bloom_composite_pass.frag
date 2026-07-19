// Phys-based-bloom fragment shader (composite pass)

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT {
	vec2 texCoord;
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out vec4 fso_fragColor;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
layout(binding = 0) uniform sampler2D u_sceneTexture;
layout(binding = 1) uniform sampler2D u_bloomBlurTexture;
uniform float u_bloomStrength = 0.04f;

void main()
{
	const vec3 sceneColor = texture(u_sceneTexture, fsi.texCoord).rgb;
	const vec3 bloomColor = texture(u_bloomBlurTexture, fsi.texCoord).rgb;
    
	// mix bloom
	vec3 mixedColor = mix(sceneColor, bloomColor, u_bloomStrength); // linear interpolation

	fso_fragColor = vec4(mixedColor.rgb, 1.0);
}