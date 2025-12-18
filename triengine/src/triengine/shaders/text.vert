// text.vert
//#version 460 core

////////////////////////////////////////////
// shader inputs
////////////////////////////////////////////
layout (location = 0) in vec2 vi_glyphQuadVertPos;  // local vertex positions of glyph quad
layout (location = 1) in vec2 vi_glyphQuadTexCoord; // local uv coordinates of glyph quad

////////////////////////////////////////////
// shader outputs
////////////////////////////////////////////
out VS_OUT {
    vec2 texCoord;
} vo;

////////////////////////////////////////////
// shader uniforms
////////////////////////////////////////////
uniform mat4 u_glyphQuadProj; // orthographic projection matrix for glyph quad

////////////////////////////////////////////
// SSBO buffers
////////////////////////////////////////////

#define MAX_QUEUE_SIZE 1024 // NOTE: must be same as cpp side's max queue size!!
layout(std430, binding = 0) readonly restrict buffer TextRenderSSBOQueue
{
    mat4 glyphQuadModelsQueue[MAX_QUEUE_SIZE]; // glyph quad's model matrix(transformation matrix) queue
    vec4 glyphTexUVRectsQueue[MAX_QUEUE_SIZE]; // glyph texture uv rect(.x = u_min, .y = v_min, .z = u_width, .w = v_height) queue
} text_render_queue;

void main()
{
    const int workingIndex = gl_InstanceID; // current working index(instance id) of TextRenderSSBOQueue

    const mat4 glyphQuadModel = text_render_queue.glyphQuadModelsQueue[workingIndex];
    const vec4 glyphTexUVRect = text_render_queue.glyphTexUVRectsQueue[workingIndex];

    gl_Position = u_glyphQuadProj 
                * glyphQuadModel
                * vec4(vi_glyphQuadVertPos.xy, 0.0, 1.0);
                
    // [NOTE]
    // OpenGL texture coordinate origin: Bottom-Left of the image
    // First byte of FreeType Glyph bitmap buffer: Top-Left of the image
    // If the Glyph bitmap buffer is uploaded to texture as-is,
    // OpenGL stores the first byte of the bitmap buffer (Top-Left of image)
    // at v=0 (bottom of texture), resulting in the glyph being stored upside-down in VRAM.
    // Therefore, when reading the Quad's local UV (0.0~1.0), the y-axis must be flipped (1.0 - y)
    // to sample the upside-down stored texture in the correct orientation.
    const vec2 flippedTexCoord = vec2(vi_glyphQuadTexCoord.x, 1.0 - vi_glyphQuadTexCoord.y);
    
    // [UV Coordinate Mapping]
    // `vi_glyphQuadTexCoord` is in the range (0,0)~(1,1).
    // This is scaled/translated to a specific region within the atlas (glyphTexUVRect).
    // glyphTexUVRect.xy = uv_min (start position)
    // glyphTexUVRect.zw = uv_width, uv_height (size)
    vo.texCoord = glyphTexUVRect.xy + (flippedTexCoord * glyphTexUVRect.zw);
}