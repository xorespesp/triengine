#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/geometry/pcd_object.hh>

#include <optional>

namespace triengine::renderer
{
    class pcd_renderer
        : public object_renderer_base<pcd_renderer, geometry::pcd_object>
    {
    private:
        // Render options
        std::optional<float> _point_size;

        // OpenGL resources
        core::gl_context* _glctx{ nullptr };
        core::shader_program 
            _solid_shader, 
            _transparent_shader;

    public:
        pcd_renderer();

        void set_pcd_point_size(float point_size);

        // CRTP methods
        void create_impl(core::gl_context& glctx, const core::shader_preprocessor& shader_prep);
        void destroy_impl();
        void render_impl(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
            pred_callback_type predicate,
            void* predicate_userdata);

    };

} // namespace