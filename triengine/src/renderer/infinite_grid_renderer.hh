#pragma once
#include "renderer_base.hh"

namespace triengine::renderer
{
    struct infinite_grid_options
    {
        vec3_f32 grid_color{ 0.5f, 0.5f, 0.5f };
        float grid_cell_size{ 0.05f };

        infinite_grid_options() = default;
    };

    class infinite_grid_renderer
        : public renderer_base
    {
    private:
        // OpenGL shaders
        shader_program _shader;

        // OpenGL objects
        GLuint _vao{}; // vertex array object

        // Render Options
        infinite_grid_options _options;

    public:
        infinite_grid_renderer();

        const infinite_grid_options& get_options() const { return _options; }
        void set_options(const infinite_grid_options& options) { _options = options; }
        
        // Renderer functions
        void create(GLFWwindow* window) override;
        void destroy() override;
        void render(const render_context& render_ctx) override;

    }; // class

} // namespace