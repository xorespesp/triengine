#pragma once
#include <triengine/renderer/renderer_base.hh>

namespace triengine::renderer
{
    class light_source_renderer
        : public renderer_base<light_source_renderer>
    {
    private:
        // OpenGL resources
        core::shader_program _point_light_source_shader;
        GLuint _vao{};
        GLuint _vbo{};
        GLuint _ibo{};
        GLsizei _num_triangle_indices{};

    public:
        light_source_renderer();

        // CRTP methods
        void create_impl(core::gl_context& glctx, const core::shader_loader& shader_ldr, const core::shader_preprocessor& shader_prep);
        void destroy_impl();
        void render_impl(const render_context& render_ctx);

    }; // class

} // namespace