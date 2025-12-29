#include "mesh_renderer.hh"

#include <triengine/utility/debug_utils.hh>

namespace triengine::renderer
{
    mesh_renderer::mesh_renderer()
    {}

    void mesh_renderer::set_render_mode(render_mode_type new_mode)
    {
        _curr_render_mode = new_mode;
    }

    void mesh_renderer::create_impl(core::gl_context& glctx)
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
        _vtxshaded_deferred_opaque_pass_shader
            .attach_vertex_shader({ shader_ldr->load("vtxshaded_mesh_deferred_geom_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("vtxshaded_mesh_deferred_geom_pass.frag")->c_str() })
            .link();

        _vtxshaded_forward_trans_pass_shader
            .attach_vertex_shader({ shader_ldr->load("vtxshaded_mesh_forward_trans_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("vtxshaded_mesh_forward_trans_pass.frag")->c_str() })
            .link();

        _vtxshaded_forward_opaque_pass_shader
            .attach_vertex_shader({ shader_ldr->load("vtxshaded_mesh_forward_opaque_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("vtxshaded_mesh_forward_opaque_pass.frag")->c_str() })
            .link();

        _texshaded_deferred_opaque_pass_shader
            .attach_vertex_shader({ shader_ldr->load("texshaded_mesh_deferred_geom_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("texshaded_mesh_deferred_geom_pass.frag")->c_str() })
            .link();

        _texshaded_forward_opaque_pass_shader
            .attach_vertex_shader({ shader_ldr->load("texshaded_mesh_forward_opaque_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("texshaded_mesh_forward_opaque_pass.frag")->c_str() })
            .link();
        
        _texshaded_forward_trans_pass_shader
            .attach_vertex_shader({ shader_ldr->load("texshaded_mesh_forward_trans_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("texshaded_mesh_forward_trans_pass.frag")->c_str() })
            .link();
        
        _normal_vis_shader
            .attach_vertex_shader({ shader_ldr->load("obj_normal_vis.vert")->c_str() })
            .attach_geometry_shader({ shader_ldr->load("obj_normal_vis.geom")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("obj_normal_vis.frag")->c_str() })
            .link();
    }

    void mesh_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            _vtxshaded_deferred_opaque_pass_shader.destroy();
            _vtxshaded_forward_opaque_pass_shader.destroy();
            _vtxshaded_forward_trans_pass_shader.destroy();

            _texshaded_deferred_opaque_pass_shader.destroy();
            _texshaded_forward_opaque_pass_shader.destroy();
            _texshaded_forward_trans_pass_shader.destroy();

            _normal_vis_shader.destroy();

            _glctx = nullptr;
        }
    }

    void mesh_renderer::render_impl(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::mesh_object>>& render_obj_list,
        const pred_callback_type<geometry::mesh_object> predicate,
        void* const predicate_userdata)
    {
        if (render_obj_list.empty()) {
            return;
        }

        // Enable depth testing
        //GLCall(::glEnable(GL_DEPTH_TEST));

        switch (_curr_render_mode) {
        case render_mode_type::shaded_surfaces:
            this->_render_vertex_shaded_objects(render_ctx, render_obj_list, predicate, predicate_userdata);
            this->_render_texture_shaded_objects(render_ctx, render_obj_list, predicate, predicate_userdata);
            break;
        case render_mode_type::vertex_normals:
            this->_render_objects_normals(render_ctx, render_obj_list, predicate, predicate_userdata);
            break;
        default:
            TRIENGINE_PANIC("%s(): Unsupported render mode type: %d"
                , __func__
                , static_cast<int>(_curr_render_mode)
            );
        }
    }

    void mesh_renderer::_render_vertex_shaded_objects(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
        pred_callback_type<geometry::mesh_object> const predicate,
        void* const predicate_userdata)
    {
        switch (render_ctx.curr_render_pass) {
        case render_pass_type::deferred_opaque_pass:
        {
            auto* const draw_shader = &_vtxshaded_deferred_opaque_pass_shader;

            draw_shader->use();

            // Update view/projection matrices in shader
            draw_shader->set_uniform_mat4("u_view", render_ctx.view);
            draw_shader->set_uniform_mat4("u_proj", render_ctx.projection);

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_objects)
            {
                if (!object->is_opaque()) {
                    continue;
                }

                if (!object->is_visible() || object->get_shading_mode() != geometry::mesh_object::shading_mode::vertex) {
                    continue;
                }

                if (predicate && !predicate(*object, predicate_userdata)) {
                    continue;
                }

                const auto object_gpu_rsrc = gpu_rsrc_mgr->get_mesh_geometry_resource(object);
                if (!object_gpu_rsrc) {
                    continue;
                }

                const auto& triangle_indices = object->triangle_indices;
                const auto* const material = object->get_vertex_shading_material();

                TRIENGINE_ASSERT(!triangle_indices.empty());
                TRIENGINE_ASSERT(material != nullptr && material->is_valid());

                // Update model(transform) matrix
                const mat4_f32& model = object->get_model();
                draw_shader->set_uniform_mat4("u_model", model);

                // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
                draw_shader->set_uniform_mat3("u_nmv", 
                    (render_ctx.view * model).inverse().transpose().topLeftCorner<3, 3>());

                // Update materials
                draw_shader->set_uniform_float("u_phongMaterial.ambientIntensity", material->ambient_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.diffuseIntensity", material->diffuse_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.specularIntensity", material->specular_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.shininess", static_cast<float>(material->shininess));

                // Update VAO
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                // Render triangles
                GLCall(::glBindVertexArray(object_gpu_rsrc->vao)); // Bind VAO
                GLCall(::glDrawElementsBaseVertex(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                    GL_UNSIGNED_INT,
                    nullptr/* const GLvoid* indices */,
                    0/* GLint basevertex */
                ));
                //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
            } // for

            break;
        }
        case render_pass_type::forward_opaque_pass: // for overlay rendering (skeleton renderer)
        {
            auto* const draw_shader = &_vtxshaded_forward_opaque_pass_shader;

            draw_shader->use();

            // Update view/projection matrices in shader
            draw_shader->set_uniform_mat4("u_view", render_ctx.view);
            draw_shader->set_uniform_mat4("u_proj", render_ctx.projection);

            // Update light options in shader
            render_ctx.light_opts->dir_light.apply_to_shader(*draw_shader, render_ctx.view);
            render_ctx.light_opts->point_light.apply_to_shader(*draw_shader, render_ctx.view);
            render_ctx.light_opts->simple_fog.apply_to_shader(*draw_shader, render_ctx.simple_fog_color);

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_objects)
            {
                if (!object->is_opaque()) {
                    continue;
                }

                if (!object->is_visible() || object->get_shading_mode() != geometry::mesh_object::shading_mode::vertex) {
                    continue;
                }

                if (predicate && !predicate(*object, predicate_userdata)) {
                    continue;
                }

                const auto object_gpu_rsrc = gpu_rsrc_mgr->get_mesh_geometry_resource(object);
                if (!object_gpu_rsrc) {
                    continue;
                }

                const auto& triangle_indices = object->triangle_indices;
                const auto* const material = object->get_vertex_shading_material();

                TRIENGINE_ASSERT(!triangle_indices.empty());
                TRIENGINE_ASSERT(material != nullptr && material->is_valid());

                // Update model(transform) matrix
                const mat4_f32& model = object->get_model();
                draw_shader->set_uniform_mat4("u_model", model);

                // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
                draw_shader->set_uniform_mat3("u_nmv", (render_ctx.view * model).inverse().transpose().topLeftCorner<3, 3>());

                // Update materials
                draw_shader->set_uniform_float("u_phongMaterial.ambientIntensity", material->ambient_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.diffuseIntensity", material->diffuse_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.specularIntensity", material->specular_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.shininess", static_cast<float>(material->shininess));

                // Update VAO
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                // Render triangles
                GLCall(::glBindVertexArray(object_gpu_rsrc->vao)); // Bind VAO
                GLCall(::glDrawElementsBaseVertex(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                    GL_UNSIGNED_INT,
                    nullptr/* const GLvoid* indices */,
                    0/* GLint basevertex */
                ));
                //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
            } // for

            break;
        }
        case render_pass_type::forward_transparent_pass:
        {
            auto* const draw_shader = &_vtxshaded_forward_trans_pass_shader;

            draw_shader->use();

            // Update view/projection matrices in shader
            draw_shader->set_uniform_mat4("u_view", render_ctx.view);
            draw_shader->set_uniform_mat4("u_proj", render_ctx.projection);

            // Update light options in shader
            render_ctx.light_opts->dir_light.apply_to_shader(*draw_shader, render_ctx.view);
            render_ctx.light_opts->point_light.apply_to_shader(*draw_shader, render_ctx.view);
            render_ctx.light_opts->simple_fog.apply_to_shader(*draw_shader, render_ctx.simple_fog_color);

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_objects)
            {
                if (object->is_opaque()) {
                    continue;
                }

                if (!object->is_visible() || object->get_shading_mode() != geometry::mesh_object::shading_mode::vertex) {
                    continue;
                }

                if (predicate && !predicate(*object, predicate_userdata)) {
                    continue;
                }

                const auto object_gpu_rsrc = gpu_rsrc_mgr->get_mesh_geometry_resource(object);
                if (!object_gpu_rsrc) {
                    continue;
                }

                const auto& triangle_indices = object->triangle_indices;
                const auto* const material = object->get_vertex_shading_material();

                TRIENGINE_ASSERT(!triangle_indices.empty());
                TRIENGINE_ASSERT(material != nullptr && material->is_valid());

                // Update model(transform) matrix
                const mat4_f32& model = object->get_model();
                draw_shader->set_uniform_mat4("u_model", model);

                // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
                draw_shader->set_uniform_mat3("u_nmv", (render_ctx.view * model).inverse().transpose().topLeftCorner<3, 3>());

                // Update materials
                draw_shader->set_uniform_float("u_phongMaterial.ambientIntensity", material->ambient_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.diffuseIntensity", material->diffuse_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.specularIntensity", material->specular_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.shininess", static_cast<float>(material->shininess));

                // Update alpha material (WBOIT)
                draw_shader->set_uniform_float("u_alpha", material->alpha);

                // Update VAO
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                // Render triangles
                GLCall(::glBindVertexArray(object_gpu_rsrc->vao)); // Bind VAO
                GLCall(::glDrawElementsBaseVertex(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                    GL_UNSIGNED_INT,
                    nullptr/* const GLvoid* indices */,
                    0/* GLint basevertex */
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

    void mesh_renderer::_render_texture_shaded_objects(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
        pred_callback_type<geometry::mesh_object> const predicate,
        void* const predicate_userdata)
    {
        switch (render_ctx.curr_render_pass) {
        case render_pass_type::deferred_opaque_pass:
        {
            auto* const draw_shader = &_texshaded_deferred_opaque_pass_shader;

            draw_shader->use();

            // Update view, projection matrices in shader
            draw_shader->set_uniform_mat4("u_view", render_ctx.view);
            draw_shader->set_uniform_mat4("u_proj", render_ctx.projection);

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_objects)
            {
                if (!object->is_opaque()) {
                    continue;
                }

                if (!object->is_visible() || object->get_shading_mode() != geometry::mesh_object::shading_mode::texture) {
                    continue;
                }

                if (predicate && !predicate(*object, predicate_userdata)) {
                    continue;
                }

                const auto object_gpu_rsrc = gpu_rsrc_mgr->get_mesh_geometry_resource(object);
                if (!object_gpu_rsrc) {
                    continue;
                }

                const auto& triangle_indices = object->triangle_indices;
                const auto* const material = object->get_texture_shading_material();

                TRIENGINE_ASSERT(!triangle_indices.empty());
                TRIENGINE_ASSERT(material != nullptr && material->is_valid());

                // Update model(transform) matrix in shader
                const mat4_f32& model = object->get_model();
                draw_shader->set_uniform_mat4("u_model", model);

                // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
                draw_shader->set_uniform_mat3("u_nmv", 
                    (render_ctx.view * model).inverse().transpose().topLeftCorner<3, 3>());

                // Update materials
                draw_shader->set_uniform_float("u_phongMaterial.ambientIntensity", material->ambient_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.diffuseIntensity", material->diffuse_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.specularIntensity", material->specular_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.shininess", static_cast<float>(material->shininess));

                // Update material diffuse, specular color map
                auto diffuse_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->diffuse_map);
                TRIENGINE_ASSERT(diffuse_tex2d_gpu_rsrc != nullptr && diffuse_tex2d_gpu_rsrc->is_valid());

                auto specular_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->specular_map);
                TRIENGINE_ASSERT(specular_tex2d_gpu_rsrc != nullptr && specular_tex2d_gpu_rsrc->is_valid());

                ::glBindTextureUnit(0, diffuse_tex2d_gpu_rsrc->id());
                ::glBindTextureUnit(1, specular_tex2d_gpu_rsrc->id());

                // Update VAO
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                // Render triangles
                GLCall(::glBindVertexArray(object_gpu_rsrc->vao)); // Bind VAO
                GLCall(::glDrawElementsBaseVertex(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                    GL_UNSIGNED_INT,
                    nullptr/* const GLvoid* indices */,
                    0/* GLint basevertex */
                ));
                //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
            } // for

            break;
        }
        case render_pass_type::forward_opaque_pass: // for overlay rendering (skeleton renderer)
        {
            auto* const draw_shader = &_texshaded_forward_opaque_pass_shader;

            draw_shader->use();

            // Update view, projection matrices in shader
            draw_shader->set_uniform_mat4("u_view", render_ctx.view);
            draw_shader->set_uniform_mat4("u_proj", render_ctx.projection);

            // Update light options in shader
            render_ctx.light_opts->dir_light.apply_to_shader(*draw_shader, render_ctx.view);
            render_ctx.light_opts->point_light.apply_to_shader(*draw_shader, render_ctx.view);
            render_ctx.light_opts->simple_fog.apply_to_shader(*draw_shader, render_ctx.simple_fog_color);

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_objects)
            {
                if (!object->is_opaque()) {
                    continue;
                }

                if (!object->is_visible() || object->get_shading_mode() != geometry::mesh_object::shading_mode::texture) {
                    continue;
                }

                if (predicate && !predicate(*object, predicate_userdata)) {
                    continue;
                }

                const auto object_gpu_rsrc = gpu_rsrc_mgr->get_mesh_geometry_resource(object);
                if (!object_gpu_rsrc) {
                    continue;
                }

                const auto& triangle_indices = object->triangle_indices;
                const auto* const material = object->get_texture_shading_material();

                TRIENGINE_ASSERT(!triangle_indices.empty());
                TRIENGINE_ASSERT(material != nullptr && material->is_valid());

                // Update model(transform) matrix in shader
                const mat4_f32& model = object->get_model();
                draw_shader->set_uniform_mat4("u_model", model);

                // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
                draw_shader->set_uniform_mat3("u_nmv",
                    (render_ctx.view * model).inverse().transpose().topLeftCorner<3, 3>());

                // Update materials
                draw_shader->set_uniform_float("u_phongMaterial.ambientIntensity", material->ambient_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.diffuseIntensity", material->diffuse_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.specularIntensity", material->specular_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.shininess", static_cast<float>(material->shininess));

                // Update material diffuse, specular color map
                auto diffuse_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->diffuse_map);
                TRIENGINE_ASSERT(diffuse_tex2d_gpu_rsrc != nullptr && diffuse_tex2d_gpu_rsrc->is_valid());

                auto specular_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->specular_map);
                TRIENGINE_ASSERT(specular_tex2d_gpu_rsrc != nullptr && specular_tex2d_gpu_rsrc->is_valid());

                ::glBindTextureUnit(0, diffuse_tex2d_gpu_rsrc->id());
                ::glBindTextureUnit(1, specular_tex2d_gpu_rsrc->id());

                // Update VAO
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                // Render triangles
                GLCall(::glBindVertexArray(object_gpu_rsrc->vao)); // Bind VAO
                GLCall(::glDrawElementsBaseVertex(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                    GL_UNSIGNED_INT,
                    nullptr/* const GLvoid* indices */,
                    0/* GLint basevertex */
                ));
                //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
            } // for

            break;
        }
        case render_pass_type::forward_transparent_pass:
        {
            auto* const draw_shader = &_texshaded_forward_trans_pass_shader;

            draw_shader->use();

            // Update view, projection matrices in shader
            draw_shader->set_uniform_mat4("u_view", render_ctx.view);
            draw_shader->set_uniform_mat4("u_proj", render_ctx.projection);

            // Update light options in shader
            render_ctx.light_opts->dir_light.apply_to_shader(*draw_shader, render_ctx.view);
            render_ctx.light_opts->point_light.apply_to_shader(*draw_shader, render_ctx.view);
            render_ctx.light_opts->simple_fog.apply_to_shader(*draw_shader, render_ctx.simple_fog_color);

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_objects)
            {
                if (object->is_opaque()) { 
                    continue; 
                }

                if (!object->is_visible() || object->get_shading_mode() != geometry::mesh_object::shading_mode::texture) {
                    continue;
                }

                if (predicate && !predicate(*object, predicate_userdata)) {
                    continue;
                }

                const auto object_gpu_rsrc = gpu_rsrc_mgr->get_mesh_geometry_resource(object);
                if (!object_gpu_rsrc) {
                    continue;
                }

                const auto& triangle_indices = object->triangle_indices;
                const auto* const material = object->get_texture_shading_material();

                TRIENGINE_ASSERT(!triangle_indices.empty());
                TRIENGINE_ASSERT(material != nullptr && material->is_valid());

                // Update model(transform) matrix in shader
                const mat4_f32& model = object->get_model();
                draw_shader->set_uniform_mat4("u_model", model);

                // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
                draw_shader->set_uniform_mat3("u_nmv", 
                    (render_ctx.view * model).inverse().transpose().topLeftCorner<3, 3>());

                // Update materials
                draw_shader->set_uniform_float("u_phongMaterial.ambientIntensity", material->ambient_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.diffuseIntensity", material->diffuse_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.specularIntensity", material->specular_intensity);
                draw_shader->set_uniform_float("u_phongMaterial.shininess", static_cast<float>(material->shininess));

                // Update material diffuse, specular color map
                auto diffuse_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->diffuse_map);
                TRIENGINE_ASSERT(diffuse_tex2d_gpu_rsrc != nullptr && diffuse_tex2d_gpu_rsrc->is_valid());

                auto specular_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->specular_map);
                TRIENGINE_ASSERT(specular_tex2d_gpu_rsrc != nullptr && specular_tex2d_gpu_rsrc->is_valid());

                ::glBindTextureUnit(0, diffuse_tex2d_gpu_rsrc->id());
                ::glBindTextureUnit(1, specular_tex2d_gpu_rsrc->id());

                // Update alpha material (WBOIT)
                draw_shader->set_uniform_float("u_alpha", material->alpha);

                // Update VAO
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                // Render triangles
                GLCall(::glBindVertexArray(object_gpu_rsrc->vao)); // Bind VAO
                GLCall(::glDrawElementsBaseVertex(
                    GL_TRIANGLES,
                    static_cast<GLsizei>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                    GL_UNSIGNED_INT,
                    nullptr/* const GLvoid* indices */,
                    0/* GLint basevertex */
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

    void mesh_renderer::_render_objects_normals(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
        pred_callback_type<geometry::mesh_object> const predicate,
        void* const predicate_userdata)
    {
        if (render_ctx.curr_render_pass != render_pass_type::forward_opaque_pass) {
            TRIENGINE_PANIC("%s(): Unsupported render pass type: %d"
                , __func__
                , static_cast<int>(render_ctx.curr_render_pass)
            );
        }

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

            const auto object_gpu_rsrc = gpu_rsrc_mgr->get_mesh_geometry_resource(object);
            if (!object_gpu_rsrc) {
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
                object_gpu_rsrc->update(object); 
                object->clear_dirty();
            }

            // Render triangle mesh
            GLCall(::glBindVertexArray(object_gpu_rsrc->vao));
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