#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/renderer/phong_shaded_object_block.hh>
#include <triengine/geometry/pcd_object.hh>

#include <cstdint>
#include <optional>
#include <vector>

namespace triengine::renderer
{
    class pcd_renderer
        : public renderer_base<pcd_renderer>
    {
    private:
        // Render options
        std::optional<float> _point_size;

        // Renderer resources
        core::gl_context* _glctx{ nullptr };
        core::shader_program 
            _deferred_opaque_pass_shader, 
            _forward_trans_pass_shader;

        // Per-object records of each draw loop, one vector per shader program
        std::optional<phong_shaded_object_storage_vector>
            _deferred_opaque_pass_records,
            _forward_trans_pass_records;

        // A draw collected by a draw loop, after its record is written
        struct draw_item {
            uint32_t vao;
            int32_t vertex_count;
            uint32_t record_index; // `baseinstance` of the draw
        };
        std::vector<draw_item> _draw_items; // reused by every draw loop

    public:
        pcd_renderer();
        ~pcd_renderer();

        void set_pcd_point_size(float point_size);

        // CRTP methods
        void create_impl(core::gl_context& glctx);
        void destroy_impl();
        void render_impl(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<geometry::pcd_object>>& render_obj_list,
            pred_callback_type<geometry::pcd_object> predicate = nullptr,
            void* predicate_userdata = nullptr);

    };

} // namespace