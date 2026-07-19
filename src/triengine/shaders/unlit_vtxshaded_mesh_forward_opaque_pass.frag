// Vertex-Shaded Unlit Mesh Object Fragment Shader (forward opaque pass)

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT
{
    vec3 fragColor; // fragment color
} fsi;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out vec4 fso_fragColor;

void main()
{
    // Unlit: output the interpolated vertex color directly (no lighting applied).
    fso_fragColor = vec4(fsi.fragColor, 1.0);
}
