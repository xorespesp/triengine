#pragma once
#include <triengine/common.h>
#include <triengine/core/shader_storage_vector.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/geometry/pcd_object.hh>

#include <cstdint>

namespace triengine::core
{
    class shader_program;
}

namespace triengine::renderer
{
    // `PhongShadedObjectMaterial` of shaders/includes/phong_material.glsl
    struct glsl_phong_shaded_object_material
    {
        float ambientIntensity;
        float diffuseIntensity;
        float specularIntensity;
        float shininess;
        TRIENGINE_GLSL_STRUCT(glsl_phong_shaded_object_material, ambientIntensity, diffuseIntensity, specularIntensity, shininess)
    };

    // `PhongShadedObjectRecord` of shaders/includes/phong_shaded_object_block.glsl
    struct glsl_phong_shaded_object_record
    {
        mat4_f32 model;
        mat3_f32 normalMatrixInWorld;
        glsl_phong_shaded_object_material material;
        float alpha;
        TRIENGINE_GLSL_STRUCT(glsl_phong_shaded_object_record, model, normalMatrixInWorld, material, alpha)
    };

    // `PhongShadedObjectBlock`, one record per draw
    using phong_shaded_object_storage_vector = core::shader_block::shader_storage_vector<glsl_phong_shaded_object_record>;

    // `binding` of `PhongShadedObjectBlock`
    inline constexpr uint32_t kPhongShadedObjectBlockBindingPointIndex = 1;

    glsl_phong_shaded_object_record make_phong_shaded_object_record(const mat4_f32& model, const geometry::mesh_object::vertex_shading_material& material);
    glsl_phong_shaded_object_record make_phong_shaded_object_record(const mat4_f32& model, const geometry::mesh_object::texture_shading_material& material);
    glsl_phong_shaded_object_record make_phong_shaded_object_record(const mat4_f32& model, const geometry::pcd_object::material_t& material);

    // Call it after linking a program whose shaders include phong_shaded_object_block.glsl
    void verify_phong_shaded_object_block(const core::shader_program& program);

} // namespace
