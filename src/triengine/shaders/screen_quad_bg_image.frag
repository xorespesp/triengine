// Screen Quad Background Image Fragment Shader
// Fills the background region with a texture, applying an aspect-fit UV transform.

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
uniform sampler2D u_bgImage;       // background image to sample
uniform vec2 u_uvScale;            // centered UV scale produced by the aspect-fit mode
uniform vec4 u_letterboxColor;     // bar color for the contain fit mode (= bg_color)

void main()
{
    // Scale the UV around its center so the image keeps its aspect ratio.
    vec2 uv = 0.5 + (fi.texCoord - 0.5) * u_uvScale;

    // Only the contain fit mode produces UVs outside [0, 1] (letterbox bars);
    // stretch and cover keep the UV in range, so this branch never triggers for them.
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        fo_frag = u_letterboxColor;
    } else {
        fo_frag = texture(u_bgImage, uv);
    }
}
