// Infinite Grid Vertex Shader

const vec3 g_vertexPositions[4] = vec3[4](
	vec3(-1.0, 0.0, -1.0), // [0]: bottom left
	vec3( 1.0, 0.0, -1.0), // [1]: bottom right
	vec3( 1.0, 0.0,  1.0), // [2]: top right
	vec3(-1.0, 0.0,  1.0)  // [3]: top left
);

const int g_vertexIndices[6] = int[6](
	0, 2, 1, // first triangle 
	2, 0, 3  // second triangle
);
        
////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out VS_OUT
{
    vec3 vertPosInWorld; // vertex position in world space (for pattern calculation and falloff)
    vec3 fragPosInView; // fragment position in view space (for lighting)
    vec3 fragNormalInView; // fragment normal in view space (for lighting)
} vso;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform mat4 u_view; // view matrix
uniform mat4 u_proj; // projection matrix
uniform vec3 u_eyePosInWorld; // camera position in world-space
uniform float u_planeHalfSize = 100.0; // half-size of the entire plane in world space (max view distance; unit: [m])
uniform float u_gridCellSize = 1.0; // plane grid cell size in world space (unit: [m])

void main()
{
    const int currVertexIndex = g_vertexIndices[gl_VertexID];
            
    vec3 currVertexPos = g_vertexPositions[currVertexIndex];
    currVertexPos *= u_planeHalfSize;
    currVertexPos.x += u_eyePosInWorld.x;
    currVertexPos.z += u_eyePosInWorld.z;

    gl_Position = u_proj * u_view * vec4(currVertexPos, 1.0);
    
    vso.vertPosInWorld = currVertexPos;
    vso.fragPosInView = vec3(u_view * vec4(currVertexPos, 1.0));

    // For XZ-plane, normal is (0, 1, 0) in world space
    vso.fragNormalInView = normalize(mat3(u_view) * vec3(0.0, 1.0, 0.0));
}