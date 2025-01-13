#pragma once
#include "renderer_base.hh"
#include "../geometry/pcd_object.hh"

#include <optional>

namespace triengine::renderer
{
    class pcd_renderer
        : public object_renderer_base<geometry::pcd_object>
    {
    private:
        // Render options
        std::optional<float> _point_size;

        // OpenGL shaders
        shader_program _shader;

        // OpenGL objects
        GLuint _vao{}; // vertex array object
        GLuint // vertex buffer objects
            _vbo_positions{}, 
            _vbo_normals{}, 
            _vbo_colors{};

        // OpenGL shader uniform locations
        GLint _uloc_model{}; // model matrix
        GLint _uloc_view{}; // view matrix
        GLint _uloc_proj{}; // projection matrix

    public:
        pcd_renderer();
        virtual ~pcd_renderer();

        void set_pcd_point_size(float point_size);

        void create(GLFWwindow* window) override;
        void destroy() override;
        void render(const render_context& render_ctx) override;
    };

} // namespace