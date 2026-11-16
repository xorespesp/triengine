#include "object_model_block.hh"
#include <triengine/core/shader.hh>

namespace triengine::renderer
{
    static_assert(object_model_storage_vector::layout::runtime_array_stride() == 64);

    void verify_object_model_block(const core::shader_program& program)
    {
        object_model_storage_vector::layout::verify(program.id(), object_model_storage_vector::type(),
            "ObjectModelBlock", kObjectModelBlockBindingPointIndex);
    }

} // namespace
