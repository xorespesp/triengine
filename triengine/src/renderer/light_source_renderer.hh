#pragma once
#include "renderer_base.hh"
#include "../geometry/light_source_object.hh"

namespace triengine::renderer
{
    class light_source_renderer
        : public object_renderer_base<light_source_renderer, geometry::light_source_object>
    {
    private:
        // OpenGL shaders
        shader_program _shader;

        // OpenGL objects
        GLuint _vao{}; // vertex array object
        GLuint _vbo_positions{}, _vbo_normals{}; // vertex buffer objects
        GLuint _ibo{}; // index buffer object

    public:
        light_source_renderer();

        // CRTP methods
        void create_impl(GLFWwindow* window);
        void destroy_impl();
        void render_impl(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
            pred_callback_type predicate,
            void* predicate_userdata);

    }; // class

} // namespace