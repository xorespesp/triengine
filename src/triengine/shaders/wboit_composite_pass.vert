// WBOIT Composite Pass Vertex Shader

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
layout (location = 0) in vec3 vsi_vertPos;

void main()
{
	gl_Position = vec4(vsi_vertPos, 1.0f);
}