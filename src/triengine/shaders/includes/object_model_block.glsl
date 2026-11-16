// Model matrices of objects without a material (linesets), read at the draw's `gl_BaseInstance`.
// C++: `renderer::object_model_storage_vector` (renderer/object_model_block.hh)

#include "storage_vector.glsl"

DECL_STORAGE_VECTOR(
    2/* BINDING_POINT_INDEX */,
    ObjectModelBlock/* BLOCK_NAME */,
    mat4/* ELEMENT_TYPE */,
    u_objectModels/* INSTANCE_NAME */
);
