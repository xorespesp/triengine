#include "triangle_mesh_renderer.hh"

#include <triengine/utility/debug_utils.hh>
#include <triengine/shaders/shader_version.h>

namespace triengine::renderer
{
    triangle_mesh_renderer::triangle_mesh_renderer()
    {}

    void triangle_mesh_renderer::enable_object_normal_rendering(bool enable)
    {
        _show_object_normals = enable;
    }

    void triangle_mesh_renderer::create_impl(core::gl_context& glctx, const core::shader_loader& shader_ldr, const core::shader_preprocessor& shader_prep)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        glctx.make_context_current();

        _glctx = &glctx;

        //
        // Context Settings
        //

        // Create shader program
        _vertmode_solid_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shader_ldr.load("vert_shaded_mesh_obj.vert").value()).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shader_ldr.load("vert_shaded_mesh_obj_opaque_pass.frag").value()).c_str() })
            .link();

        _vertmode_transparent_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shader_ldr.load("vert_shaded_mesh_obj.vert").value()).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shader_ldr.load("vert_shaded_mesh_obj_transparent_pass.frag").value()).c_str() })
            .link();

        _texmode_solid_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shader_ldr.load("tex_shaded_mesh_obj.vert").value()).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shader_ldr.load("tex_shaded_mesh_obj_opaque_pass.frag").value()).c_str() })
            .link();

        _texmode_transparent_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shader_ldr.load("tex_shaded_mesh_obj.vert").value()).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shader_ldr.load("tex_shaded_mesh_obj_transparent_pass.frag").value()).c_str() })
            .link();
        
        _normal_vis_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shader_ldr.load("obj_normal_vis.vert").value()).c_str() })
            .attach_geometry_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shader_ldr.load("obj_normal_vis.geom").value()).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shader_ldr.load("obj_normal_vis.frag").value()).c_str() })
            .link();
    }

    void triangle_mesh_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            _vertmode_solid_shader.destroy();
            _vertmode_transparent_shader.destroy();
            _texmode_solid_shader.destroy();
            _texmode_transparent_shader.destroy();
            _normal_vis_shader.destroy();

            _glctx = nullptr;
        }
    }

    void triangle_mesh_renderer::render_impl(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
        const pred_callback_type predicate,
        void* const predicate_userdata)
    {
        if (render_obj_list.empty()) {
            return;
        }

        // Enable depth testing
        //GLCall(::glEnable(GL_DEPTH_TEST));

        this->_render_vertex_shading_objects(render_ctx, render_obj_list, predicate, predicate_userdata);
        this->_render_texture_shading_objects(render_ctx, render_obj_list, predicate, predicate_userdata);

        if (_show_object_normals) { // for debugging
            this->_render_objects_normals(render_ctx, render_obj_list, predicate, predicate_userdata);
        }
    }

    void triangle_mesh_renderer::_render_vertex_shading_objects(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects,
        pred_callback_type const predicate,
        void* const predicate_userdata)
    {
        auto& draw_shader =
            (render_ctx.curr_render_pass == render_pass_type::wboit_transparent_rendering)
            ? _vertmode_transparent_shader
            : _vertmode_solid_shader;

        draw_shader.use();

        // Update view/projection matrices in shader
        draw_shader.set_uniform_mat4("u_view", render_ctx.view);
        draw_shader.set_uniform_mat4("u_proj", render_ctx.projection);

        // Update light options in shader
        render_ctx.light_opts->dir_light.apply_to_shader(draw_shader);
        render_ctx.light_opts->point_light.apply_to_shader(draw_shader);

        auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
        for (const auto& object : render_objects)
        {
            if (!object->is_visible() || object->get_shading_mode() != geometry::triangle_mesh_object::shading_mode::vertex) {
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

            const auto gpu_rsrc = gpu_rsrc_mgr->get_triangle_mesh_resource(object);
            if (!gpu_rsrc) {
                continue;
            }

            const auto& triangle_indices = object->triangle_indices;
            const auto* const material = object->get_vertex_shading_material();

            TRIENGINE_ASSERT(!triangle_indices.empty());
            TRIENGINE_ASSERT(material != nullptr && material->is_valid());

            // Update model(transform) matrix
            const mat4_f32& model = object->get_model();
            draw_shader.set_uniform_mat4("u_model", model);

            // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
            draw_shader.set_uniform_mat3("u_nmv", (render_ctx.view * model).inverse().transpose().topLeftCorner<3, 3>());

            // Update material
            draw_shader.set_uniform_float("u_material.ambient", material->ambient);
            draw_shader.set_uniform_float("u_material.diffuse", material->diffuse);
            draw_shader.set_uniform_float("u_material.specular", material->specular);
            draw_shader.set_uniform_float("u_material.shininess", static_cast<float>(material->shininess));

            // Update alpha material (WBOIT)
            if (render_ctx.curr_render_pass == render_pass_type::wboit_transparent_rendering) {
                draw_shader.set_uniform_float("u_material.alpha", material->alpha);
            }
            
            // Update VAO
            if (object->is_dirty()) {
                gpu_rsrc->update(object); 
                object->clear_dirty();
            }

            // Render triangles
            GLCall(::glBindVertexArray(gpu_rsrc->vao)); // Bind VAO
            GLCall(::glDrawElementsBaseVertex(
                GL_TRIANGLES,
                static_cast<GLsizei>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                GL_UNSIGNED_INT,
                nullptr/* const GLvoid* indices */,
                0/* GLint basevertex */
            ));
            //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
        } // for
    }

    void triangle_mesh_renderer::_render_texture_shading_objects(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects,
        pred_callback_type const predicate,
        void* const predicate_userdata)
    {
        auto& draw_shader = 
            (render_ctx.curr_render_pass == render_pass_type::wboit_transparent_rendering)
            ? _texmode_transparent_shader
            : _texmode_solid_shader;

        draw_shader.use();
        
        // Update view, projection matrices in shader
        draw_shader.set_uniform_mat4("u_view", render_ctx.view);
        draw_shader.set_uniform_mat4("u_proj", render_ctx.projection);

        // Update light options in shader
        render_ctx.light_opts->dir_light.apply_to_shader(draw_shader);
        render_ctx.light_opts->point_light.apply_to_shader(draw_shader);

        auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
        for (const auto& object : render_objects)
        {
            if (!object->is_visible() || object->get_shading_mode() != geometry::triangle_mesh_object::shading_mode::texture) {
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

            const auto gpu_rsrc = gpu_rsrc_mgr->get_triangle_mesh_resource(object);
            if (!gpu_rsrc) {
                continue;
            }

            const auto& triangle_indices = object->triangle_indices;
            const auto* const material = object->get_texture_shading_material();

            TRIENGINE_ASSERT(!triangle_indices.empty());
            TRIENGINE_ASSERT(material != nullptr && material->is_valid());

            // Update model(transform) matrix in shader
            const mat4_f32& model = object->get_model();
            draw_shader.set_uniform_mat4("u_model", model);

            // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
            draw_shader.set_uniform_mat3("u_nmv", (render_ctx.view * model).inverse().transpose().topLeftCorner<3, 3>());

            // Update material shininess
            draw_shader.set_uniform_float("u_material.shininess", static_cast<float>(material->shininess));

            // Update material diffuse map
            draw_shader.set_uniform_int("u_material.diffuse", 0);
            ::glActiveTexture(GL_TEXTURE0);
            ::glBindTexture(GL_TEXTURE_2D, material->diffuse_map.id());
            
            // Update material specular map
            draw_shader.set_uniform_int("u_material.specular", 1);
            ::glActiveTexture(GL_TEXTURE1);
            ::glBindTexture(GL_TEXTURE_2D, material->specular_map.id());

            // Update alpha material (WBOIT)
            if (render_ctx.curr_render_pass == render_pass_type::wboit_transparent_rendering) {
                draw_shader.set_uniform_float("u_material.alpha", material->alpha);
            }

            // Update VAO
            if (object->is_dirty()) {
                gpu_rsrc->update(object); 
                object->clear_dirty();
            }

            // Render triangles
            GLCall(::glBindVertexArray(gpu_rsrc->vao)); // Bind VAO
            GLCall(::glDrawElementsBaseVertex(
                GL_TRIANGLES,
                static_cast<GLsizei>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                GL_UNSIGNED_INT,
                nullptr/* const GLvoid* indices */,
                0/* GLint basevertex */
            ));
            //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
        } // for
    }

    void triangle_mesh_renderer::_render_objects_normals(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects,
        pred_callback_type const predicate,
        void* const predicate_userdata)
    {
        auto& draw_shader = _normal_vis_shader;
        draw_shader.use();

        // Update view, projection matrices
        draw_shader.set_uniform_mat4("u_view_proj", render_ctx.projection * render_ctx.view);

        auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
        for (const auto& object : render_objects)
        {
            if (!object->is_visible()) {
                continue;
            }

            if (predicate && !predicate(*object, predicate_userdata)) {
                continue;
            }

            const auto gpu_rsrc = gpu_rsrc_mgr->get_triangle_mesh_resource(object);
            if (!gpu_rsrc) {
                continue;
            }

            const auto& triangle_indices = object->triangle_indices;

            TRIENGINE_ASSERT(!triangle_indices.empty());

            // Update model(transform) matrix
            const mat4_f32& model = object->get_model();
            draw_shader.set_uniform_mat4("u_model", model);

            // Update normal matrix; `mat3(transpose(inverse(u_model)))`
            draw_shader.set_uniform_mat3("u_nm", model.inverse().transpose().topLeftCorner<3, 3>());

            // Update VAO (if needed)
            if (object->is_dirty()) {
                gpu_rsrc->update(object); 
                object->clear_dirty();
            }

            // Render triangle mesh
            GLCall(::glBindVertexArray(gpu_rsrc->vao));
            GLCall(::glDrawElementsBaseVertex(
                GL_TRIANGLES,
                static_cast<GLsizei>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                GL_UNSIGNED_INT,
                nullptr/* const GLvoid* indices */,
                0/* GLint basevertex */
            ));
            //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
        } // for
    }

} // namespace