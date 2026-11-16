#include "mesh_renderer.hh"
#include <triengine/renderer/camera_block.hh>
#include <triengine/renderer/lighting_block.hh>

#include <triengine/utility/debug_utils.hh>

namespace triengine::renderer
{
    mesh_renderer::mesh_renderer()
    {}

    mesh_renderer::~mesh_renderer()
    {
        this->destroy_impl();
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
        _lit_vtxshaded_deferred_opaque_pass_shader
            .attach_vertex_shader({ shader_ldr->load("lit_vtxshaded_mesh_deferred_geom_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("lit_vtxshaded_mesh_deferred_geom_pass.frag")->c_str() })
            .link();
        verify_camera_block(_lit_vtxshaded_deferred_opaque_pass_shader);
        verify_phong_shaded_object_block(_lit_vtxshaded_deferred_opaque_pass_shader);
        _lit_vtxshaded_deferred_opaque_pass_records.emplace(kPhongShadedObjectBlockBindingPointIndex);

        _lit_vtxshaded_forward_trans_pass_shader
            .attach_vertex_shader({ shader_ldr->load("lit_vtxshaded_mesh_forward_trans_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("lit_vtxshaded_mesh_forward_trans_pass.frag")->c_str() })
            .link();
        verify_camera_block(_lit_vtxshaded_forward_trans_pass_shader);
        verify_lighting_block(_lit_vtxshaded_forward_trans_pass_shader);
        verify_phong_shaded_object_block(_lit_vtxshaded_forward_trans_pass_shader);
        _lit_vtxshaded_forward_trans_pass_records.emplace(kPhongShadedObjectBlockBindingPointIndex);

        _lit_vtxshaded_forward_opaque_pass_shader
            .attach_vertex_shader({ shader_ldr->load("lit_vtxshaded_mesh_forward_opaque_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("lit_vtxshaded_mesh_forward_opaque_pass.frag")->c_str() })
            .link();
        verify_camera_block(_lit_vtxshaded_forward_opaque_pass_shader);
        verify_lighting_block(_lit_vtxshaded_forward_opaque_pass_shader);
        verify_phong_shaded_object_block(_lit_vtxshaded_forward_opaque_pass_shader);
        _lit_vtxshaded_forward_opaque_pass_records.emplace(kPhongShadedObjectBlockBindingPointIndex);

        _lit_texshaded_deferred_opaque_pass_shader
            .attach_vertex_shader({ shader_ldr->load("lit_texshaded_mesh_deferred_geom_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("lit_texshaded_mesh_deferred_geom_pass.frag")->c_str() })
            .link();
        verify_camera_block(_lit_texshaded_deferred_opaque_pass_shader);
        verify_phong_shaded_object_block(_lit_texshaded_deferred_opaque_pass_shader);
        _lit_texshaded_deferred_opaque_pass_records.emplace(kPhongShadedObjectBlockBindingPointIndex);

        _lit_texshaded_forward_opaque_pass_shader
            .attach_vertex_shader({ shader_ldr->load("lit_texshaded_mesh_forward_opaque_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("lit_texshaded_mesh_forward_opaque_pass.frag")->c_str() })
            .link();
        verify_camera_block(_lit_texshaded_forward_opaque_pass_shader);
        verify_lighting_block(_lit_texshaded_forward_opaque_pass_shader);
        verify_phong_shaded_object_block(_lit_texshaded_forward_opaque_pass_shader);
        _lit_texshaded_forward_opaque_pass_records.emplace(kPhongShadedObjectBlockBindingPointIndex);
        
        _lit_texshaded_forward_trans_pass_shader
            .attach_vertex_shader({ shader_ldr->load("lit_texshaded_mesh_forward_trans_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("lit_texshaded_mesh_forward_trans_pass.frag")->c_str() })
            .link();
        verify_camera_block(_lit_texshaded_forward_trans_pass_shader);
        verify_lighting_block(_lit_texshaded_forward_trans_pass_shader);
        verify_phong_shaded_object_block(_lit_texshaded_forward_trans_pass_shader);
        _lit_texshaded_forward_trans_pass_records.emplace(kPhongShadedObjectBlockBindingPointIndex);

        _unlit_vtxshaded_forward_opaque_pass_shader
            .attach_vertex_shader({ shader_ldr->load("unlit_vtxshaded_mesh_forward_opaque_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("unlit_vtxshaded_mesh_forward_opaque_pass.frag")->c_str() })
            .link();
        verify_camera_block(_unlit_vtxshaded_forward_opaque_pass_shader);
        verify_phong_shaded_object_block(_unlit_vtxshaded_forward_opaque_pass_shader);
        _unlit_vtxshaded_forward_opaque_pass_records.emplace(kPhongShadedObjectBlockBindingPointIndex);

        _unlit_vtxshaded_forward_trans_pass_shader
            .attach_vertex_shader({ shader_ldr->load("unlit_vtxshaded_mesh_forward_trans_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("unlit_vtxshaded_mesh_forward_trans_pass.frag")->c_str() })
            .link();
        verify_camera_block(_unlit_vtxshaded_forward_trans_pass_shader);
        verify_phong_shaded_object_block(_unlit_vtxshaded_forward_trans_pass_shader);
        _unlit_vtxshaded_forward_trans_pass_records.emplace(kPhongShadedObjectBlockBindingPointIndex);

        _unlit_texshaded_forward_opaque_pass_shader
            .attach_vertex_shader({ shader_ldr->load("unlit_texshaded_mesh_forward_opaque_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("unlit_texshaded_mesh_forward_opaque_pass.frag")->c_str() })
            .link();
        verify_camera_block(_unlit_texshaded_forward_opaque_pass_shader);
        verify_phong_shaded_object_block(_unlit_texshaded_forward_opaque_pass_shader);
        _unlit_texshaded_forward_opaque_pass_records.emplace(kPhongShadedObjectBlockBindingPointIndex);

        _unlit_texshaded_forward_trans_pass_shader
            .attach_vertex_shader({ shader_ldr->load("unlit_texshaded_mesh_forward_trans_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("unlit_texshaded_mesh_forward_trans_pass.frag")->c_str() })
            .link();
        verify_camera_block(_unlit_texshaded_forward_trans_pass_shader);
        verify_phong_shaded_object_block(_unlit_texshaded_forward_trans_pass_shader);
        _unlit_texshaded_forward_trans_pass_records.emplace(kPhongShadedObjectBlockBindingPointIndex);

        _normal_vis_shader
            .attach_vertex_shader({ shader_ldr->load("obj_normal_vis.vert")->c_str() })
            .attach_geometry_shader({ shader_ldr->load("obj_normal_vis.geom")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("obj_normal_vis.frag")->c_str() })
            .link();
        verify_camera_block(_normal_vis_shader);
        verify_phong_shaded_object_block(_normal_vis_shader);
        _normal_vis_records.emplace(kPhongShadedObjectBlockBindingPointIndex);
    }

    void mesh_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            _lit_vtxshaded_deferred_opaque_pass_shader.destroy();
            _lit_vtxshaded_deferred_opaque_pass_records.reset();
            _lit_vtxshaded_forward_opaque_pass_shader.destroy();
            _lit_vtxshaded_forward_opaque_pass_records.reset();
            _lit_vtxshaded_forward_trans_pass_shader.destroy();
            _lit_vtxshaded_forward_trans_pass_records.reset();

            _lit_texshaded_deferred_opaque_pass_shader.destroy();
            _lit_texshaded_deferred_opaque_pass_records.reset();
            _lit_texshaded_forward_opaque_pass_shader.destroy();
            _lit_texshaded_forward_opaque_pass_records.reset();
            _lit_texshaded_forward_trans_pass_shader.destroy();
            _lit_texshaded_forward_trans_pass_records.reset();

            _unlit_vtxshaded_forward_opaque_pass_shader.destroy();
            _unlit_vtxshaded_forward_opaque_pass_records.reset();
            _unlit_vtxshaded_forward_trans_pass_shader.destroy();
            _unlit_vtxshaded_forward_trans_pass_records.reset();
            _unlit_texshaded_forward_opaque_pass_shader.destroy();
            _unlit_texshaded_forward_opaque_pass_records.reset();
            _unlit_texshaded_forward_trans_pass_shader.destroy();
            _unlit_texshaded_forward_trans_pass_records.reset();

            _normal_vis_shader.destroy();
            _normal_vis_records.reset();

            _glctx = nullptr;
        }
    }

    void mesh_renderer::render_impl(
        const render_context& render_ctx,
        const render_mode_type render_mode,
        const std::list<std::shared_ptr<geometry::mesh_object>>& render_obj_list,
        const pred_callback_type<geometry::mesh_object> predicate,
        void* const predicate_userdata)
    {
        if (render_obj_list.empty()) {
            return;
        }

        // Enable depth testing
        //GLCall(::glEnable(GL_DEPTH_TEST));

        switch (render_mode) {
        case render_mode_type::lit_shaded_surfaces:
            this->_render_lit_vertex_shaded_objects(render_ctx, render_obj_list, predicate, predicate_userdata);
            this->_render_lit_texture_shaded_objects(render_ctx, render_obj_list, predicate, predicate_userdata);
            break;
        case render_mode_type::unlit_shaded_surfaces:
            this->_render_unlit_vertex_shaded_objects(render_ctx, render_obj_list, predicate, predicate_userdata);
            this->_render_unlit_texture_shaded_objects(render_ctx, render_obj_list, predicate, predicate_userdata);
            break;
        case render_mode_type::vertex_normals:
            this->_render_objects_normals(render_ctx, render_obj_list, predicate, predicate_userdata);
            break;
        default:
            TRIENGINE_PANIC("%s(): Unsupported render mode type: %d"
                , __func__
                , static_cast<int>(render_mode)
            );
        }
    }

    void mesh_renderer::_render_lit_vertex_shaded_objects(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
        pred_callback_type<geometry::mesh_object> const predicate,
        void* const predicate_userdata)
    {
        switch (render_ctx.curr_render_pass) {
        case render_pass_type::deferred_opaque_pass:
        {
            auto* const draw_shader = &_lit_vtxshaded_deferred_opaque_pass_shader;
            auto& object_records = *_lit_vtxshaded_deferred_opaque_pass_records;

            draw_shader->use();

            // Write a record per draw and collect the draws
            object_records.clear();
            _draw_items.clear();

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_objects)
            {
                if (!object->is_opaque()) {
                    continue;
                }

                if (!object->is_visible() ||
                    object->get_lighting_mode() != geometry::mesh_object::lighting_mode::lit ||
                    object->get_shading_mode() != geometry::mesh_object::shading_mode::vertex) {
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

                // Update VAO
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                const size_t record_index = object_records.push_back(
                    make_phong_shaded_object_record(object->get_model(), *material)
                );
                _draw_items.push_back(draw_item{
                    object_gpu_rsrc->vao,
                    static_cast<int32_t>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                    static_cast<uint32_t>(record_index),
                });
            } // for

            // Upload the records written above, before the draws that read them
            object_records.upload();
            object_records.bind();

            // Render triangles
            for (const draw_item& item : _draw_items)
            {
                GLCall(::glBindVertexArray(item.vao)); // Bind VAO
                // Draws like `glDrawElements`, and also hands `baseinstance` to the shaders as `gl_BaseInstance`.
                // With one instance, that number is just the index of this object's record in `u_phongShadedObjects`.
                GLCall(::glDrawElementsInstancedBaseVertexBaseInstance(
                    GL_TRIANGLES,
                    item.index_count,
                    GL_UNSIGNED_INT,
                    nullptr/* const GLvoid* indices */,
                    1/* GLsizei instancecount */,
                    0/* GLint basevertex */,
                    item.record_index/* GLuint baseinstance */
                ));
                //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
            } // for

            break;
        }
        case render_pass_type::forward_opaque_pass: // for overlay rendering (skeleton renderer)
        {
            auto* const draw_shader = &_lit_vtxshaded_forward_opaque_pass_shader;
            auto& object_records = *_lit_vtxshaded_forward_opaque_pass_records;

            draw_shader->use();

            // Write a record per draw and collect the draws
            object_records.clear();
            _draw_items.clear();

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_objects)
            {
                if (!object->is_opaque()) {
                    continue;
                }

                if (!object->is_visible() ||
                    object->get_lighting_mode() != geometry::mesh_object::lighting_mode::lit ||
                    object->get_shading_mode() != geometry::mesh_object::shading_mode::vertex) {
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

                // Update VAO
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                const size_t record_index = object_records.push_back(
                    make_phong_shaded_object_record(object->get_model(), *material)
                );
                _draw_items.push_back(draw_item{
                    object_gpu_rsrc->vao,
                    static_cast<int32_t>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                    static_cast<uint32_t>(record_index),
                });
            } // for

            // Upload the records written above, before the draws that read them
            object_records.upload();
            object_records.bind();

            // Render triangles
            for (const draw_item& item : _draw_items)
            {
                GLCall(::glBindVertexArray(item.vao)); // Bind VAO
                // Draws like `glDrawElements`, and also hands `baseinstance` to the shaders as `gl_BaseInstance`.
                // With one instance, that number is just the index of this object's record in `u_phongShadedObjects`.
                GLCall(::glDrawElementsInstancedBaseVertexBaseInstance(
                    GL_TRIANGLES,
                    item.index_count,
                    GL_UNSIGNED_INT,
                    nullptr/* const GLvoid* indices */,
                    1/* GLsizei instancecount */,
                    0/* GLint basevertex */,
                    item.record_index/* GLuint baseinstance */
                ));
                //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
            } // for

            break;
        }
        case render_pass_type::forward_transparent_pass:
        {
            auto* const draw_shader = &_lit_vtxshaded_forward_trans_pass_shader;
            auto& object_records = *_lit_vtxshaded_forward_trans_pass_records;

            draw_shader->use();

            // Write a record per draw and collect the draws
            object_records.clear();
            _draw_items.clear();

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_objects)
            {
                if (object->is_opaque()) {
                    continue;
                }

                if (!object->is_visible() ||
                    object->get_lighting_mode() != geometry::mesh_object::lighting_mode::lit ||
                    object->get_shading_mode() != geometry::mesh_object::shading_mode::vertex) {
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

                // Update VAO
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                const size_t record_index = object_records.push_back(
                    make_phong_shaded_object_record(object->get_model(), *material)
                );
                _draw_items.push_back(draw_item{
                    object_gpu_rsrc->vao,
                    static_cast<int32_t>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                    static_cast<uint32_t>(record_index),
                });
            } // for

            // Upload the records written above, before the draws that read them
            object_records.upload();
            object_records.bind();

            // Render triangles
            for (const draw_item& item : _draw_items)
            {
                GLCall(::glBindVertexArray(item.vao)); // Bind VAO
                // Draws like `glDrawElements`, and also hands `baseinstance` to the shaders as `gl_BaseInstance`.
                // With one instance, that number is just the index of this object's record in `u_phongShadedObjects`.
                GLCall(::glDrawElementsInstancedBaseVertexBaseInstance(
                    GL_TRIANGLES,
                    item.index_count,
                    GL_UNSIGNED_INT,
                    nullptr/* const GLvoid* indices */,
                    1/* GLsizei instancecount */,
                    0/* GLint basevertex */,
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

    void mesh_renderer::_render_lit_texture_shaded_objects(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
        pred_callback_type<geometry::mesh_object> const predicate,
        void* const predicate_userdata)
    {
        switch (render_ctx.curr_render_pass) {
        case render_pass_type::deferred_opaque_pass:
        {
            auto* const draw_shader = &_lit_texshaded_deferred_opaque_pass_shader;
            auto& object_records = *_lit_texshaded_deferred_opaque_pass_records;

            draw_shader->use();

            // Write a record per draw and collect the draws
            object_records.clear();
            _lit_texshaded_draw_items.clear();

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_objects)
            {
                if (!object->is_opaque()) {
                    continue;
                }

                if (!object->is_visible() ||
                    object->get_lighting_mode() != geometry::mesh_object::lighting_mode::lit ||
                    object->get_shading_mode() != geometry::mesh_object::shading_mode::texture) {
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

                // Diffuse, specular color maps; bound with the draw below
                auto diffuse_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->diffuse_map);
                TRIENGINE_ASSERT(diffuse_tex2d_gpu_rsrc != nullptr && diffuse_tex2d_gpu_rsrc->is_valid());

                auto specular_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->specular_map);
                TRIENGINE_ASSERT(specular_tex2d_gpu_rsrc != nullptr && specular_tex2d_gpu_rsrc->is_valid());

                // Update VAO
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                const size_t record_index = object_records.push_back(
                    make_phong_shaded_object_record(object->get_model(), *material)
                );
                _lit_texshaded_draw_items.push_back(lit_texshaded_draw_item{
                    {
                        object_gpu_rsrc->vao,
                        static_cast<int32_t>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                        static_cast<uint32_t>(record_index),
                    },
                    diffuse_tex2d_gpu_rsrc->id(),
                    specular_tex2d_gpu_rsrc->id(),
                });
            } // for

            // Upload the records written above, before the draws that read them
            object_records.upload();
            object_records.bind();

            // Render triangles
            for (const lit_texshaded_draw_item& item : _lit_texshaded_draw_items)
            {
                ::glBindTextureUnit(0, item.diffuse_map);
                ::glBindTextureUnit(1, item.specular_map);
                GLCall(::glBindVertexArray(item.vao)); // Bind VAO
                // Draws like `glDrawElements`, and also hands `baseinstance` to the shaders as `gl_BaseInstance`.
                // With one instance, that number is just the index of this object's record in `u_phongShadedObjects`.
                GLCall(::glDrawElementsInstancedBaseVertexBaseInstance(
                    GL_TRIANGLES,
                    item.index_count,
                    GL_UNSIGNED_INT,
                    nullptr/* const GLvoid* indices */,
                    1/* GLsizei instancecount */,
                    0/* GLint basevertex */,
                    item.record_index/* GLuint baseinstance */
                ));
                //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
            } // for

            break;
        }
        case render_pass_type::forward_opaque_pass: // for overlay rendering (skeleton renderer)
        {
            auto* const draw_shader = &_lit_texshaded_forward_opaque_pass_shader;
            auto& object_records = *_lit_texshaded_forward_opaque_pass_records;

            draw_shader->use();

            // Write a record per draw and collect the draws
            object_records.clear();
            _lit_texshaded_draw_items.clear();

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_objects)
            {
                if (!object->is_opaque()) {
                    continue;
                }

                if (!object->is_visible() ||
                    object->get_lighting_mode() != geometry::mesh_object::lighting_mode::lit ||
                    object->get_shading_mode() != geometry::mesh_object::shading_mode::texture) {
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

                // Diffuse, specular color maps; bound with the draw below
                auto diffuse_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->diffuse_map);
                TRIENGINE_ASSERT(diffuse_tex2d_gpu_rsrc != nullptr && diffuse_tex2d_gpu_rsrc->is_valid());

                auto specular_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->specular_map);
                TRIENGINE_ASSERT(specular_tex2d_gpu_rsrc != nullptr && specular_tex2d_gpu_rsrc->is_valid());

                // Update VAO
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                const size_t record_index = object_records.push_back(
                    make_phong_shaded_object_record(object->get_model(), *material)
                );
                _lit_texshaded_draw_items.push_back(lit_texshaded_draw_item{
                    {
                        object_gpu_rsrc->vao,
                        static_cast<int32_t>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                        static_cast<uint32_t>(record_index),
                    },
                    diffuse_tex2d_gpu_rsrc->id(),
                    specular_tex2d_gpu_rsrc->id(),
                });
            } // for

            // Upload the records written above, before the draws that read them
            object_records.upload();
            object_records.bind();

            // Render triangles
            for (const lit_texshaded_draw_item& item : _lit_texshaded_draw_items)
            {
                ::glBindTextureUnit(0, item.diffuse_map);
                ::glBindTextureUnit(1, item.specular_map);
                GLCall(::glBindVertexArray(item.vao)); // Bind VAO
                // Draws like `glDrawElements`, and also hands `baseinstance` to the shaders as `gl_BaseInstance`.
                // With one instance, that number is just the index of this object's record in `u_phongShadedObjects`.
                GLCall(::glDrawElementsInstancedBaseVertexBaseInstance(
                    GL_TRIANGLES,
                    item.index_count,
                    GL_UNSIGNED_INT,
                    nullptr/* const GLvoid* indices */,
                    1/* GLsizei instancecount */,
                    0/* GLint basevertex */,
                    item.record_index/* GLuint baseinstance */
                ));
                //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
            } // for

            break;
        }
        case render_pass_type::forward_transparent_pass:
        {
            auto* const draw_shader = &_lit_texshaded_forward_trans_pass_shader;
            auto& object_records = *_lit_texshaded_forward_trans_pass_records;

            draw_shader->use();

            // Write a record per draw and collect the draws
            object_records.clear();
            _lit_texshaded_draw_items.clear();

            auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
            for (const auto& object : render_objects)
            {
                if (object->is_opaque()) {
                    continue;
                }

                if (!object->is_visible() ||
                    object->get_lighting_mode() != geometry::mesh_object::lighting_mode::lit ||
                    object->get_shading_mode() != geometry::mesh_object::shading_mode::texture) {
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

                // Diffuse, specular color maps; bound with the draw below
                auto diffuse_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->diffuse_map);
                TRIENGINE_ASSERT(diffuse_tex2d_gpu_rsrc != nullptr && diffuse_tex2d_gpu_rsrc->is_valid());

                auto specular_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->specular_map);
                TRIENGINE_ASSERT(specular_tex2d_gpu_rsrc != nullptr && specular_tex2d_gpu_rsrc->is_valid());

                // Update VAO
                if (object->is_dirty()) {
                    object_gpu_rsrc->update(object);
                    object->clear_dirty();
                }

                const size_t record_index = object_records.push_back(
                    make_phong_shaded_object_record(object->get_model(), *material)
                );
                _lit_texshaded_draw_items.push_back(lit_texshaded_draw_item{
                    {
                        object_gpu_rsrc->vao,
                        static_cast<int32_t>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                        static_cast<uint32_t>(record_index),
                    },
                    diffuse_tex2d_gpu_rsrc->id(),
                    specular_tex2d_gpu_rsrc->id(),
                });
            } // for

            // Upload the records written above, before the draws that read them
            object_records.upload();
            object_records.bind();

            // Render triangles
            for (const lit_texshaded_draw_item& item : _lit_texshaded_draw_items)
            {
                ::glBindTextureUnit(0, item.diffuse_map);
                ::glBindTextureUnit(1, item.specular_map);
                GLCall(::glBindVertexArray(item.vao)); // Bind VAO
                // Draws like `glDrawElements`, and also hands `baseinstance` to the shaders as `gl_BaseInstance`.
                // With one instance, that number is just the index of this object's record in `u_phongShadedObjects`.
                GLCall(::glDrawElementsInstancedBaseVertexBaseInstance(
                    GL_TRIANGLES,
                    item.index_count,
                    GL_UNSIGNED_INT,
                    nullptr/* const GLvoid* indices */,
                    1/* GLsizei instancecount */,
                    0/* GLint basevertex */,
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

    void mesh_renderer::_render_unlit_vertex_shaded_objects(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
        pred_callback_type<geometry::mesh_object> const predicate,
        void* const predicate_userdata)
    {
        // Vertex-colored unlit objects bypass the deferred pipeline entirely; they are drawn
        // in the forward passes (opaque -> forward_opaque_pass, transparent -> WBOIT) and the
        // interpolated vertex color is output as-is without any lighting.
        core::shader_program* draw_shader = nullptr;
        phong_shaded_object_storage_vector* object_records = nullptr;
        bool render_opaque = false; // true: render opaque objects, false: render transparent objects

        switch (render_ctx.curr_render_pass) {
        case render_pass_type::deferred_opaque_pass:
            // Unlit objects are not part of the deferred geometry pass.
            return;
        case render_pass_type::forward_opaque_pass:
            draw_shader = &_unlit_vtxshaded_forward_opaque_pass_shader;
            object_records = &*_unlit_vtxshaded_forward_opaque_pass_records;
            render_opaque = true;
            break;
        case render_pass_type::forward_transparent_pass:
            draw_shader = &_unlit_vtxshaded_forward_trans_pass_shader;
            object_records = &*_unlit_vtxshaded_forward_trans_pass_records;
            render_opaque = false;
            break;
        default:
            TRIENGINE_PANIC("%s(): Unsupported render pass type: %d"
                , __func__
                , static_cast<int>(render_ctx.curr_render_pass)
            );
        }

        draw_shader->use();

        // Write a record per draw and collect the draws
        object_records->clear();
        _draw_items.clear();

        auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
        for (const auto& object : render_objects)
        {
            if (object->is_opaque() != render_opaque) {
                continue;
            }

            if (!object->is_visible() ||
                object->get_lighting_mode() != geometry::mesh_object::lighting_mode::unlit ||
                object->get_shading_mode() != geometry::mesh_object::shading_mode::vertex) {
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
            TRIENGINE_ASSERT(material != nullptr);

            // Update VAO
            if (object->is_dirty()) {
                object_gpu_rsrc->update(object);
                object->clear_dirty();
            }

            const size_t record_index = object_records->push_back(
                make_phong_shaded_object_record(object->get_model(), *material)
            );
            _draw_items.push_back(draw_item{
                object_gpu_rsrc->vao,
                static_cast<int32_t>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                static_cast<uint32_t>(record_index),
            });
        } // for

        // Upload the records written above, before the draws that read them
        object_records->upload();
        object_records->bind();

        // Render triangles
        for (const draw_item& item : _draw_items)
        {
            GLCall(::glBindVertexArray(item.vao)); // Bind VAO
            // Draws like `glDrawElements`, and also hands `baseinstance` to the shaders as `gl_BaseInstance`.
            // With one instance, that number is just the index of this object's record in `u_phongShadedObjects`.
            GLCall(::glDrawElementsInstancedBaseVertexBaseInstance(
                GL_TRIANGLES,
                item.index_count,
                GL_UNSIGNED_INT,
                nullptr/* const GLvoid* indices */,
                1/* GLsizei instancecount */,
                0/* GLint basevertex */,
                item.record_index/* GLuint baseinstance */
            ));
            //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
        } // for
    }

    void mesh_renderer::_render_unlit_texture_shaded_objects(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
        pred_callback_type<geometry::mesh_object> const predicate,
        void* const predicate_userdata)
    {
        // Texture-shaded unlit objects bypass the deferred pipeline entirely; they are drawn
        // in the forward passes (opaque -> forward_opaque_pass, transparent -> WBOIT) and the
        // diffuse map color is output as-is without any lighting. (specular map is unused)
        core::shader_program* draw_shader = nullptr;
        phong_shaded_object_storage_vector* object_records = nullptr;
        bool render_opaque = false; // true: render opaque objects, false: render transparent objects

        switch (render_ctx.curr_render_pass) {
        case render_pass_type::deferred_opaque_pass:
            // Unlit objects are not part of the deferred geometry pass.
            return;
        case render_pass_type::forward_opaque_pass:
            draw_shader = &_unlit_texshaded_forward_opaque_pass_shader;
            object_records = &*_unlit_texshaded_forward_opaque_pass_records;
            render_opaque = true;
            break;
        case render_pass_type::forward_transparent_pass:
            draw_shader = &_unlit_texshaded_forward_trans_pass_shader;
            object_records = &*_unlit_texshaded_forward_trans_pass_records;
            render_opaque = false;
            break;
        default:
            TRIENGINE_PANIC("%s(): Unsupported render pass type: %d"
                , __func__
                , static_cast<int>(render_ctx.curr_render_pass)
            );
        }

        draw_shader->use();

        // Write a record per draw and collect the draws
        object_records->clear();
        _unlit_texshaded_draw_items.clear();

        auto gpu_rsrc_mgr = _glctx->get_gpu_resource_manager();
        for (const auto& object : render_objects)
        {
            if (object->is_opaque() != render_opaque) {
                continue;
            }

            if (!object->is_visible() ||
                object->get_lighting_mode() != geometry::mesh_object::lighting_mode::unlit ||
                object->get_shading_mode() != geometry::mesh_object::shading_mode::texture) {
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
            // Unlit only needs the diffuse map (specular map is ignored), so do not require full is_valid().
            TRIENGINE_ASSERT(material != nullptr && material->diffuse_map != kInvalidTextureHandle);

            // Diffuse(base color) map; bound with the draw below
            auto diffuse_tex2d_gpu_rsrc = gpu_rsrc_mgr->get_texture_2d_resource(material->diffuse_map);
            TRIENGINE_ASSERT(diffuse_tex2d_gpu_rsrc != nullptr && diffuse_tex2d_gpu_rsrc->is_valid());

            // Update VAO
            if (object->is_dirty()) {
                object_gpu_rsrc->update(object);
                object->clear_dirty();
            }

            const size_t record_index = object_records->push_back(
                make_phong_shaded_object_record(object->get_model(), *material)
            );
            _unlit_texshaded_draw_items.push_back(unlit_texshaded_draw_item{
                {
                    object_gpu_rsrc->vao,
                    static_cast<int32_t>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                    static_cast<uint32_t>(record_index),
                },
                diffuse_tex2d_gpu_rsrc->id(),
            });
        } // for

        // Upload the records written above, before the draws that read them
        object_records->upload();
        object_records->bind();

        // Render triangles
        for (const unlit_texshaded_draw_item& item : _unlit_texshaded_draw_items)
        {
            ::glBindTextureUnit(0, item.diffuse_map);
            GLCall(::glBindVertexArray(item.vao)); // Bind VAO
            // Draws like `glDrawElements`, and also hands `baseinstance` to the shaders as `gl_BaseInstance`.
            // With one instance, that number is just the index of this object's record in `u_phongShadedObjects`.
            GLCall(::glDrawElementsInstancedBaseVertexBaseInstance(
                GL_TRIANGLES,
                item.index_count,
                GL_UNSIGNED_INT,
                nullptr/* const GLvoid* indices */,
                1/* GLsizei instancecount */,
                0/* GLint basevertex */,
                item.record_index/* GLuint baseinstance */
            ));
            //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
        } // for
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
        auto& object_records = *_normal_vis_records;
        draw_shader.use();

        // Write a record per draw and collect the draws
        object_records.clear();
        _draw_items.clear();

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

            // Update VAO (if needed)
            if (object->is_dirty()) {
                object_gpu_rsrc->update(object);
                object->clear_dirty();
            }

            // The record carries the material of the shading mode; this pass reads only `model` and the normal matrix
            const mat4_f32& model = object->get_model();
            const size_t record_index = object_records.push_back(
                object->get_shading_mode() == geometry::mesh_object::shading_mode::texture
                    ? make_phong_shaded_object_record(model, *object->get_texture_shading_material())
                    : make_phong_shaded_object_record(model, *object->get_vertex_shading_material())
            );
            _draw_items.push_back(draw_item{
                object_gpu_rsrc->vao,
                static_cast<int32_t>(triangle_indices.size() * std::decay_t<decltype(triangle_indices)>::value_type::SizeAtCompileTime),
                static_cast<uint32_t>(record_index),
            });
        } // for

        // Upload the records written above, before the draws that read them
        object_records.upload();
        object_records.bind();

        // Render triangle mesh
        for (const draw_item& item : _draw_items)
        {
            GLCall(::glBindVertexArray(item.vao)); // Bind VAO
            // Draws like `glDrawElements`, and also hands `baseinstance` to the shaders as `gl_BaseInstance`.
            // With one instance, that number is just the index of this object's record in `u_phongShadedObjects`.
            GLCall(::glDrawElementsInstancedBaseVertexBaseInstance(
                GL_TRIANGLES,
                item.index_count,
                GL_UNSIGNED_INT,
                nullptr/* const GLvoid* indices */,
                1/* GLsizei instancecount */,
                0/* GLint basevertex */,
                item.record_index/* GLuint baseinstance */
            ));
            //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
        } // for
    }

} // namespace