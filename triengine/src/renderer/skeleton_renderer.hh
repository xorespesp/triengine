#pragma once
#include "renderer_base.hh"
#include "../geometry/skeleton_object.hh"

#include "triangle_mesh_renderer.hh"

namespace triengine::renderer
{
    class skeleton_renderer
        : public renderer_base<geometry::skeleton_object>
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
        void render(
            const mat4_f32& view,
            const mat4_f32& projection,
            const lighting_options& light_opts
        ) override;
    };
}