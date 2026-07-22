#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/geometry/skeleton_object.hh>
#include <triengine/renderer/mesh_renderer.hh>

namespace triengine::renderer
{
    class skeleton_renderer
        : public renderer_base<skeleton_renderer>
    {
    private:
        // Render options
        bool _flag_show_joint_axis{ false };

        // Internal renderer
        mesh_renderer _mesh_renderer;

    public:
        skeleton_renderer();
        ~skeleton_renderer();

        void show_joint_axis(bool show);

        // CRTP methods
        void create_impl(core::gl_context& glctx);
        void destroy_impl();
        void render_impl(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::skeleton_object>>& render_obj_list,
            pred_callback_type<geometry::skeleton_object> predicate = nullptr,
            void* predicate_userdata = nullptr);
    };
}