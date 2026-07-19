#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/infinite_plane_options.hh>

namespace triengine::renderer
{
    class infinite_plane_renderer
        : public renderer_base<infinite_plane_renderer>
    {
    private:
        // Render options
        infinite_plane_options _options;

        // Renderer resources
        core::shader_program _transparent_grid_shader;
        core::shader_program _box_filtered_grid_shader;
        core::shader_program _box_filtered_chess_shader;
        GLuint _vao{};

    public:
        infinite_plane_renderer();

        const infinite_plane_options& get_options() const { return _options; }
        void set_options(const infinite_plane_options& options) { _options = options; }

        // CRTP methods
        void create_impl(core::gl_context& glctx);
        void destroy_impl();
        void render_impl(const render_context& render_ctx);

    }; // class

} // namespace