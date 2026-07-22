#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/geometry/lineset_object.hh>

namespace triengine::renderer
{
    class lineset_renderer
        : public renderer_base<lineset_renderer>
    {
    private:
        // Renderer resources
        core::gl_context* _glctx{ nullptr };
        core::shader_program _shader;

    public:
        lineset_renderer();
        ~lineset_renderer();

        // CRTP methods
        void create_impl(core::gl_context& glctx);
        void destroy_impl();
        void render_impl(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::lineset_object>>& render_obj_list,
            pred_callback_type<geometry::lineset_object> predicate = nullptr,
            void* predicate_userdata = nullptr);

    };

} // namespace