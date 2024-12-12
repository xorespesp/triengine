#include "skeleton_renderer.hh"
#include "../misc/debug_utils.hh"
#include "../math.hh"

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
        const mat4_f32& view,
        const mat4_f32& projection,
        const lighting_options& light_opts)
    {
        for (const auto& object : this->get_objects()) {
            _mesh_renderer.render(object->joint_objects, view, projection, light_opts);
            _mesh_renderer.render(object->bone_objects, view, projection, light_opts);
        }
    }

} // namespace
