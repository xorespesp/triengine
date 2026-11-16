// Per-object data of Phong-shaded objects (meshes, point clouds), read at the draw's `gl_BaseInstance`.
// C++: `renderer::glsl_phong_shaded_object_record` (renderer/phong_shaded_object_block.hh)

#include "phong_material.glsl"
#include "storage_vector.glsl"

struct PhongShadedObjectRecord
{
    mat4 model; // model matrix
    mat3 normalMatrixInWorld; // transpose(inverse(mat3(model))); `mat3(u_view) * normalMatrixInWorld` is the view-space one
    PhongShadedObjectMaterial material;
    float alpha; // object transparency (WBOIT)
};

DECL_STORAGE_VECTOR(
    1/* BINDING_POINT_INDEX */,
    PhongShadedObjectBlock/* BLOCK_NAME */,
    PhongShadedObjectRecord/* ELEMENT_TYPE */,
    u_phongShadedObjects/* INSTANCE_NAME */
);
