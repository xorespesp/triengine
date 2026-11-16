#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/renderer/object_model_block.hh>
#include <triengine/geometry/lineset_object.hh>

#include <cstdint>
#include <optional>
#include <vector>

namespace triengine::renderer
{
    class lineset_renderer
        : public renderer_base<lineset_renderer>
    {
    private:
        // Renderer resources
        core::gl_context* _glctx{ nullptr };
        core::shader_program _shader;

        // Model matrices of the draw loop
        std::optional<object_model_storage_vector> _object_models;

        // A draw collected by the draw loop, after its model matrix is written
        struct draw_item {
            uint32_t vao;
            int32_t index_count;
            uint32_t model_index; // `baseinstance` of the draw
        };
        std::vector<draw_item> _draw_items;

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