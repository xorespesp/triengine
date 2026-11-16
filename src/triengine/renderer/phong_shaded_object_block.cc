#include "phong_shaded_object_block.hh"
#include <triengine/core/shader.hh>

namespace triengine::renderer
{
    namespace
    {
        using record_layout = core::shader_block::block_layout<core::shader_block::layout_rule::std430, glsl_phong_shaded_object_record>;

        // The offsets of phong_shaded_object_block.glsl
        static_assert(record_layout::offset_of<&glsl_phong_shaded_object_record::normalMatrixInWorld>() == 64);
        static_assert(record_layout::offset_of<&glsl_phong_shaded_object_record::material>() == 112);
        static_assert(record_layout::offset_of<&glsl_phong_shaded_object_record::alpha>() == 128);
        static_assert(phong_shaded_object_storage_vector::layout::runtime_array_stride() == 144);

        // The three materials have the same Phong fields.
        template <class _Material>
        glsl_phong_shaded_object_record make_record(const mat4_f32& model, const _Material& material)
        {
            return glsl_phong_shaded_object_record{
                model,
                mat3_f32(model.topLeftCorner<3, 3>().inverse().transpose()),
                glsl_phong_shaded_object_material{
                    material.ambient_intensity,
                    material.diffuse_intensity,
                    material.specular_intensity,
                    static_cast<float>(material.shininess),
                },
                material.alpha,
            };
        }
    } // namespace

    glsl_phong_shaded_object_record make_phong_shaded_object_record(const mat4_f32& model, const geometry::mesh_object::vertex_shading_material& material)
    {
        return make_record(model, material);
    }

    glsl_phong_shaded_object_record make_phong_shaded_object_record(const mat4_f32& model, const geometry::mesh_object::texture_shading_material& material)
    {
        return make_record(model, material);
    }

    glsl_phong_shaded_object_record make_phong_shaded_object_record(const mat4_f32& model, const geometry::pcd_object::material_t& material)
    {
        return make_record(model, material);
    }

    void verify_phong_shaded_object_block(const core::shader_program& program)
    {
        phong_shaded_object_storage_vector::layout::verify(program.id(), phong_shaded_object_storage_vector::type(),
            "PhongShadedObjectBlock", kPhongShadedObjectBlockBindingPointIndex);
    }

} // namespace
