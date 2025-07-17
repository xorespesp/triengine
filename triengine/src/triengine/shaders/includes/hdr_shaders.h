#pragma once

namespace triengine::shaders::includes
{
    static const char* const kHDRShader = R"glsl(
        /**
         * HDR Tone-mapping algorithm(curve equation)s
         */
        
        subroutine vec3 ToneMappingCurveEquation(vec3 color); // Subroutine Type
        
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

        /**
         * HDR Gamma Correct Function
         */

        vec3 toSRGB(vec3 color)
        { 
            const float invGamma = 1.0 / 2.2;
            return pow(color, vec3(invGamma));
        }

    )glsl";

} // namespace