// Camera of the frame, shared by every program that declares the block.
// C++: `renderer::camera_block` (renderer/camera_block.hh)

layout(std140, binding = 0) uniform CameraBlock
{
    mat4 u_view; // view matrix
    mat4 u_proj; // projection matrix
    vec3 u_eyePosInWorld; // camera position in world-space
};
