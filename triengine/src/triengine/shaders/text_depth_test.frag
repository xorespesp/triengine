// text_depth_test.frag
//#version 460 core

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT {
    vec2 texCoord;
    vec3 viewSpacePos; // view(camera) space position of the 3d label (for depth comparison)
    vec2 screenSpacePos; // screen space position of the 3d label (for depth texture sampling)
} fi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
layout (location = 0) out vec4 fo_textFragColor; // output text fragment color

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
layout (binding = 0) uniform sampler2D u_glyphAtlasTexture;
layout (binding = 1) uniform sampler2D u_depthTexture;

uniform vec3 u_textColor;
uniform float u_cameraNear;
uniform float u_cameraFar;
uniform float u_depthBias = 0.005;
uniform float u_occlusionAlpha = 0.1;

// Function to linearize raw depth buffer value
float linearizeDepth(float depth) {
    float z_ndc = depth * 2.0 - 1.0; // Convert from [0,1] to [-1,1]
    return (2.0 * u_cameraNear * u_cameraFar) / (u_cameraFar + u_cameraNear - z_ndc * (u_cameraFar - u_cameraNear));
}

bool isTextOccluded(float textDepth, vec2 textScreenPos) {
    // Sample the closest depth value from the depth buffer (view space's depth buffer)
    // and linearize it to convert it to linear view-space depth.
    const float closestDepth = linearizeDepth(texture(u_depthTexture, textScreenPos).r);
    
    const float bias = u_depthBias;

    return textDepth - bias > closestDepth;
}

void main()
{    
    // Sample the glyph texture
    const vec4 sampled = vec4(1.0, 1.0, 1.0, texture(u_glyphAtlasTexture, fi.texCoord).r);
    vec4 textColor = vec4(u_textColor, 1.0) * sampled;
    
    // Early exit if alpha is too low
    if (textColor.a < 0.01) {
        discard;
    }
    
    // Get text depth in linear view space
    const float textDepth = -fi.viewSpacePos.z; // Negate to make positive (view space has negative Z)
    
    if (isTextOccluded(textDepth, fi.screenSpacePos)) {
        // Text is occluded
        textColor.rgb *= 0.7;  // Slightly darken occluded text
        textColor.a *= u_occlusionAlpha;
    }
    
    fo_textFragColor = textColor;
}