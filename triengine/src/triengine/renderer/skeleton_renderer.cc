#include "skeleton_renderer.hh"

#include <triengine/utility/debug_utils.hh>

namespace triengine::renderer
{
    skeleton_renderer::skeleton_renderer()
    { }

    void skeleton_renderer::show_joint_axis(bool show)
    {
        _flag_show_joint_axis = show;
    }

    void skeleton_renderer::create_impl(core::gl_context& glctx)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        _mesh_renderer.create(glctx);
    }

    void skeleton_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            _mesh_renderer.destroy();
        }
    }

    void skeleton_renderer::render_impl(
        const render_context& render_ctx,
        const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
        pred_callback_type const predicate,
        void* const predicate_userdata)
    {
        for (const auto& object : render_obj_list)
        {
            if (!object->is_visible()) {
                continue;
            }

            if (predicate && !predicate(*object, predicate_userdata)) {
                continue;
            }

            _mesh_renderer.render(render_ctx, object->get_joint_objects());
            _mesh_renderer.render(render_ctx, object->get_bone_objects());
        }
    }

} // namespace
