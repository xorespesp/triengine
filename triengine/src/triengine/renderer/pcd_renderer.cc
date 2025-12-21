#include "pcd_renderer.hh"

#include <triengine/utility/debug_utils.hh>

namespace triengine::renderer
{
    pcd_renderer::pcd_renderer()
    { }

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

        _forward_trans_pass_shader
            .attach_vertex_shader({ shader_ldr->load("pcd_forward_trans_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("pcd_forward_trans_pass.frag")->c_str() })
            .link();
    }

    void pcd_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            _deferred_opaque_pass_shader.destroy();
            _forward_trans_pass_shader.destroy();

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

            draw_shader->use();

            // Update view/projective matrices in shader
            draw_shader->set_uniform_mat4("u_view", render_ctx.view);
            draw_shader->set_uniform_mat4("u_proj", render_ctx.projection);

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

                const auto gpu_rsrc = gpu_rsrc_mgr->get_pcd_resource(object);
                if (!gpu_rsrc) {
                    continue;
                }

                const auto& vertex_positions = object->points;
                const auto& material = object->material;

                // Update model(transform) matrix in shader
                const mat4_f32& model = object->get_model();
                draw_shader->set_uniform_mat4("u_model", model);

                // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
                draw_shader->set_uniform_mat3("u_nmv", 
                    (render_ctx.view * model).inverse().transpose().topLeftCorner<3, 3>());

                // Update material
                draw_shader->set_uniform_float("u_phongMaterial.ambientIntensity", material.ambient_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.diffuseIntensity", material.diffuse_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.specularIntensity", material.specular_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.shininess", static_cast<float>(material.shininess));

                // Update VAO (if needed)
                if (object->is_dirty()) {
                    gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                // Render pointcloud
                GLCall(::glBindVertexArray(gpu_rsrc->vao));
                GLCall(::glDrawArrays(
                    GL_POINTS,
                    0,
                    static_cast<GLsizei>(vertex_positions.size())
                ));
                //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
            } // for

            break;
        }
        case render_pass_type::forward_transparent_pass:
        {
            auto* const draw_shader = &_forward_trans_pass_shader;

            draw_shader->use();

            // Update view/projective matrices in shader
            draw_shader->set_uniform_mat4("u_view", render_ctx.view);
            draw_shader->set_uniform_mat4("u_proj", render_ctx.projection);

            // Update light options in shader
            render_ctx.light_opts->dir_light.apply_to_shader(*draw_shader, render_ctx.view);
            render_ctx.light_opts->point_light.apply_to_shader(*draw_shader, render_ctx.view);
            render_ctx.light_opts->simple_fog.apply_to_shader(*draw_shader, render_ctx.simple_fog_color);

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

                const auto gpu_rsrc = gpu_rsrc_mgr->get_pcd_resource(object);
                if (!gpu_rsrc) {
                    continue;
                }

                const auto& vertex_positions = object->points;
                const auto& material = object->material;

                // Update model(transform) matrix in shader
                const mat4_f32& model = object->get_model();
                draw_shader->set_uniform_mat4("u_model", model);

                // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
                draw_shader->set_uniform_mat3("u_nmv", 
                    (render_ctx.view * model).inverse().transpose().topLeftCorner<3, 3>());

                // Update material
                draw_shader->set_uniform_float("u_phongMaterial.ambientIntensity", material.ambient_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.diffuseIntensity", material.diffuse_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.specularIntensity", material.specular_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.shininess", static_cast<float>(material.shininess));

                // Update alpha material (WBOIT)
                draw_shader->set_uniform_float("u_alpha", material.alpha);

                // Update VAO (if needed)
                if (object->is_dirty()) {
                    gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                // Render pointcloud
                GLCall(::glBindVertexArray(gpu_rsrc->vao));
                GLCall(::glDrawArrays(
                    GL_POINTS,
                    0,
                    static_cast<GLsizei>(vertex_positions.size())
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
