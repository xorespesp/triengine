// A runtime-sized array that the CPU writes and shaders read. (CPU->GPU only)
// see: `core::shader_block::shader_storage_vector` (core/shader_storage_vector.hh)
//
//   DECL_STORAGE_VECTOR(3, ParticleBlock, Particle, u_particles); // read as u_particles.elements[i]
//
// NOTE: `elements.length()` is the capacity, not the number of elements written this frame
#define DECL_STORAGE_VECTOR(BINDING_POINT_INDEX, BLOCK_NAME, ELEMENT_TYPE, INSTANCE_NAME) \
    layout(std430, binding = BINDING_POINT_INDEX) readonly buffer BLOCK_NAME { \
        ELEMENT_TYPE elements[]; /* NOTE: `elements` is a predefined naming rule */ \
    } INSTANCE_NAME
