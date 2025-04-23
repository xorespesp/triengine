#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/geometry/triangle_mesh_object.hh>

namespace triengine::renderer
{
    class triangle_mesh_renderer
        : public object_renderer_base<triangle_mesh_renderer, geometry::triangle_mesh_object>
    {
    private:
        // OpenGL shaders
        shader_program 
            _vertmode_solid_shader,
            _vertmode_transparent_shader,
            _texmode_solid_shader,
            _texmode_transparent_shader,
            _normal_vis_shader;

        // OpenGL objects
        GLuint // vertex array object
            _vao_vertmode{}, // Coloring by linear-interpolation from vertices, requires vertex colors (as rgb)
            _vao_texmode{}; // Coloring by texture, requires texture uv coordinates (color information)

        GLuint // vertex buffer objects
            _vbo_positions{},
            _vbo_normals{},
            _vbo_colors{},
            _vbo_texcoords{}; // texture uv coordinates

        GLuint _ibo{}; // index buffer object

        bool _show_object_normals{ false };

    public:
        triangle_mesh_renderer();

        // Toggle object's normal visualization.
        void enable_object_normal_rendering(bool enable);

        // CRTP methods
        void create_impl(gl_context& glctx, const shader_preprocessor& shader_prep);
        void destroy_impl();
        void render_impl(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
            pred_callback_type predicate,
            void* predicate_userdata
        );

    private:
        void _render_vertex_shading_objects(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects,
            pred_callback_type predicate,
            void* predicate_userdata
        );

        void _render_texture_shading_objects(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects,
            pred_callback_type predicate,
            void* predicate_userdata
        );

        void _render_objects_normals(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects,
            pred_callback_type predicate,
            void* predicate_userdata
        );

    }; // class

} // namespace