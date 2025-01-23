#pragma once
#include "renderer_base.hh"
#include "../infinite_grid_options.hh"

namespace triengine::renderer
{
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