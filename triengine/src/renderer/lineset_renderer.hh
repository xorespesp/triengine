#pragma once
#include "renderer_base.hh"
#include "../geometry/lineset_object.hh"

#include <list>

namespace triengine::renderer
{
    class lineset_renderer
        : public renderer_base<geometry::lineset_object>
    {
    private:
        // OpenGL shaders
        shader_program _shader;

        // OpenGL objects
        GLuint _vao{}; // vertex array object
        GLuint _vbo_positions{}, _vbo_colors{}; // vertex buffer objects
        GLuint _ibo{}; // index buffer object

        // OpenGL shader uniform locations
        GLint _uloc_model{}; // model matrix
        GLint _uloc_view{}; // view matrix
        GLint _uloc_proj{}; // projection matrix

    public:
        lineset_renderer();

        // Renderer functions
        void create(GLFWwindow* window) override;
        void destroy() override;
        void render(
            const mat4_f32& view,
            const mat4_f32& projection,
            const lighting_options& light_opts
        ) override;
    };

} // namespace