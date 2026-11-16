#include "pcd_renderer.hh"
#include <triengine/renderer/camera_block.hh>
#include <triengine/renderer/lighting_block.hh>
#include <triengine/renderer/phong_shaded_object_block.hh>

#include <triengine/utility/debug_utils.hh>

namespace triengine::renderer
{
    pcd_renderer::pcd_renderer()
    { }

    pcd_renderer::~pcd_renderer()
    {
        this->destroy_impl();
    }

    void pcd_renderer::create_impl(core::gl_context& glctx)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        glctx.make_context_current();

        _glctx = &glctx;

        //
        // Context Settings
        //

        GLCall(::glEnable(GL_PROGRAM_POINT_SIZE));

        const auto shader_ldr = glctx.get_shader_loader();

        // Create shader program
        _deferred_opaque_pass_shader
            .attach_vertex_shader({ shader_ldr->load("pcd_deferred_geom_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("pcd_deferred_geom_pass.frag")->c_str() })
            .link();
        verify_camera_block(_deferred_opaque_pass_shader);
        verify_phong_shaded_object_block(_deferred_opaque_pass_shader);
        _deferred_opaque_pass_records.emplace(kPhongShadedObjectBlockBindingPointIndex);

        _forward_trans_pass_shader
            .attach_vertex_shader({ shader_ldr->load("pcd_forward_trans_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("pcd_forward_trans_pass.frag")->c_str() })
            .link();
        verify_camera_block(_forward_trans_pass_shader);
        verify_lighting_block(_forward_trans_pass_shader);
        verify_phong_shaded_object_block(_forward_trans_pass_shader);
        _forward_trans_pass_records.emplace(kPhongShadedObjectBlockBindingPointIndex);
    }

    void pcd_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            _deferred_opaque_pass_shader.destroy();
            _deferred_opaque_pass_records.reset();
            _forward_trans_pass_shader.destroy();
            _forward_trans_pass_records.reset();

            _glctx = nullptr;
        }
    }

    void pcd_renderer::set_pcd_point_size(float point_size)
    {
        _point_size = point_size;
    }

    void pcd_renderer::render_impl(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::pcd_object>>& render_obj_list,
        pred_callback_type<geometry::pcd_object> const predicate,
        void* const predicate_userdata)
    {
        if (render_obj_list.empty()) {
            return;
        }

        // Enable depth testing
        //GLCall(::glEnable(GL_DEPTH_TEST));

        // Update point size
        GLCall(::glPointSize(static_cast<GLfloat>(_point_size.value_or(1.0f/* default size */))));

        switch (render_ctx.curr_render_pass) {
        case render_pass_type::deferred_opaque_pass:
        {
            auto* const draw_shader = &_deferred_opaque_pass_shader;
            auto& object_records = *_deferred_opaque_pass_records;

            draw_shader->use();

            // Write a record per draw and collect the draws
            object_records.clear();
            _draw_items.clear();

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_obj_list)
            {
                if (!object->is_opaque()) {
                    continue;
                }

                if (!object->is_visible()) {
                    continue;
                }

                if (predicate && !predicate(*object, predicate_userdata)) {
                    continue;
                }

                const auto object_gpu_rsrc = gpu_rsrc_mgr->get_pcd_geometry_resource(object);
                if (!object_gpu_rsrc) {
                    continue;
                }

                const auto& vertex_positions = object->points;
                const auto& material = object->material;

                // Update VAO (if needed)
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                const size_t record_index = object_records.push_back(
                    make_phong_shaded_object_record(object->get_model(), material)
                );
                _draw_items.push_back(draw_item{
                    object_gpu_rsrc->vao,
                    static_cast<int32_t>(vertex_positions.size()),
                    static_cast<uint32_t>(record_index),
                });
            } // for

            // Upload the records written above, before the draws that read them
            object_records.upload();
            object_records.bind();

            // Render pointcloud
            for (const draw_item& item : _draw_items)
            {
                GLCall(::glBindVertexArray(item.vao));
                // Draws like `glDrawArrays`, and also hands `baseinstance` to the shaders as `gl_BaseInstance`.
                // With one instance, that number is just the index of this object's record in `u_phongShadedObjects`.
                GLCall(::glDrawArraysInstancedBaseInstance(
                    GL_POINTS,
                    0/* GLint first */,
                    item.vertex_count,
                    1/* GLsizei instancecount */,
                    item.record_index/* GLuint baseinstance */
                ));
                //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
            } // for

            break;
        }
        case render_pass_type::forward_transparent_pass:
        {
            auto* const draw_shader = &_forward_trans_pass_shader;
            auto& object_records = *_forward_trans_pass_records;

            draw_shader->use();

            // Write a record per draw and collect the draws
            object_records.clear();
            _draw_items.clear();

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_obj_list)
            {
                if (object->is_opaque()) { 
                    continue;
                }

                if (!object->is_visible()) {
                    continue;
                }

                if (predicate && !predicate(*object, predicate_userdata)) {
                    continue;
                }

                const auto object_gpu_rsrc = gpu_rsrc_mgr->get_pcd_geometry_resource(object);
                if (!object_gpu_rsrc) {
                    continue;
                }

                const auto& vertex_positions = object->points;
                const auto& material = object->material;

                // Update VAO (if needed)
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                const size_t record_index = object_records.push_back(
                    make_phong_shaded_object_record(object->get_model(), material)
                );
                _draw_items.push_back(draw_item{
                    object_gpu_rsrc->vao,
                    static_cast<int32_t>(vertex_positions.size()),
                    static_cast<uint32_t>(record_index),
                });
            } // for

            // Upload the records written above, before the draws that read them
            object_records.upload();
            object_records.bind();

            // Render pointcloud
            for (const draw_item& item : _draw_items)
            {
                GLCall(::glBindVertexArray(item.vao));
                // Draws like `glDrawArrays`, and also hands `baseinstance` to the shaders as `gl_BaseInstance`.
                // With one instance, that number is just the index of this object's record in `u_phongShadedObjects`.
                GLCall(::glDrawArraysInstancedBaseInstance(
                    GL_POINTS,
                    0/* GLint first */,
                    item.vertex_count,
                    1/* GLsizei instancecount */,
                    item.record_index/* GLuint baseinstance */
                ));
                //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
            } // for

            break;
        }
        default:
            TRIENGINE_PANIC("%s(): Unsupported render pass type: %d"
                , __func__
                , static_cast<int>(render_ctx.curr_render_pass)
            );
        }
    }

} // namespace
