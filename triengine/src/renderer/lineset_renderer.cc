#include "lineset_renderer.hh"

#include "../misc/debug_utils.hh"
#include "../shader/lineset_shaders.h"

namespace triengine::renderer
{
    lineset_renderer::lineset_renderer()
    { }

    void lineset_renderer::create(GLFWwindow* window)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        ::glfwMakeContextCurrent(window);

        //
        // Context Settings
        //

        // Create shader program
        _shader.create();
        _shader.attach_vertex_shader({ shader::glslShaderVersion, shader::kLinesetVertexShader });
        _shader.attach_fragment_shader({ shader::glslShaderVersion, shader::kLinesetFragmentShader });
        _shader.link();

        // Get shader index
        _uloc_model = _shader.get_uniform("u_model");
        _uloc_view = _shader.get_uniform("u_view");
        _uloc_proj = _shader.get_uniform("u_proj");

        // ********************** Generate Vertex Array Object (VAO) **********************
        // Create & Bind VAO
        GLCall(::glGenVertexArrays(1, &_vao));
        GLCall(::glBindVertexArray(_vao));

        // Create vertex position VBO
        GLCall(::glGenBuffers(1, &_vbo_positions));
        GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_positions));
        GLCall(::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_f32), nullptr));
        GLCall(::glEnableVertexAttribArray(0));
        
        // Create vertex color VBO
        GLCall(::glGenBuffers(1, &_vbo_colors));
        GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_colors));
        GLCall(::glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_f32), nullptr));
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

    void lineset_renderer::destroy()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            GLCall(::glDeleteVertexArrays(1, &_vao));
            GLCall(::glDeleteBuffers(1, &_vbo_positions));
            GLCall(::glDeleteBuffers(1, &_ibo));

            _shader.destroy();
        }
    }

    void lineset_renderer::render(const render_context& render_ctx)
    {
        const auto& render_objects = this->get_objects();

        if (render_objects.empty()) {
            return;
        }

        // Enable depth testing
        GLCall(::glEnable(GL_DEPTH_TEST));

        // Enable smooth line
        //GLCall(::glEnable(GL_LINE_SMOOTH));

        GLCall(::glLineWidth(1.0f));

        _shader.use();

        // Update view/projective matrices in shader
        GLCall(::glUniformMatrix4fv(_uloc_view, 1, GL_FALSE, render_ctx.view.data()));
        GLCall(::glUniformMatrix4fv(_uloc_proj, 1, GL_FALSE, render_ctx.projection.data()));

        for (const auto& object : render_objects)
        {
            if (!object->is_visible()) {
                continue;
            }

            const auto& line_points = object->line_points;
            const auto& line_indices = object->line_indices;
            const auto& line_colors = object->line_colors;

            TRIENGINE_ASSERT(line_points.size() == line_colors.size());

            // update model(transform) matrix
            GLCall(::glUniformMatrix4fv(_uloc_model, 1, GL_FALSE, object->get_model().data()));

            // Update VAO
            // ********************************************************************************

            // Bind VAO
            GLCall(::glBindVertexArray(_vao));

            // Update vertex positions
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_positions));
            GLCall(::glBufferData(GL_ARRAY_BUFFER, 
                static_cast<GLsizeiptr>(line_points.size() * sizeof(std::decay_t<decltype(line_points)>::value_type)),
                line_points.data(), 
                GL_STREAM_DRAW
            ));
            
            // Update vertex colors
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_colors));
            GLCall(::glBufferData(GL_ARRAY_BUFFER, 
                static_cast<GLsizeiptr>(line_colors.size() * sizeof(std::decay_t<decltype(line_colors)>::value_type)),
                line_colors.data(), 
                GL_STREAM_DRAW
            ));

            // Update vertex indices
            GLCall(::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo));
            GLCall(::glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
                line_indices.size() * sizeof(std::decay_t<decltype(line_indices)>::value_type),
                line_indices.data(), 
                GL_STREAM_DRAW
            ));

            // ********************************************************************************

            // Render lines
            GLCall(::glDrawElements(
                GL_LINES, 
                static_cast<GLsizei>(line_indices.size() * line_indices.front().size()),
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
