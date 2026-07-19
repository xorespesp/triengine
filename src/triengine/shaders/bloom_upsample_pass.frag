// Phys-based-bloom fragment shader (upsample pass)

// This shader performs upsampling on a texture,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT {
	vec2 texCoord;
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
layout (location = 0) out vec3 fso_upsample;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
// Remember to add bilinear minification filter for this texture!
// Remember to use a floating-point texture format (for HDR)!
// Remember to use edge clamping for this texture!
layout(binding = 0) uniform sampler2D u_srcTexture;
uniform float u_aspectRatio; // srcResolution.x / srcResolution.y
uniform float u_filterRadius;

void main()
{
	// The filter kernel is applied with a radius, specified in texture
	// coordinates, so that the radius will vary across mip resolutions.
	const float x = u_filterRadius;
	const float y = u_filterRadius * u_aspectRatio;

	// Take 9 samples around current texel:
	// a - b - c
	// d - e - f
	// g - h - i
	// === ('e' is the current texel) ===
	const vec3 a = texture(u_srcTexture, vec2(fsi.texCoord.x - x, fsi.texCoord.y + y)).rgb;
	const vec3 b = texture(u_srcTexture, vec2(fsi.texCoord.x,     fsi.texCoord.y + y)).rgb;
	const vec3 c = texture(u_srcTexture, vec2(fsi.texCoord.x + x, fsi.texCoord.y + y)).rgb;

	const vec3 d = texture(u_srcTexture, vec2(fsi.texCoord.x - x, fsi.texCoord.y)).rgb;
	const vec3 e = texture(u_srcTexture, vec2(fsi.texCoord.x,     fsi.texCoord.y)).rgb;
	const vec3 f = texture(u_srcTexture, vec2(fsi.texCoord.x + x, fsi.texCoord.y)).rgb;

	const vec3 g = texture(u_srcTexture, vec2(fsi.texCoord.x - x, fsi.texCoord.y - y)).rgb;
	const vec3 h = texture(u_srcTexture, vec2(fsi.texCoord.x,     fsi.texCoord.y - y)).rgb;
	const vec3 i = texture(u_srcTexture, vec2(fsi.texCoord.x + x, fsi.texCoord.y - y)).rgb;

	// Apply weighted distribution, by using a 3x3 tent filter:
	//  1   | 1 2 1 |
	// -- * | 2 4 2 |
	// 16   | 1 2 1 |
	const float inv16 = 1.0 / 16.0;
	fso_upsample =  e*4.0;
	fso_upsample += (b+d+f+h)*2.0;
	fso_upsample += (a+c+g+i);
	fso_upsample *= inv16;
}