#include "lineset_renderer.hh"

#include <triengine/utility/debug_utils.hh>

namespace triengine::renderer
{
    lineset_renderer::lineset_renderer()
    { }

    void lineset_renderer::create_impl(core::gl_context& glctx)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        glctx.make_context_current();

        _glctx = &glctx;

        //
        // Context Settings
        //

        const auto shader_ldr = glctx.get_shader_loader();

        // Create shader program
        _shader
            .attach_vertex_shader({ shader_ldr->load("lineset_forward_opaque_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("lineset_forward_opaque_pass.frag")->c_str() })
            .link();
    }

    void lineset_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            _shader.destroy();
            _glctx = nullptr;
        }
    }

    void lineset_renderer::render_impl(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
        pred_callback_type const predicate,
        void* const predicate_userdata)
    {
        if (render_ctx.curr_render_pass != render_pass_type::forward_opaque_pass) {
            return;
        }

        if (render_obj_list.empty()) {
            return;
        }

        auto& draw_shader = _shader;
        draw_shader.use();

        // Enable depth testing
        //GLCall(::glEnable(GL_DEPTH_TEST));

        // Enable smooth line
        //GLCall(::glEnable(GL_LINE_SMOOTH));

        GLCall(::glLineWidth(1.0f));

        // Update view/projective matrices in shader
        draw_shader.set_uniform_mat4("u_view", render_ctx.view);
        draw_shader.set_uniform_mat4("u_proj", render_ctx.projection);

        auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
        for (const auto& object : render_obj_list)
        {
            if (!object->is_visible()) {
                continue;
            }

            if (predicate && !predicate(*object, predicate_userdata)) {
                continue;
            }

            const auto gpu_rsrc = gpu_rsrc_mgr->get_lineset_resource(object);
            if (!gpu_rsrc) {
                continue;
            }

            const auto& line_indices = object->line_indices;

            // update model(transform) matrix in shader
            draw_shader.set_uniform_mat4("u_model", object->get_model());

            // Update VAO (if needed)
            if (object->is_dirty()) {
                gpu_rsrc->update(object); 
                object->clear_dirty();
            }

            // Render lines
            GLCall(::glBindVertexArray(gpu_rsrc->vao));
            GLCall(::glDrawElementsBaseVertex(
                GL_LINES, 
                static_cast<GLsizei>(line_indices.size() * std::decay_t<decltype(line_indices)>::value_type::SizeAtCompileTime),
                GL_UNSIGNED_INT,
                nullptr/* const GLvoid* indices */,
                0/* GLint basevertex */
            ));
            //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
        } // for

    }

} // namespace
