// Overlay Rendering Composite Pass Vertex Shader (for skeleton_renderer)

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
layout (location = 0) in vec3 vsi_vertPos;

void main()
{
	gl_Position = vec4(vsi_vertPos, 1.0f);
}