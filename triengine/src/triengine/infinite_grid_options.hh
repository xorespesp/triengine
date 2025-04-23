#pragma once
#include <triengine/common.h>
#include <triengine/shader.hh>
#include <triengine/misc/gl_utils.hh>

namespace triengine
{
    struct infinite_grid_options
    {
        vec3_f32 grid_color{ 0.8f, 0.8f, 0.8f };
        float grid_cell_size{ 0.05f };

        void apply_to_shader(shader_program& shader) const
        {
#if defined (TRIENGINE_DEBUG_MODE)
            {
                // Ref: https://stackoverflow.com/a/62663705
                GLint curr_shader_id{ -1 };
                GLCall(::glGetIntegerv(GL_CURRENT_PROGRAM, &curr_shader_id));
                TRIENGINE_ASSERT(shader.id() == static_cast<GLuint>(curr_shader_id));
            }
#endif // ^^^ TRIENGINE_DEBUG_MODE ^^^
            
            shader.set_uniform_vec3("u_gridColor", this->grid_color);
            shader.set_uniform_float("u_gridCellSize", this->grid_cell_size);
        }
    };

} // namespace