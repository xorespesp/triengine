#include "pcd_renderer.hh"

#include <triengine/shaders/pcd_shaders.h>
#include <triengine/utility/debug_utils.hh>

namespace triengine::renderer
{
    pcd_renderer::pcd_renderer()
    { }

    void pcd_renderer::create_impl(core::gl_context& glctx, const core::shader_preprocessor& shader_prep)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        glctx.make_context_current();

        _glctx = &glctx;

        //
        // Context Settings
        //

        GLCall(::glEnable(GL_PROGRAM_POINT_SIZE));

        // Create shader program
        _solid_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kPcdVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kSolidPcdFragmentShader).c_str() })
            .link();

        _transparent_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kPcdVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kTransparentPcdFragmentShader).c_str() })
            .link();
    }

    void pcd_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            _solid_shader.destroy();
            _transparent_shader.destroy();

            _glctx = nullptr;
        }
    }

    void pcd_renderer::set_pcd_point_size(float point_size)
    {
        _point_size = point_size;
    }

    void pcd_renderer::render_impl(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
        pred_callback_type const predicate,
        void* const predicate_userdata)
    {
        if (render_obj_list.empty()) {
            return;
        }

        // Enable depth testing
        //GLCall(::glEnable(GL_DEPTH_TEST));

        auto& draw_shader =
            (render_ctx.curr_render_pass == render_pass_type::wboit_transparent_rendering)
            ? _transparent_shader
            : _solid_shader;

        draw_shader.use();

        // Update view/projective matrices in shader
        draw_shader.set_uniform_mat4("u_view", render_ctx.view);
        draw_shader.set_uniform_mat4("u_proj", render_ctx.projection);

        // Update light options in shader
        render_ctx.light_opts->dir_light.apply_to_shader(draw_shader);
        render_ctx.light_opts->point_light.apply_to_shader(draw_shader);

        // Update point size
        GLCall(::glPointSize(static_cast<GLfloat>(_point_size.value_or(1.0f/* default size */))));

        auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
        for (const auto& object : render_obj_list)
        {
            if (!object->is_visible()) {
                continue;
            }

            if (predicate && !predicate(*object, predicate_userdata)) {
                continue;
            }

            switch (render_ctx.curr_render_pass) {
            case render_pass_type::wboit_solid_rendering:
                if (!object->is_opaque()) { continue; }
                break;
            case render_pass_type::wboit_transparent_rendering:
                if (object->is_opaque()) { continue; }
                break;
            }

            const auto gpu_rsrc = gpu_rsrc_mgr->get_pcd_resource(object);
            if (!gpu_rsrc) {
                continue;
            }

            const auto& vertex_positions = object->points;
            const auto& material = object->material;

            // Update model(transform) matrix in shader
            const mat4_f32& model = object->get_model();
            draw_shader.set_uniform_mat4("u_model", model);

            // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
            draw_shader.set_uniform_mat3("u_nmv", (render_ctx.view * model).inverse().transpose().topLeftCorner<3, 3>());

            // Update material
            draw_shader.set_uniform_float("u_material.ambient", material.ambient);
            draw_shader.set_uniform_float("u_material.diffuse", material.diffuse);
            draw_shader.set_uniform_float("u_material.specular", material.specular);
            draw_shader.set_uniform_float("u_material.shininess", static_cast<float>(material.shininess));

            // Update alpha material (WBOIT)
            if (render_ctx.curr_render_pass == render_pass_type::wboit_transparent_rendering) {
                draw_shader.set_uniform_float("u_material.alpha", material.alpha);
            }

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
    }

} // namespace
