#include "lineset_renderer.hh"
#include <triengine/renderer/camera_block.hh>
#include <triengine/renderer/lighting_block.hh>
#include <triengine/renderer/object_model_block.hh>

#include <triengine/utility/debug_utils.hh>

namespace triengine::renderer
{
    lineset_renderer::lineset_renderer()
    { }

    lineset_renderer::~lineset_renderer()
    {
        this->destroy_impl();
    }

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
        verify_camera_block(_shader);
        verify_lighting_block(_shader);
        verify_object_model_block(_shader);
        _object_models.emplace(kObjectModelBlockBindingPointIndex);
    }

    void lineset_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            _shader.destroy();
            _object_models.reset();
            _glctx = nullptr;
        }
    }

    void lineset_renderer::render_impl(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::lineset_object>>& render_obj_list,
        pred_callback_type<geometry::lineset_object> const predicate,
        void* const predicate_userdata)
    {
        if (render_ctx.curr_render_pass != render_pass_type::forward_opaque_pass) {
            return;
        }

        if (render_obj_list.empty()) {
            return;
        }

        auto& draw_shader = _shader;
        auto& object_models = *_object_models;
        draw_shader.use();

        // Enable depth testing
        //GLCall(::glEnable(GL_DEPTH_TEST));

        // Enable smooth line
        //GLCall(::glEnable(GL_LINE_SMOOTH));

        GLCall(::glLineWidth(1.0f));

        // Write a model matrix per draw and collect the draws
        object_models.clear();
        _draw_items.clear();

        auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
        for (const auto& object : render_obj_list)
        {
            if (!object->is_visible()) {
                continue;
            }

            if (predicate && !predicate(*object, predicate_userdata)) {
                continue;
            }

            const auto object_gpu_rsrc = gpu_rsrc_mgr->get_lineset_geometry_resource(object);
            if (!object_gpu_rsrc) {
                continue;
            }

            const auto& line_indices = object->line_indices;

            // Update VAO (if needed)
            if (object->is_dirty()) {
                object_gpu_rsrc->update(object); 
                object->clear_dirty();
            }

            const size_t model_index = object_models.push_back(object->get_model());
            _draw_items.push_back(draw_item{
                object_gpu_rsrc->vao,
                static_cast<int32_t>(line_indices.size() * std::decay_t<decltype(line_indices)>::value_type::SizeAtCompileTime),
                static_cast<uint32_t>(model_index),
            });
        } // for

        // Upload the model matrices written above, before the draws that read them
        object_models.upload();
        object_models.bind();

        // Render lines
        for (const draw_item& item : _draw_items)
        {
            GLCall(::glBindVertexArray(item.vao));
            // Draws like `glDrawElements`, and also hands `baseinstance` to the shaders as `gl_BaseInstance`.
            // With one instance, that number is just the index of this object's model in `u_objectModels`.
            GLCall(::glDrawElementsInstancedBaseVertexBaseInstance(
                GL_LINES,
                item.index_count,
                GL_UNSIGNED_INT,
                nullptr/* const GLvoid* indices */,
                1/* GLsizei instancecount */,
                0/* GLint basevertex */,
                item.model_index/* GLuint baseinstance */
            ));
            //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
        } // for

    }

} // namespace
