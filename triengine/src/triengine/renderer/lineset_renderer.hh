#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/geometry/lineset_object.hh>

namespace triengine::renderer
{
    class lineset_renderer
        : public object_renderer_base<lineset_renderer, geometry::lineset_object>
    {
    private:
        // OpenGL shaders
        shader_program _shader;

        // OpenGL objects
        GLuint _vao{}; // vertex array object
        GLuint _vbo_positions{}, _vbo_colors{}; // vertex buffer objects
        GLuint _ibo{}; // index buffer object

    public:
        lineset_renderer();

        // CRTP methods
        void create_impl(core::gl_context& glctx, const core::shader_preprocessor& shader_prep);
        void destroy_impl();
        void render_impl(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
            pred_callback_type predicate,
            void* predicate_userdata);

    };

} // namespace