#pragma once
#include <triengine/shaders/shader_version.h>

namespace triengine::shaders
{
    // Physically-based bloom vertex shader
    static const char* const kPhysBloomVertexShader = R"glsl(
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
	        gl_Position = vec4(vsi_vertPos, 1.0);
        }
    )glsl";

    static const char* const kPhysBloomDownsampleFragmentShader = R"glsl(
		// This shader performs downsampling on a texture,
		// as taken from Call Of Duty method, presented at ACM Siggraph 2014.
		// This particular method was customly designed to eliminate
		// "pulsating artifacts and temporal stability issues".

		////////////////////////////////////////////
		// shader inputs
		////////////////////////////////////////////
		in VS_OUT {
			vec2 texCoord;
		} fsi;

		////////////////////////////////////////////
		// shader outputs
		////////////////////////////////////////////
		layout (location = 0) out vec3 fso_downsample;

		////////////////////////////////////////////
		// shader uniforms
		////////////////////////////////////////////
		// Remember to add bilinear minification filter for this texture!
		// Remember to use a floating-point texture format (for HDR)!
		// Remember to use edge clamping for this texture!
		layout(binding = 0) uniform sampler2D u_srcTexture;
		uniform vec2 u_srcTexelSize; // 1.0 / srcTextureSize;
		uniform int u_mipLevel = 1; // which mip we are writing to, used for Karis average

		vec3 PowVec3(vec3 v, float p) {
			return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
		}

		vec3 ToSRGB(vec3 color) { 
			const float invGamma = 1.0 / 2.2;
			return PowVec3(color, invGamma);
		}

		float sRGBToLuma(vec3 color) {
			//return dot(col, vec3(0.2126f, 0.7152f, 0.0722f));
			return dot(color, vec3(0.299f, 0.587f, 0.114f));
		}

		float KarisAverage(vec3 color) {
			// Formula is 1 / (1 + luma)
			const float luma = sRGBToLuma(ToSRGB(color)) * 0.25f;
			return 1.0f / (1.0f + luma);
		}

		// NOTE: This is the readable version of this shader. It will be optimized!
		void main()
		{
			const float x = u_srcTexelSize.x;
			const float y = u_srcTexelSize.y;

			// Take 13 samples around current texel:
			// a - b - c
			// - j - k -
			// d - e - f
			// - l - m -
			// g - h - i
			// === ('e' is the current texel) ===
			const vec3 a = texture(u_srcTexture, vec2(fsi.texCoord.x - 2*x, fsi.texCoord.y + 2*y)).rgb;
			const vec3 b = texture(u_srcTexture, vec2(fsi.texCoord.x,       fsi.texCoord.y + 2*y)).rgb;
			const vec3 c = texture(u_srcTexture, vec2(fsi.texCoord.x + 2*x, fsi.texCoord.y + 2*y)).rgb;

			const vec3 d = texture(u_srcTexture, vec2(fsi.texCoord.x - 2*x, fsi.texCoord.y)).rgb;
			const vec3 e = texture(u_srcTexture, vec2(fsi.texCoord.x,       fsi.texCoord.y)).rgb;
			const vec3 f = texture(u_srcTexture, vec2(fsi.texCoord.x + 2*x, fsi.texCoord.y)).rgb;

			const vec3 g = texture(u_srcTexture, vec2(fsi.texCoord.x - 2*x, fsi.texCoord.y - 2*y)).rgb;
			const vec3 h = texture(u_srcTexture, vec2(fsi.texCoord.x,       fsi.texCoord.y - 2*y)).rgb;
			const vec3 i = texture(u_srcTexture, vec2(fsi.texCoord.x + 2*x, fsi.texCoord.y - 2*y)).rgb;

			const vec3 j = texture(u_srcTexture, vec2(fsi.texCoord.x - x, fsi.texCoord.y + y)).rgb;
			const vec3 k = texture(u_srcTexture, vec2(fsi.texCoord.x + x, fsi.texCoord.y + y)).rgb;
			const vec3 l = texture(u_srcTexture, vec2(fsi.texCoord.x - x, fsi.texCoord.y - y)).rgb;
			const vec3 m = texture(u_srcTexture, vec2(fsi.texCoord.x + x, fsi.texCoord.y - y)).rgb;

			// Apply weighted distribution:
			// 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
			// a,b,d,e * 0.125
			// b,c,e,f * 0.125
			// d,e,g,h * 0.125
			// e,f,h,i * 0.125
			// j,k,l,m * 0.5
			// This shows 5 square areas that are being sampled. But some of them overlap,
			// so to have an energy preserving downsample we need to make some adjustments.
			// The weights are the distributed, so that the sum of j,k,l,m (e.g.)
			// contribute 0.5 to the final color output. The code below is written
			// to effectively yield this sum. We get:
			// 0.125*5 + 0.03125*4 + 0.0625*4 = 1

			// Check if we need to perform Karis average on each block of 4 samples
			const bool applyKarisAvg = u_mipLevel == 0;
			if (applyKarisAvg) {
				// We are writing to mip 0, so we need to apply Karis average to each block
				// of 4 samples to prevent fireflies (very bright subpixels, leads to pulsating
				// artifacts).
				vec3 groups[5];
				groups[0] = (a+b+d+e) * (0.125f/4.0f);
				groups[1] = (b+c+e+f) * (0.125f/4.0f);
				groups[2] = (d+e+g+h) * (0.125f/4.0f);
				groups[3] = (e+f+h+i) * (0.125f/4.0f);
				groups[4] = (j+k+l+m) * (0.5f/4.0f);
				groups[0] *= KarisAverage(groups[0]);
				groups[1] *= KarisAverage(groups[1]);
				groups[2] *= KarisAverage(groups[2]);
				groups[3] *= KarisAverage(groups[3]);
				groups[4] *= KarisAverage(groups[4]);
				fso_downsample = groups[0]+groups[1]+groups[2]+groups[3]+groups[4];
				fso_downsample = max(fso_downsample, 0.0001f);
			} else {
				fso_downsample = e*0.125;                // ok
				fso_downsample += (a+c+g+i)*0.03125;     // ok
				fso_downsample += (b+d+f+h)*0.0625;      // ok
				fso_downsample += (j+k+l+m)*0.125;       // ok
			}
		}
    )glsl";

    static const char* const kPhysBloomUpsampleFragmentShader = R"glsl(
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
    )glsl";

    static const char* const kPhysBloomCompositeFragmentShader = R"glsl(
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
    )glsl";



} // namespace