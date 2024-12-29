#include "triangle_mesh_renderer.hh"

#include "../misc/debug_utils.hh"
#include "../shader/triangle_mesh_shaders.h"
#include "../shader/normal_vis_shaders.h"

namespace triengine::renderer
{
    triangle_mesh_renderer::triangle_mesh_renderer()
    { }

    void triangle_mesh_renderer::enable_object_normal_rendering(bool enable)
    {
        _show_object_normals = enable;
    }

    void triangle_mesh_renderer::create(GLFWwindow* window)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        ::glfwMakeContextCurrent(window);

        //
        // Context Settings
        //

        // Create shader program
        _shader_vertmode.create();
        _shader_vertmode.attach_vertex_shader({ shader::glslShaderVersion, shader::kTriangleMeshVertModeVertexShader });
        _shader_vertmode.attach_fragment_shader({ shader::glslShaderVersion, shader::kTriangleMeshVertModeFragmentShader });
        _shader_vertmode.link();
        
        _shader_texmode.create();
        _shader_texmode.attach_vertex_shader({ shader::glslShaderVersion, shader::kTriangleMeshTexModeVertexShader });
        _shader_texmode.attach_fragment_shader({ shader::glslShaderVersion, shader::kTriangleMeshTexModeFragmentShader });
        _shader_texmode.link();
        
        _shader_normal_view.create();
        _shader_normal_view.attach_vertex_shader({ shader::glslShaderVersion, shader::kObjectNormalVisVertexShader });
        _shader_normal_view.attach_geometry_shader({ shader::glslShaderVersion, shader::kObjectNormalVisGeometryShader });
        _shader_normal_view.attach_fragment_shader({ shader::glslShaderVersion, shader::kObjectNormalVisFragmentShader });
        _shader_normal_view.link();

        // Get shader index
        _uloc_vertmode_model = _shader_vertmode.get_uniform("u_model");
        _uloc_vertmode_view = _shader_vertmode.get_uniform("u_view");
        _uloc_vertmode_proj = _shader_vertmode.get_uniform("u_proj");
        
        _uloc_texmode_model = _shader_texmode.get_uniform("u_model");
        _uloc_texmode_view = _shader_texmode.get_uniform("u_view");
        _uloc_texmode_proj = _shader_texmode.get_uniform("u_proj");

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

    void triangle_mesh_renderer::destroy()
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

            _shader_vertmode.destroy();
            _shader_texmode.destroy();
            _shader_normal_view.destroy();
        }
    }

    void triangle_mesh_renderer::render(
        const mat4_f32& view,
        const mat4_f32& projection,
        const lighting_options& light_opts)
    {
        this->render(
            this->get_objects(), 
            view,
            projection,
            light_opts
        );
    }

    void triangle_mesh_renderer::render(
        const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects,
        const mat4_f32& view,
        const mat4_f32& projection,
        const lighting_options& light_opts)
    {
        if (render_objects.empty()) {
            return;
        }

        // Enable depth testing
        GLCall(::glEnable(GL_DEPTH_TEST));

        this->_render_vertex_shading_objects(render_objects, view, projection, light_opts);
        this->_render_texture_shading_objects(render_objects, view, projection, light_opts);

        if (_show_object_normals) { // for debugging
            this->_render_objects_normals(render_objects, view, projection);
        }
    }

    void triangle_mesh_renderer::_render_vertex_shading_objects(
        const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects, 
        const mat4_f32& view, 
        const mat4_f32& projection, 
        const lighting_options& light_opts)
    {
        _shader_vertmode.use();

        // Update view, projection matrices
        GLCall(::glUniformMatrix4fv(_uloc_vertmode_view, 1, GL_FALSE, view.data()));
        GLCall(::glUniformMatrix4fv(_uloc_vertmode_proj, 1, GL_FALSE, projection.data()));

        // Update light options in shader
        light_opts.apply_to_shader(_shader_vertmode);

        for (const auto& object : render_objects)
        {
            if (!object->is_visible() || object->get_shading_mode() != geometry::triangle_mesh_object::shading_mode::vertex) {
                continue;
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
            GLCall(::glUniformMatrix4fv(_uloc_vertmode_model, 1, GL_FALSE, model.data()));

            // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
            _shader_vertmode.set_uniform_mat3("u_nmv", (view * model).inverse().transpose().topLeftCorner<3, 3>());

            // Update material
            _shader_vertmode.set_uniform_float("u_material.ambient", material->ambient);
            _shader_vertmode.set_uniform_float("u_material.diffuse", material->diffuse);
            _shader_vertmode.set_uniform_float("u_material.specular", material->specular);
            _shader_vertmode.set_uniform_float("u_material.shininess", static_cast<float>(material->shininess));

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
        const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects, 
        const mat4_f32& view, 
        const mat4_f32& projection, 
        const lighting_options& light_opts)
    {
        _shader_texmode.use();
        
        // Update view, projection matrices
        GLCall(::glUniformMatrix4fv(_uloc_texmode_view, 1, GL_FALSE, view.data()));
        GLCall(::glUniformMatrix4fv(_uloc_texmode_proj, 1, GL_FALSE, projection.data()));

        // Update light options in shader
        light_opts.apply_to_shader(_shader_texmode);

        for (const auto& object : render_objects)
        {
            if (!object->is_visible() || object->get_shading_mode() != geometry::triangle_mesh_object::shading_mode::texture) {
                continue;
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

            // Update model(transform) matrix
            const mat4_f32& model = object->get_model();
            GLCall(::glUniformMatrix4fv(_uloc_texmode_model, 1, GL_FALSE, model.data()));

            // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
            _shader_texmode.set_uniform_mat3("u_nmv", (view * model).inverse().transpose().topLeftCorner<3, 3>());

            // Update material shininess
            _shader_texmode.set_uniform_float("u_material.shininess", static_cast<float>(material->shininess));

            // Update material diffuse map
            _shader_texmode.set_uniform_int("u_material.diffuse", 0);
            ::glActiveTexture(GL_TEXTURE0);
            ::glBindTexture(GL_TEXTURE_2D, material->diffuse_map.id());
            
            // Update material specular map
            _shader_texmode.set_uniform_int("u_material.specular", 1);
            ::glActiveTexture(GL_TEXTURE1);
            ::glBindTexture(GL_TEXTURE_2D, material->specular_map.id());

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
        const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects, 
        const mat4_f32& view, 
        const mat4_f32& projection)
    {
        _shader_normal_view.use();

        // Update view, projection matrices
        _shader_normal_view.set_uniform_mat4("u_view_proj", projection * view);

        for (const auto& object : render_objects)
        {
            if (!object->is_visible()) {
                continue;
            }

            const auto& vertex_positions = object->vertex_positions;
            const auto& vertex_normals = object->vertex_normals;
            const auto& triangle_indices = object->triangle_indices;

            TRIENGINE_ASSERT(vertex_normals.size() == vertex_positions.size() || vertex_normals.empty());
            TRIENGINE_ASSERT(!triangle_indices.empty());

            // Update model(transform) matrix
            const mat4_f32& model = object->get_model();
            _shader_normal_view.set_uniform_mat4("u_model", model);

            // Update normal matrix; `mat3(transpose(inverse(u_model)))`
            _shader_normal_view.set_uniform_mat3("u_nm", model.inverse().transpose().topLeftCorner<3, 3>());


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