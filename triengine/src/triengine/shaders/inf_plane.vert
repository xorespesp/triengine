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
    vec3 vertPos; // vertex position in world space
    vec3 eyePos; // camera position in world-space
    float planeHalfSize; // half-size of the entire plane in world space (unit: [m])
    float gridCellSize; // plane grid cell size in world space (unit: [m])
} vso;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform mat4 u_view; // view matrix
uniform mat4 u_proj; // projection matrix
uniform vec3 u_eyePos; // camera position in world-space
uniform float u_maxViewDist = 100.0; // half-size of the entire plane in world space (max view distance; unit: [m])
uniform float u_gridCellSize = 1.0;

void main()
{
    const int currVertexIndex = g_vertexIndices[gl_VertexID];
            
    vec3 currVertexPos = g_vertexPositions[currVertexIndex];
    currVertexPos *= u_maxViewDist;
    currVertexPos.x += u_eyePos.x;
    currVertexPos.z += u_eyePos.z;

    gl_Position = u_proj * u_view * vec4(currVertexPos, 1.0);
    vso.vertPos = currVertexPos;
    vso.eyePos = u_eyePos;
    vso.planeHalfSize = u_maxViewDist;
    vso.gridCellSize = u_gridCellSize;
}