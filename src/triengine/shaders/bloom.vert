// Phys-based-bloom vertex shader
// Used in downsample & upsample & composite pass

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
layout (location = 0) in vec3 vsi_vertPos;
layout (location = 1) in vec2 vsi_texCoord;

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out VS_OUT {
    vec2 texCoord;
} vso;

void main()
{
	vso.texCoord = vsi_texCoord;
	gl_Position = vec4(vsi_vertPos, 1.0);
}