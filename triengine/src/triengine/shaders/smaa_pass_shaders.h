#pragma once
#include <triengine/shaders/shader_version.h>

namespace triengine::shaders
{
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // SMAA Edge Detection Pass Vertex Shader
    static const char* const kSMAAEdgeDetectionPassVertexShader = R"glsl(
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
            vec4 smaaRTMetrics;
            vec4 offsets[3];
        } vso;

        ////////////////////////////////////////////
        // shader uniforms
        ////////////////////////////////////////////
        uniform vec4 u_smaaRTMetrics; // `vec4(1.0 / screen_width_pixels, 1.0 / screen_height_pixels, screen_width_pixels, screen_height_pixels)`

        #define SMAA_INCLUDE_PS 0
        #define SMAA_INCLUDE_VS 1
        #define SMAA_GLSL_4 1
        #define SMAA_PRESET_ULTRA
        #define SMAA_RT_METRICS u_smaaRTMetrics
        #include <SMAA.hlsl>

        void main()
        {
	        vso.texCoord = vsi_texCoord;
            vso.smaaRTMetrics = u_smaaRTMetrics;

            SMAAEdgeDetectionVS(vsi_texCoord, vso.offsets);

	        gl_Position = vec4(vsi_vertPos, 1.0f);
        }
    )glsl";

    // SMAA Edge Detection Pass Fragment Shader
    static const char* const kSMAAEdgeDetectionPassFragmentShader = R"glsl(
        ////////////////////////////////////////////
        // shader inputs
        ////////////////////////////////////////////
        in VS_OUT {
            vec2 texCoord;
            vec4 smaaRTMetrics;
            vec4 offsets[3];
        } fsi;

        ////////////////////////////////////////////
        // shader outputs
        ////////////////////////////////////////////
        out vec2 fso_fragColor;

        ////////////////////////////////////////////
        // shader uniforms
        ////////////////////////////////////////////
        uniform sampler2D u_colorTex;

        #define SMAA_INCLUDE_PS 1
        #define SMAA_INCLUDE_VS 0
        #define SMAA_GLSL_4 1
        #define SMAA_PRESET_ULTRA
        #define SMAA_RT_METRICS fsi.smaaRTMetrics
        #include <SMAA.hlsl>

        void main()
        {
            fso_fragColor = SMAAColorEdgeDetectionPS(fsi.texCoord, fsi.offsets, u_colorTex); // vec2 type
        }
    )glsl";

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // SMAA Blending Weight Calculation Pass Vertex Shader
    static const char* const kSMAABlendingWeightPassVertexShader = R"glsl(
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
            vec4 smaaRTMetrics;
            vec2 pixCoord;
            vec4 offsets[3];
        } vso;

        ////////////////////////////////////////////
        // shader uniforms
        ////////////////////////////////////////////
        uniform vec4 u_smaaRTMetrics; // `vec4(1.0 / screen_width_pixels, 1.0 / screen_height_pixels, screen_width_pixels, screen_height_pixels)`

        #define SMAA_INCLUDE_PS 0
        #define SMAA_INCLUDE_VS 1
        #define SMAA_GLSL_4 1
        #define SMAA_PRESET_ULTRA
        #define SMAA_RT_METRICS u_smaaRTMetrics
        #include <SMAA.hlsl>

        void main()
        {
	        vso.texCoord = vsi_texCoord;
            vso.smaaRTMetrics = u_smaaRTMetrics;

            SMAABlendingWeightCalculationVS(vsi_texCoord, vso.pixCoord, vso.offsets);

	        gl_Position = vec4(vsi_vertPos, 1.0f);
        }
    )glsl";

    // SMAA Blending Weight Calculation Pass Fragment Shader
    static const char* const kSMAABlendingWeightPassFragmentShader = R"glsl(
        ////////////////////////////////////////////
        // shader inputs
        ////////////////////////////////////////////
        in VS_OUT {
            vec2 texCoord;
            vec4 smaaRTMetrics;
            vec2 pixCoord;
            vec4 offsets[3];
        } fsi;

        ////////////////////////////////////////////
        // shader outputs
        ////////////////////////////////////////////
        out vec4 fso_fragColor;

        ////////////////////////////////////////////
        // shader uniforms
        ////////////////////////////////////////////
        uniform vec4 u_subsampleIndices;

        #define SMAA_INCLUDE_PS 1
        #define SMAA_INCLUDE_VS 0
        #define SMAA_GLSL_4 1
        #define SMAA_PRESET_ULTRA
        #define SMAA_RT_METRICS fsi.smaaRTMetrics
        #include <SMAA.hlsl>

        layout(binding = 0) uniform SMAATexture2D(u_edgesTex);
        layout(binding = 1) uniform SMAATexture2D(u_areaTex);
        layout(binding = 2) uniform SMAATexture2D(u_searchTex);

        void main()
        {
            fso_fragColor = SMAABlendingWeightCalculationPS(
                fsi.texCoord, 
                fsi.pixCoord, 
                fsi.offsets, 
                u_edgesTex, 
                u_areaTex, 
                u_searchTex, 
                u_subsampleIndices // Just pass zero for SMAA 1x, see @SUBSAMPLE_INDICES.
            );
        }
    )glsl";

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // SMAA Neighbor Blending Pass Vertex Shader
    static const char* const kSMAANeighborBlendingPassVertexShader = R"glsl(
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
            vec4 smaaRTMetrics;
            vec4 offset;
        } vso;

        ////////////////////////////////////////////
        // shader uniforms
        ////////////////////////////////////////////
        uniform vec4 u_smaaRTMetrics; // `vec4(1.0 / screen_width_pixels, 1.0 / screen_height_pixels, screen_width_pixels, screen_height_pixels)`

        #define SMAA_INCLUDE_PS 0
        #define SMAA_INCLUDE_VS 1
        #define SMAA_GLSL_4 1
        #define SMAA_PRESET_ULTRA
        #define SMAA_RT_METRICS u_smaaRTMetrics
        #include <SMAA.hlsl>

        void main()
        {
	        vso.texCoord = vsi_texCoord;
            vso.smaaRTMetrics = u_smaaRTMetrics;

            SMAANeighborhoodBlendingVS(vsi_texCoord, vso.offset);

	        gl_Position = vec4(vsi_vertPos, 1.0f);
        }
    )glsl";

    // SMAA Neighbor Blending Pass Fragment Shader
    static const char* const kSMAANeighborBlendingPassFragmentShader = R"glsl(
        ////////////////////////////////////////////
        // shader inputs
        ////////////////////////////////////////////
        in VS_OUT {
            vec2 texCoord;
            vec4 smaaRTMetrics;
            vec4 offset;
        } fsi;

        ////////////////////////////////////////////
        // shader outputs
        ////////////////////////////////////////////
        out vec4 fso_fragColor;

        #define SMAA_INCLUDE_PS 1
        #define SMAA_INCLUDE_VS 0
        #define SMAA_GLSL_4 1
        #define SMAA_PRESET_ULTRA
        #define SMAA_RT_METRICS fsi.smaaRTMetrics
        #include <SMAA.hlsl>

        layout(binding = 0) uniform SMAATexture2D(u_colorTex);
        layout(binding = 1) uniform SMAATexture2D(u_blendTex);

        void main()
        {
            fso_fragColor = SMAANeighborhoodBlendingPS(fsi.texCoord, fsi.offset, u_colorTex, u_blendTex);
        }
    )glsl";

} // namespace