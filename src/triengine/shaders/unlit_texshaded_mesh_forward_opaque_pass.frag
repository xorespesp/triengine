// Texture-Shaded Unlit Mesh Object Fragment Shader (forward opaque pass)

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT
{
    vec2 texCoords; // texture uv coordinate
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out vec4 fso_fragColor;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
layout(binding = 0) uniform sampler2D u_diffuseMap; // base color map (RGB)

void main()
{
    // Unlit: output the base color directly (no lighting applied).
    fso_fragColor = vec4(texture(u_diffuseMap, fsi.texCoords).rgb, 1.0);
}
