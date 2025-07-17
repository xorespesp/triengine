#pragma once
#include <triengine/shaders/shader_version.h>

namespace triengine::shaders
{
    // Screen-Quad Vertex Shader
    static const char* const kScreenQuadVertexShader = R"glsl(
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

    /////////////////////////////////////////////////////////////////////

    // Screen-Quad Fragment Shader
    static const char* const kScreenQuadFragmentShader = R"glsl(
        ////////////////////////////////////////////
        // shader inputs
        ////////////////////////////////////////////
        in VS_OUT {
            vec2 texCoord;
        } fi;

        ////////////////////////////////////////////
        // shader outputs
        ////////////////////////////////////////////
        layout (location = 0) out vec4 fo_fragColor;

        ////////////////////////////////////////////
        // shader uniforms
        ////////////////////////////////////////////
        layout (binding = 0) uniform sampler2D u_screenTexture; // screen image texture

        void main()
        {
            vec4 colorPixel = texture(u_screenTexture, fi.texCoord).rgba;
            fo_fragColor = colorPixel.rgba; //vec4(colorPixel.rgb, 1.0f);
        }
    )glsl";

    // Screen-Quad Fragment Shader (with HDR)
    static const char* const kScreenQuadFragmentShader_HDR = R"glsl(
        #include <hdr>

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
        layout (binding = 0) uniform sampler2D u_screenHdrTexture; // HDR screen color texture
        uniform float u_exposure; // tone-mapping exposure
        layout(location = 0) subroutine uniform ToneMappingCurveEquation u_toneMappingCurve; // Subroutine uniform

        void main()
        {
            vec4 hdrColor = texture(u_screenHdrTexture, fi.texCoord).rgba;

            // apply exposure
            hdrColor.rgb *= u_exposure;	

            // tone mapping
            vec3 mappedColor = u_toneMappingCurve(hdrColor.rgb);

            // gamma correction
            fo_frag = vec4(toSRGB(mappedColor), hdrColor.a);
        }
    )glsl";

} // namespace