// Object Normal Visualization Geometry Shader

layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
in VS_OUT {
    vec3 vertex_color;
    vec4 normal_end_position_clip;
} gsi[];

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out GS_OUT {
    vec3 vertex_color;
} gso;

const float kNormalMagnitude = 0.03; // Unit: [m]
const float kMaxRenderDist = 5.0; // Unit: [m]

void GenerateLine(int index)
{
    const vec4 vertex_position_clip = gl_in[index].gl_Position;

    const float squared_distance = dot(vertex_position_clip, vertex_position_clip); // use dot istead of length to prevent sqrt
    if (squared_distance < kMaxRenderDist * kMaxRenderDist)
    {
        gso.vertex_color = gsi[index].vertex_color;

        gl_Position = vertex_position_clip;
        EmitVertex();

        gl_Position = gsi[index].normal_end_position_clip;
        EmitVertex();

        EndPrimitive();
    }
}

void main()
{
    GenerateLine(0); // first vertex normal
    GenerateLine(1); // second vertex normal
    GenerateLine(2); // third vertex normal
}