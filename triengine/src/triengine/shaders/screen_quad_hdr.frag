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
// screen-quad shader, with HDR tone-mapping

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