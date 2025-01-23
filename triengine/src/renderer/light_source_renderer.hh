#pragma once
#include "renderer_base.hh"
#include "../geometry/light_source_object.hh"

namespace triengine::renderer
{
    class light_source_renderer
        : public object_renderer_base<geometry::light_source_object>
    {
    private:
        // OpenGL shaders
        shader_program _shader;

        // OpenGL objects
        GLuint _vao{}; // vertex array object
        GLuint _vbo_positions{}, _vbo_normals{}; // vertex buffer objects
        GLuint _ibo{}; // index buffer object

        // OpenGL shader uniform locations
        GLint _uloc_model{};
        GLint _uloc_view{};
        GLint _uloc_projection{};
        GLint _uloc_color{};

    public:
        light_source_renderer();

        // Renderer functions
        void create(GLFWwindow* window) override;
        void destroy() override;
        void render(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<render_object_type>>& render_obj_list
        ) override;

    }; // class

} // namespace