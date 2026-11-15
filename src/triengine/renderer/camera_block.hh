#pragma once
#include <triengine/common.h>
#include <triengine/core/shader_block_buffer.hh>

#include <cstdint>

namespace triengine::core
{
    class shader_program;
}

namespace triengine::renderer
{
    struct render_context;

    // `CameraBlock` of shaders/includes/camera_block.glsl
    struct camera_block
    {
        mat4_f32 u_view;
        mat4_f32 u_proj;
        vec3_f32 u_eyePosInWorld;
        TRIENGINE_GLSL_BLOCK(camera_block, "CameraBlock", u_view, u_proj, u_eyePosInWorld)
    };

    // `layout(std140) uniform` of `CameraBlock`
    using camera_uniform_buffer = core::shader_block::uniform_buffer<camera_block>;

    // `binding` of `CameraBlock`
    inline constexpr uint32_t kCameraBlockBindingPointIndex = 0;

    camera_block make_camera_block(const render_context& render_ctx);

    // Call it after linking a program whose shaders include camera_block.glsl
    void verify_camera_block(const core::shader_program& program);

} // namespace
