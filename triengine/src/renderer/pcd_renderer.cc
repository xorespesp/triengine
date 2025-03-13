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

    void pcd_renderer::create_impl(GLFWwindow* window)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        ::glfwMakeContextCurrent(window);

        //
        // Context Settings
        //

        GLCall(::glEnable(GL_PROGRAM_POINT_SIZE));

        // Create shader program
        _solid_shader
            .attach_vertex_shader({ shader::glslShaderVersion, shader::kPcdVertexShader })
            .attach_fragment_shader({ shader::glslShaderVersion, shader::kSolidPcdFragmentShader })
            .link();

        _transparent_shader
            .attach_vertex_shader({ shader::glslShaderVersion, shader::kPcdVertexShader })
            .attach_fragment_shader({ shader::glslShaderVersion, shader::kTransparentPcdFragmentShader })
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

    void pcd_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            GLCall(::glDeleteVertexArrays(1, &_vao));
            GLCall(::glDeleteBuffers(1, &_vbo_positions));
            GLCall(::glDeleteBuffers(1, &_vbo_normals));
            GLCall(::glDeleteBuffers(1, &_vbo_colors));

            _solid_shader.destroy();
            _transparent_shader.destroy();
        }
    }

    void pcd_renderer::set_pcd_point_size(float point_size)
    {
        _point_size = point_size;
    }

    void pcd_renderer::render_impl(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
        pred_callback_type const predicate,
        void* const predicate_userdata)
    {
        if (render_obj_list.empty()) {
            return;
        }

        // Enable depth testing
        //GLCall(::glEnable(GL_DEPTH_TEST));

        auto& draw_shader =
            (render_ctx.curr_render_pass == render_pass_type::wboit_transparent_rendering)
            ? _transparent_shader
            : _solid_shader;

        draw_shader.use();

        // Update view/projective matrices in shader
        draw_shader.set_uniform_mat4("u_view", render_ctx.view);
        draw_shader.set_uniform_mat4("u_proj", render_ctx.projection);

        // Update light options in shader
        render_ctx.light_opts->apply_to_shader(draw_shader);

        // Update point size
        GLCall(::glPointSize(static_cast<GLfloat>(_point_size.value_or(1.0f/* default size */))));

        for (const auto& object : render_obj_list)
        {
            if (!object->is_visible()) {
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

            const auto& point_positions = object->points;
            const auto& point_normals = object->normals;
            const auto& point_colors = object->colors;
            const auto& material = object->material;

            TRIENGINE_ASSERT(point_positions.size() == point_colors.size());
            TRIENGINE_ASSERT(point_normals.size() == point_positions.size() || point_normals.empty());

            if (point_positions.empty()) {
                continue;
            }

            // Update model(transform) matrix in shader
            const mat4_f32& model = object->get_model();
            draw_shader.set_uniform_mat4("u_model", model);

            // Update view-space normal matrix; `mat3(transpose(inverse(u_view * u_model)))`
            draw_shader.set_uniform_mat3("u_nmv", (render_ctx.view * model).inverse().transpose().topLeftCorner<3, 3>());

            // Update material
            draw_shader.set_uniform_float("u_material.ambient", material.ambient);
            draw_shader.set_uniform_float("u_material.diffuse", material.diffuse);
            draw_shader.set_uniform_float("u_material.specular", material.specular);
            draw_shader.set_uniform_float("u_material.shininess", static_cast<float>(material.shininess));

            // Update alpha material (WBOIT)
            if (render_ctx.curr_render_pass == render_pass_type::wboit_transparent_rendering) {
                draw_shader.set_uniform_float("u_material.alpha", material.alpha);
            }

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
