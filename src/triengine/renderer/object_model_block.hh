#pragma once
#include <triengine/common.h>
#include <triengine/core/shader_storage_vector.hh>

#include <cstdint>

namespace triengine::core
{
    class shader_program;
}

namespace triengine::renderer
{
    // `ObjectModelBlock` of shaders/includes/object_model_block.glsl, one model matrix per draw of an object without a material
    using object_model_storage_vector = core::shader_block::shader_storage_vector<mat4_f32>;

    // `binding` of `ObjectModelBlock`
    inline constexpr uint32_t kObjectModelBlockBindingPointIndex = 2;

    // Call it after linking a program whose shaders include object_model_block.glsl
    void verify_object_model_block(const core::shader_program& program);

} // namespace
