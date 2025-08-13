#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/geometry/mesh_object.hh>

namespace triengine::renderer
{
    class mesh_renderer
        : public object_renderer_base<mesh_renderer, geometry::mesh_object>
    {
    private:
        // Render options
        bool _show_object_normals{ false };

        // OpenGL resources
        core::gl_context* _glctx{ nullptr };
        core::shader_program 
            _vertmode_solid_shader,
            _vertmode_transparent_shader,
            _texmode_solid_shader,
            _texmode_transparent_shader,
            _normal_vis_shader;

    public:
        mesh_renderer();

        // Toggle object's normal visualization.
        void enable_object_normal_rendering(bool enable);

        // CRTP methods
        void create_impl(core::gl_context& glctx);
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
            const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
            pred_callback_type predicate,
            void* predicate_userdata
        );

        void _render_texture_shading_objects(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
            pred_callback_type predicate,
            void* predicate_userdata
        );

        void _render_objects_normals(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
            pred_callback_type predicate,
            void* predicate_userdata
        );

    }; // class

} // namespace