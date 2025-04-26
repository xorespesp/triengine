#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/infinite_grid_options.hh>

namespace triengine::renderer
{
    class infinite_grid_renderer
        : public renderer_base<infinite_grid_renderer>
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

        // CRTP methods
        void create_impl(core::gl_context& glctx, const core::shader_preprocessor& shader_prep);
        void destroy_impl();
        void render_impl(const render_context& render_ctx);

    }; // class

} // namespace