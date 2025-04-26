#include "triangle_mesh_renderer.hh"

#include <triengine/misc/debug_utils.hh>
#include <triengine/shaders/triangle_mesh_shaders.h>
#include <triengine/shaders/normal_vis_shaders.h>

namespace triengine::renderer
{
    triangle_mesh_renderer::triangle_mesh_renderer()
    { }

    void triangle_mesh_renderer::enable_object_normal_rendering(bool enable)
    {
        _show_object_normals = enable;
    }

    void triangle_mesh_renderer::create_impl(core::gl_context& glctx, const core::shader_preprocessor& shader_prep)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        ::glfwMakeContextCurrent(glctx.get_glfw_window());

        //
        // Context Settings
        //

        // Create shader program
        _vertmode_solid_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kVertShadedTriangleMeshVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kVertShadedSolidTriangleMeshFragmentShader).c_str() })
            .link();

        _vertmode_transparent_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kVertShadedTriangleMeshVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kVertShadedTransparentTriangleMeshFragmentShader).c_str() })
            .link();

        _texmode_solid_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kTexShadedTriangleMeshVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kTexShadedSolidTriangleMeshFragmentShader).c_str() })
            .link();

        _texmode_transparent_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kTexShadedTriangleMeshVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kTexShadedTransparentTriangleMeshFragmentShader).c_str() })
            .link();
        
        _normal_vis_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kObjectNormalVisVertexShader).c_str() })
            .attach_geometry_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kObjectNormalVisGeometryShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kObjectNormalVisFragmentShader).c_str() })
            .link();

        //
        // NOTE: Since the vertex memory layout differs depending on the shading(coloring) mode, 
        //       so we create a separate VAO for each shading mode and then bind the necessary VBOs for each.
        // 
        //       (VBO can be bound to multiple VAOs, because they do not store vertex data itself, but references to VBOs.)
        //       https://computergraphics.stackexchange.com/a/4625
        //       https://stackoverflow.com/a/15439487
        //

        {
            // ****************************** Create GL Objects ******************************

            // Create vertex-shading VAO
            GLCall(::glGenVertexArrays(1, &_vao_vertmode));

            // Create texture-shading VAO
            GLCall(::glGenVertexArrays(1, &_vao_texmode));

            // Create vertex position VBO
            GLCall(::glGenBuffers(1, &_vbo_positions));

            // Create vertex normal VBO
            GLCall(::glGenBuffers(1, &_vbo_normals));

            // Create vertex color VBO
            GLCall(::glGenBuffers(1, &_vbo_colors));
            
            // Create vertex color VBO
            GLCall(::glGenBuffers(1, &_vbo_texcoords));

            // Create Index Buffer Object (IBO)
            GLCall(::glGenBuffers(1, &_ibo));

            // ********************************************************************************
        }

        {
            // ****************************** Setup Vertex-Shading VAO ******************************

            // Bind VAO
            GLCall(::glBindVertexArray(_vao_vertmode));

            // Bind vertex position VBO
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_positions));
            GLCall(::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
            GLCall(::glEnableVertexAttribArray(0));

            // Bind vertex normal VBO
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_normals));
            GLCall(::glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
            GLCall(::glEnableVertexAttribArray(1));

            // Bind vertex color VBO
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_colors));
            GLCall(::glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
            GLCall(::glEnableVertexAttribArray(2));

            // Bind Index Buffer Object (IBO)
            GLCall(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo));
            //...
            
            // ********************************************************************************
        }
        
        {
            // ****************************** Setup Texture-Shading VAO ******************************

            // Bind VAO
            GLCall(::glBindVertexArray(_vao_texmode));

            // Bind vertex position VBO
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_positions));
            GLCall(::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
            GLCall(::glEnableVertexAttribArray(0));

            // Bind vertex normal VBO
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_normals));
            GLCall(::glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
            GLCall(::glEnableVertexAttribArray(1));

            // Bind texture uv coordinates VBO
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_texcoords));
            GLCall(::glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr));
            GLCall(::glEnableVertexAttribArray(2));

            // Bind Index Buffer Object (IBO)
            GLCall(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo));
            //...
            
            // ********************************************************************************
        }

        // Unbind VAO
        GLCall(::glBindVertexArray(0));
        GLCall(::glBindBuffer(GL_ARRAY_BUFFER, 0));
        GLCall(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    }

    void triangle_mesh_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            GLCall(::glDeleteVertexArrays(1, &_vao_vertmode));
            GLCall(::glDeleteVertexArrays(1, &_vao_texmode));
            GLCall(::glDeleteBuffers(1, &_vbo_positions));
            GLCall(::glDeleteBuffers(1, &_vbo_normals));
            GLCall(::glDeleteBuffers(1, &_vbo_colors));
            GLCall(::glDeleteBuffers(1, &_vbo_texcoords));
            GLCall(::glDeleteBuffers(1, &_ibo));

            _vertmode_solid_shader.destroy();
            _vertmode_transparent_shader.destroy();
            _texmode_solid_shader.destroy();
            _texmode_transparent_shader.destroy();
            _normal_vis_shader.destroy();
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
        render_ctx.light_opts->apply_to_shader(draw_shader);

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

            const auto& vertex_positions = object->vertex_positions;
            const auto& vertex_normals = object->vertex_normals;
            const auto& vertex_colors = object->vertex_colors;
            const auto& triangle_indices = object->triangle_indices;
            const auto* const material = object->get_vertex_shading_material();

            TRIENGINE_ASSERT(vertex_colors.size() == vertex_positions.size());
            TRIENGINE_ASSERT(vertex_normals.size() == vertex_positions.size() || vertex_normals.empty());
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
            // ********************************************************************************

            // Bind VAO
            GLCall(::glBindVertexArray(_vao_vertmode));

            // Update vertex positions
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_positions));
            GLCall(::glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertex_positions.size() * sizeof(std::decay_t<decltype(vertex_positions)>::value_type)),
                vertex_positions.data(),
                GL_STREAM_DRAW
            ));

            // Update vertex normals
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_normals));
            GLCall(::glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertex_normals.size() * sizeof(std::decay_t<decltype(vertex_normals)>::value_type)),
                vertex_normals.empty() ? nullptr : vertex_normals.data(),
                GL_STREAM_DRAW
            ));

            // Update vertex colors
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_colors));
            GLCall(::glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertex_colors.size() * sizeof(std::decay_t<decltype(vertex_colors)>::value_type)),
                vertex_colors.data(),
                GL_STREAM_DRAW
            ));

            // Update vertex indices
            GLCall(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo));
            GLCall(::glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(triangle_indices.size() * sizeof(std::decay_t<decltype(triangle_indices)>::value_type)),
                triangle_indices.data(),
                GL_STREAM_DRAW
            ));

            // ********************************************************************************

            // Render triangles
            GLCall(::glDrawElements(
                GL_TRIANGLES,
                static_cast<GLsizei>(triangle_indices.size() * triangle_indices.front().size()),
                GL_UNSIGNED_INT,
                NULL
            ));

            // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBindVertexArray.xhtml
            // https://www.reddit.com/r/opengl/comments/f3sclv/comment/fhkou86/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
            // `glBindVertexArray(0)` will unbind any bound VAO. 
            // I mean, as long as you always bind another VAO before you draw another object, you really don't have to do this.
            //GLCall(::glBindVertexArray(0));
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
        render_ctx.light_opts->apply_to_shader(draw_shader);

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

            const auto& vertex_positions = object->vertex_positions;
            const auto& vertex_normals = object->vertex_normals;
            const auto& vertex_uvs = object->vertex_uvs;
            const auto& triangle_indices = object->triangle_indices;
            const auto* const material = object->get_texture_shading_material();

            TRIENGINE_ASSERT(vertex_uvs.size() == vertex_positions.size());
            TRIENGINE_ASSERT(vertex_normals.size() == vertex_positions.size() || vertex_normals.empty());
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
            // ********************************************************************************

            // Bind VAO
            GLCall(::glBindVertexArray(_vao_texmode));

            // Update vertex positions
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_positions));
            GLCall(::glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertex_positions.size() * sizeof(std::decay_t<decltype(vertex_positions)>::value_type)),
                vertex_positions.data(),
                GL_STREAM_DRAW
            ));

            // Update vertex normals
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_normals));
            GLCall(::glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertex_normals.size() * sizeof(std::decay_t<decltype(vertex_normals)>::value_type)),
                vertex_normals.data(),
                GL_STREAM_DRAW
            ));

            // Update texture uv coordinates
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_texcoords));
            GLCall(::glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertex_uvs.size() * sizeof(std::decay_t<decltype(vertex_uvs)>::value_type)),
                vertex_uvs.data(),
                GL_STREAM_DRAW
            ));

            // Update vertex indices
            GLCall(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo));
            GLCall(::glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(triangle_indices.size() * sizeof(std::decay_t<decltype(triangle_indices)>::value_type)),
                triangle_indices.data(),
                GL_STREAM_DRAW
            ));

            // ********************************************************************************

            // Render triangles
            GLCall(::glDrawElements(
                GL_TRIANGLES,
                static_cast<GLsizei>(triangle_indices.size() * triangle_indices.front().size()),
                GL_UNSIGNED_INT,
                NULL
            ));

            // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBindVertexArray.xhtml
            // https://www.reddit.com/r/opengl/comments/f3sclv/comment/fhkou86/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
            // `glBindVertexArray(0)` will unbind any bound VAO. 
            // I mean, as long as you always bind another VAO before you draw another object, you really don't have to do this.
            //GLCall(::glBindVertexArray(0));
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

        for (const auto& object : render_objects)
        {
            if (!object->is_visible()) {
                continue;
            }

            if (predicate && !predicate(*object, predicate_userdata)) {
                continue;
            }

            const auto& vertex_positions = object->vertex_positions;
            const auto& vertex_normals = object->vertex_normals;
            const auto& triangle_indices = object->triangle_indices;

            TRIENGINE_ASSERT(vertex_normals.size() == vertex_positions.size() || vertex_normals.empty());
            TRIENGINE_ASSERT(!triangle_indices.empty());

            // Update model(transform) matrix
            const mat4_f32& model = object->get_model();
            draw_shader.set_uniform_mat4("u_model", model);

            // Update normal matrix; `mat3(transpose(inverse(u_model)))`
            draw_shader.set_uniform_mat3("u_nm", model.inverse().transpose().topLeftCorner<3, 3>());


            // Update VAO
            // ********************************************************************************

            // Bind VAO
            GLCall(::glBindVertexArray(
                object->get_shading_mode() == geometry::triangle_mesh_object::shading_mode::texture
                ? _vao_texmode
                : _vao_vertmode
            ));

            // Update vertex positions
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_positions));
            GLCall(::glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertex_positions.size() * sizeof(std::decay_t<decltype(vertex_positions)>::value_type)),
                vertex_positions.data(),
                GL_STREAM_DRAW
            ));

            // Update vertex normals
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_normals));
            GLCall(::glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertex_normals.size() * sizeof(std::decay_t<decltype(vertex_normals)>::value_type)),
                vertex_normals.data(),
                GL_STREAM_DRAW
            ));

            // Update vertex indices
            GLCall(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo));
            GLCall(::glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(triangle_indices.size() * sizeof(std::decay_t<decltype(triangle_indices)>::value_type)),
                triangle_indices.data(),
                GL_STREAM_DRAW
            ));

            // ********************************************************************************

            // Render triangles
            GLCall(::glDrawElements(
                GL_TRIANGLES,
                static_cast<GLsizei>(triangle_indices.size() * triangle_indices.front().size()),
                GL_UNSIGNED_INT,
                NULL
            ));

            // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBindVertexArray.xhtml
            // https://www.reddit.com/r/opengl/comments/f3sclv/comment/fhkou86/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
            // `glBindVertexArray(0)` will unbind any bound VAO. 
            // I mean, as long as you always bind another VAO before you draw another object, you really don't have to do this.
            //GLCall(::glBindVertexArray(0));
        } // for
    }

} // namespace