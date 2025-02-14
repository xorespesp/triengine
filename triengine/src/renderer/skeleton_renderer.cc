#include "skeleton_renderer.hh"
#include "../misc/debug_utils.hh"

namespace triengine::renderer
{
    skeleton_renderer::skeleton_renderer()
    { }

    skeleton_renderer::~skeleton_renderer()
    {
        this->destroy();
    }

    void skeleton_renderer::show_joint_axis(bool show)
    {
        _flag_show_joint_axis = show;
    }

    void skeleton_renderer::create(GLFWwindow* window)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        _mesh_renderer.create(window);
    }

    void skeleton_renderer::destroy()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            _mesh_renderer.destroy();
        }
    }

    void skeleton_renderer::render(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<render_object_type>>& render_obj_list)
    {
        for (const auto& render_obj : render_obj_list) {
            _mesh_renderer.render(render_ctx, render_obj->joint_objects);
            _mesh_renderer.render(render_ctx, render_obj->bone_objects);
        }
    }

} // namespace
