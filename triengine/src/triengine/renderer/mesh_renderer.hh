#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/geometry/mesh_object.hh>

namespace triengine::renderer
{
    class mesh_renderer
        : public object_renderer_base<mesh_renderer, geometry::mesh_object>
    {
    public:
        enum class render_mode_type
        {
            // Render shaded object surfaces (default visualization)
            // NOTE: Rendering must be performed in the following passes: deferred_opaque_pass, forward_transparent_pass(WBOIT)
            shaded_surfaces, 

            // Render object's vertex normals (debug visualization)
            // NOTE: Rendering must be performed in the following pass: forward_opaque_pass
            vertex_normals, 

            //wireframes, // Render objects in wireframe mode (debug visualization)
        };

    private:
        // Render options
        render_mode_type _curr_render_mode{ render_mode_type::shaded_surfaces };

        // OpenGL resources
        core::gl_context* _glctx{ nullptr };
        core::shader_program 
            _vtxshaded_deferred_opaque_pass_shader,
            _vtxshaded_forward_opaque_pass_shader,
            _vtxshaded_forward_trans_pass_shader,
            _texshaded_deferred_opaque_pass_shader,
            _texshaded_forward_opaque_pass_shader,
            _texshaded_forward_trans_pass_shader,
            _normal_vis_shader;

    public:
        mesh_renderer();

        render_mode_type get_render_mode() const noexcept { return _curr_render_mode; }
        void set_render_mode(render_mode_type new_mode);

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
        void _render_vertex_shaded_objects(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
            pred_callback_type predicate,
            void* predicate_userdata
        );

        void _render_texture_shaded_objects(
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