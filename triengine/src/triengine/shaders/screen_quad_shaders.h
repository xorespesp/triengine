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
        uniform sampler2D u_hdrScreenTexture; // HDR screen image texture
        uniform float u_exposure; // tone-mapping exposure

        subroutine vec3 ToneMappingCurveEquation(vec3 color); // Subroutine Type
        layout(location = 0) subroutine uniform ToneMappingCurveEquation u_tone_mapping_curve; // Subroutine uniform

        ////////////////////////////////////////////
        // tone-mapping method(algorithm)s
        ////////////////////////////////////////////

        // Reinhard Tone Mapping Curve
        // Refs: 
        // https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
        layout(index = 0) subroutine(ToneMappingCurveEquation) vec3 ReinhardToneMapping(vec3 hdrColor)
        {
            return hdrColor / (hdrColor + vec3(1.0));
        }

        // Uncharted 2 Filmic Tone Mapping Curve
        // Refs: 
        // http://filmicworlds.com/blog/filmic-tonemapping-operators/
        // https://blog.naver.com/sorkelf/221037468105
        layout(index = 1) subroutine(ToneMappingCurveEquation) vec3 Uncharted2FimicToneMapping(vec3 hdrColor)
        {
            const float A = 0.15; // A: Shoulder Strength
            const float B = 0.50; // B: Linear Strength
            const float C = 0.10; // C: Linear Angle
            const float D = 0.20; // D: Toe Strength
            const float E = 0.02; // E: Toe Numerator
            const float F = 0.30; // F: Toe Denominator (Note: E/F -> Toe Angle)
            const float W = 11.2; // W: Linear White Point Value

            // Apply the core Uncharted 2 tonemapping curve
            vec3 curr = ((hdrColor * (A * hdrColor + C * B) + D * E) / (hdrColor * (A * hdrColor + B) + D * F)) - E / F;

            // White point normalization:
            // The Uncharted 2 operator is often designed to work with a specific white point (W).
            // To ensure that this white point maps to an output of 1.0 (or a desired maximum) after tonemapping,
            // the result of the tonemapped color (curr) is divided by the result of tonemapping the white point itself.
            // This step helps to normalize the output brightness and achieve a consistent look,
            // similar to how it's described in John Hable's presentation.
            // It effectively scales the curve so that input W results in output 1.0.
            vec3 white = ((vec3(W) * (A * vec3(W) + C * B) + D * E) / (vec3(W) * (A * vec3(W) + B) + D * F)) - E / F;
            return curr / white;
        }

        // ACES Filmic Tone Mapping Curve (Krzysztof Narkowicz; Default tone mapping curve in Unreal Engine 4)
        // Refs: 
        // https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
        // https://github.com/ampas/aces-core
        layout(index = 2) subroutine(ToneMappingCurveEquation) vec3 ACESFilmicToneMapping(vec3 hdrColor)
        {
            const float A = 2.51f;
            const float B = 0.03f;
            const float C = 2.43f;
            const float D = 0.59f;
            const float E = 0.14f;

            hdrColor = (hdrColor * (A * hdrColor + B)) / (hdrColor * (C * hdrColor + D) + E);
            return clamp(hdrColor, 0.0, 1.0);
        }

        vec3 toSRGB(vec3 color)
        { 
            const float invGamma = 1.0 / 2.2;
            return pow(color, vec3(invGamma));
        }

        void main()
        {
            vec4 hdrColor = texture(u_hdrScreenTexture, fi.texCoord).rgba;

            // apply exposure
            hdrColor.rgb *= u_exposure;	

            // tone mapping
            vec3 mappedColor = u_tone_mapping_curve(hdrColor.rgb);

            // gamma correction
            fo_frag = vec4(toSRGB(mappedColor), hdrColor.a);
        }
    )glsl";

} // namespace