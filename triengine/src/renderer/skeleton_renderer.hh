#pragma once
#include "renderer_base.hh"
#include "../geometry/skeleton_object.hh"

#include "triangle_mesh_renderer.hh"

namespace triengine::renderer
{
    class skeleton_renderer
        : public object_renderer_base<geometry::skeleton_object>
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

        void create(GLFWwindow* window) override;
        void destroy() override;
        void render(const render_context& render_ctx) override;
    };
}