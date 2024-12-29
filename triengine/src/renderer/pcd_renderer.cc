#include "pcd_renderer.hh"

#include "../shader/pcd_shaders.h"
#include "../misc/debug_utils.hh"

namespace triengine::renderer
{
    pcd_renderer::pcd_renderer()
    { }

    pcd_renderer::~pcd_renderer()
    {
        this->destroy();
    }

    void pcd_renderer::create(GLFWwindow* window)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        ::glfwMakeContextCurrent(window);

        //
        // Context Settings
        //

        GLCall(::glEnable(GL_PROGRAM_POINT_SIZE));

        // Create shader program
        _shader.create();
        _shader.attach_vertex_shader({ shader::glslShaderVersion, shader::kPcdVertexShader });
        _shader.attach_fragment_shader({ shader::glslShaderVersion, shader::kPcdFragmentShader });
        _shader.link();

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
        GLCall(::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
        GLCall(::glEnableVertexAttribArray(0));

        // Create vertex normal VBO
        GLCall(::glGenBuffers(1, &_vbo_normals));
        GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_normals));
        GLCall(::glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
        GLCall(::glEnableVertexAttribArray(1));

        // Create vertex color VBO
        GLCall(::glGenBuffers(1, &_vbo_colors));
        GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_colors));
        GLCall(::glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
        GLCall(::glEnableVertexAttribArray(2));

        // Unbind VAO
        GLCall(::glBindVertexArray(0));
        GLCall(::glBindBuffer(GL_ARRAY_BUFFER, 0));
        // ********************************************************************************
    }

    void pcd_renderer::destroy()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            GLCall(::glDeleteVertexArrays(1, &_vao));
            GLCall(::glDeleteBuffers(1, &_vbo_positions));
            GLCall(::glDeleteBuffers(1, &_vbo_normals));
            GLCall(::glDeleteBuffers(1, &_vbo_colors));

            _shader.destroy();
        }
    }

    void pcd_renderer::set_pcd_point_size(float point_size)
    {
        _point_size = point_size;
    }

    void pcd_renderer::render(
        const mat4_f32& view,
        const mat4_f32& projection,
        const lighting_options& light_opts)
    {
        const auto& render_objects = this->get_objects();

        if (render_objects.empty()) {
            return;
        }

        // Enable depth testing
        GLCall(::glEnable(GL_DEPTH_TEST));
        
        _shader.use();

        // Update model/view/projective matrices in shader
        GLCall(::glUniformMatrix4fv(_uloc_view, 1, GL_FALSE, view.data()));
        GLCall(::glUniformMatrix4fv(_uloc_proj, 1, GL_FALSE, projection.data()));

        // Update light options in shader
        light_opts.apply_to_shader(_shader);

        // Update point size
        GLCall(::glPointSize(static_cast<GLfloat>(_point_size.value_or(1.0f/* default size */))));

        for (const auto& object : render_objects)
        {
            if (!object->is_visible()) {
                continue;
            }

            const auto& point_positions = object->points;
            const auto& point_normals = object->normals;
            const auto& point_colors = object->colors;

            TRIENGINE_ASSERT(point_positions.size() == point_colors.size());
            TRIENGINE_ASSERT(point_normals.size() == point_positions.size() || point_normals.empty());

            if (point_positions.empty()) {
                continue;
            }

            // Update model(transform) matrix
            const mat4_f32& model = object->get_model();
            GLCall(::glUniformMatrix4fv(_uloc_model, 1, GL_FALSE, model.data()));

            // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
            _shader.set_uniform_mat3("u_nmv", (view * model).inverse().transpose().topLeftCorner<3, 3>());

            // Update VAO
            // ********************************************************************************

            // Bind VAO
            GLCall(::glBindVertexArray(_vao));

            // Update vertex positions
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_positions));
            GLCall(::glBufferData(GL_ARRAY_BUFFER, 
                static_cast<GLsizeiptr>(point_positions.size() * sizeof(std::decay_t<decltype(point_positions)>::value_type)),
                point_positions.data(),
                GL_STREAM_DRAW
            ));

            // Update vertex normals
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_normals));
            GLCall(::glBufferData(GL_ARRAY_BUFFER, 
                static_cast<GLsizeiptr>(point_normals.size() * sizeof(std::decay_t<decltype(point_normals)>::value_type)), 
                point_normals.empty() ? nullptr : point_normals.data(),
                GL_STREAM_DRAW
            ));

            // Update vertex colors
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_colors));
            GLCall(::glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(point_colors.size() * sizeof(std::decay_t<decltype(point_colors)>::value_type)), 
                point_colors.data(),
                GL_STREAM_DRAW
            ));

            // ********************************************************************************

            // Render points
            GLCall(::glDrawArrays(
                GL_POINTS, 
                0, 
                static_cast<GLsizei>(point_positions.size())
            ));

            // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBindVertexArray.xhtml
            // https://www.reddit.com/r/opengl/comments/f3sclv/comment/fhkou86/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button
            // `glBindVertexArray(0)` will unbind any bound VAO. 
            // I mean, as long as you always bind another VAO before you draw another object, you really don't have to do this.
            //GLCall(::glBindVertexArray(0));
        } // for
    }

} // namespace
