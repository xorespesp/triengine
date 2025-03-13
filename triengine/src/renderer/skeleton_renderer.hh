#pragma once
#include "renderer_base.hh"
#include "../geometry/skeleton_object.hh"

#include "triangle_mesh_renderer.hh"

namespace triengine::renderer
{
    class skeleton_renderer
        : public object_renderer_base<skeleton_renderer, geometry::skeleton_object>
    {
    private:
        // Render options
        bool _flag_show_joint_axis{ false };

        // Render shape object
        triangle_mesh_renderer _mesh_renderer;

    public:
        skeleton_renderer();
        virtual ~skeleton_renderer();

        void show_joint_axis(bool show);

        // CRTP methods
        void create_impl(GLFWwindow* window);
        void destroy_impl();
        void render_impl(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
            pred_callback_type predicate,
            void* predicate_userdata);
    };
}