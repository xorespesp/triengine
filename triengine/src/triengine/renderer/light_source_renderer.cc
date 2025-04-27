#include "light_source_renderer.hh"

#include <triengine/utility/debug_utils.hh>
#include <triengine/shaders/light_source_shaders.h>

namespace triengine::renderer
{
    light_source_renderer::light_source_renderer()
    {}

    void light_source_renderer::create_impl(core::gl_context& glctx, const core::shader_preprocessor& shader_prep)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        ::glfwMakeContextCurrent(glctx.get_glfw_window());

        //
        // Context Settings
        //

        // Create shader program
        _shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kLightSourceVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kLightSourceFragmentShader).c_str() })
            .link();

        // ********************** Generate Vertex Array Object (VAO) **********************
        // Create & Bind VAO
        GLCall(::glGenVertexArrays(1, &_vao));
        GLCall(::glBindVertexArray(_vao));

        // Create vertex position VBO
        GLCall(::glGenBuffers(1, &_vbo_positions));
        GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_positions));
        GLCall(::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
        GLCall(::glEnableVertexAttribArray(0));

        // Create vertex normal VBO
        GLCall(::glGenBuffers(1, &_vbo_normals));
        GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_normals));
        GLCall(::glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
        GLCall(::glEnableVertexAttribArray(1));

        // Create Index Buffer Object (IBO)
        GLCall(::glGenBuffers(1, &_ibo));
        //GLCall(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo));

        // Unbind VAO
        GLCall(::glBindVertexArray(0));
        GLCall(::glBindBuffer(GL_ARRAY_BUFFER, 0));
        //GLCall(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
        // ********************************************************************************
    }

    void light_source_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            GLCall(::glDeleteVertexArrays(1, &_vao));
            GLCall(::glDeleteBuffers(1, &_vbo_positions));
            GLCall(::glDeleteBuffers(1, &_vbo_normals));
            GLCall(::glDeleteBuffers(1, &_ibo));

            _shader.destroy();
        }
    }

    void light_source_renderer::render_impl(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
        pred_callback_type const predicate,
        void* const predicate_userdata)
    {
        if (render_ctx.curr_render_pass != render_pass_type::wboit_solid_rendering) {
            return;
        }

        if (render_obj_list.empty()) {
            return;
        }

        auto& draw_shader = _shader;
        draw_shader.use();

        // Enable depth testing
        //GLCall(::glEnable(GL_DEPTH_TEST));

        // Update view/projective matrices in shader
        draw_shader.set_uniform_mat4("u_view", render_ctx.view);
        draw_shader.set_uniform_mat4("u_proj", render_ctx.projection);

        for (const auto& object : render_obj_list)
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
            const auto& triangle_color = object->color;

            TRIENGINE_ASSERT(vertex_normals.size() == vertex_positions.size() || vertex_normals.empty());
            TRIENGINE_ASSERT(!triangle_indices.empty());

            // Update model(transform) matrix in shader
            draw_shader.set_uniform_mat4("u_model", object->get_model());

            // Update color
            draw_shader.set_uniform_vec3("u_color", triangle_color.r(), triangle_color.g(), triangle_color.b());
            // Update VAO
            // ********************************************************************************

            // Bind VAO
            GLCall(::glBindVertexArray(_vao));

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

            // Update vertex indices
            GLCall(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo));
            GLCall(::glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                triangle_indices.size() * sizeof(std::decay_t<decltype(triangle_indices)>::value_type),
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