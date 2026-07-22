#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/geometry/mesh_object.hh>

namespace triengine::renderer
{
    class mesh_renderer
        : public renderer_base<mesh_renderer>
    {
    public:
        enum class render_mode_type
        {
            // Render lit (shaded) object surfaces (default visualization)
            // NOTE: Rendering must be performed in the following passes: deferred_opaque_pass, forward_transparent_pass(WBOIT)
            lit_shaded_surfaces,

            // Render unlit (no lighting) object surfaces
            // NOTE: Rendering must be performed in the following passes: forward_opaque_pass, forward_transparent_pass(WBOIT)
            unlit_shaded_surfaces,

            // Render object's vertex normals (debug visualization)
            // NOTE: Rendering must be performed in the following pass: forward_opaque_pass
            vertex_normals,

            //wireframes, // Render objects in wireframe mode (debug visualization)
        };

    private:
        // Renderer resources
        core::gl_context* _glctx{ nullptr };
        core::shader_program 
            _lit_vtxshaded_deferred_opaque_pass_shader,
            _lit_vtxshaded_forward_opaque_pass_shader,
            _lit_vtxshaded_forward_trans_pass_shader,
            _lit_texshaded_deferred_opaque_pass_shader,
            _lit_texshaded_forward_opaque_pass_shader,
            _lit_texshaded_forward_trans_pass_shader,
            _unlit_vtxshaded_forward_opaque_pass_shader,
            _unlit_vtxshaded_forward_trans_pass_shader,
            _unlit_texshaded_forward_opaque_pass_shader,
            _unlit_texshaded_forward_trans_pass_shader,
            _normal_vis_shader;

    public:
        mesh_renderer();
        ~mesh_renderer();

        // CRTP methods
        void create_impl(core::gl_context& glctx);
        void destroy_impl();
        void render_impl(
            const render_context& render_ctx,
            render_mode_type render_mode,
            const std::list<std::shared_ptr<geometry::mesh_object>>& render_obj_list,
            pred_callback_type<geometry::mesh_object> predicate = nullptr,
            void* predicate_userdata = nullptr
        );

    private:
        void _render_lit_vertex_shaded_objects(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
            pred_callback_type<geometry::mesh_object> predicate,
            void* predicate_userdata
        );

        void _render_lit_texture_shaded_objects(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
            pred_callback_type<geometry::mesh_object> predicate,
            void* predicate_userdata
        );

        void _render_unlit_vertex_shaded_objects(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
            pred_callback_type<geometry::mesh_object> predicate,
            void* predicate_userdata
        );

        void _render_unlit_texture_shaded_objects(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
            pred_callback_type<geometry::mesh_object> predicate,
            void* predicate_userdata
        );

        void _render_objects_normals(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::mesh_object>>& render_objects,
            pred_callback_type<geometry::mesh_object> predicate,
            void* predicate_userdata
        );

    }; // class

} // namespace