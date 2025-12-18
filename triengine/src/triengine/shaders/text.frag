// text.frag
//#version 460 core

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT {
    vec2 texCoord;
} fi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
layout (location = 0) out vec4 fo_textFragColor;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
layout (binding = 0) uniform sampler2D u_glyphAtlasTexture;

uniform vec3 u_textColor;

void main()
{
    const vec4 sampled = vec4(1.0, 1.0, 1.0, texture(u_glyphAtlasTexture, fi.texCoord).r);
    fo_textFragColor = vec4(u_textColor, 1.0) * sampled;
}