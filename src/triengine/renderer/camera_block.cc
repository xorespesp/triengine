#include "camera_block.hh"
#include <triengine/renderer/renderer_base.hh>
#include <triengine/core/shader.hh>

namespace triengine::renderer
{
    namespace
    {
        using layout = camera_uniform_buffer::layout;

        // The offsets of camera_block.glsl
        static_assert(layout::offset_of<&camera_block::u_proj>() == 64);
        static_assert(layout::offset_of<&camera_block::u_eyePosInWorld>() == 128);
        static_assert(layout::buffer_size_for(0) == 144);
    } // namespace

    camera_block make_camera_block(const render_context& render_ctx)
    {
        return camera_block{
            render_ctx.view,
            render_ctx.projection,
            render_ctx.camera->get_position(),
        };
    }

    void verify_camera_block(const core::shader_program& program)
    {
        layout::verify(program.id(), camera_uniform_buffer::type(), camera_block::glsl_block_name(), kCameraBlockBindingPointIndex);
    }

} // namespace
