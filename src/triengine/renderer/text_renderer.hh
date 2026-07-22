#pragma once
#include <triengine/renderer/renderer_base.hh>
#include <triengine/text_render_options.hh>
#include <triengine/text_object.hh>

namespace triengine::renderer
{
    class text_renderer
        : public renderer_base<text_renderer>
    {
	private:
		struct renderer_context_t;
		struct renderer_context_deleter {
			void operator()(renderer_context_t* p) const;
		}; // https://stackoverflow.com/a/32269374
		using  renderer_context_unique_ptr = std::unique_ptr<renderer_context_t, renderer_context_deleter>;

		renderer_context_unique_ptr _ctx;

    public:
        text_renderer();
        ~text_renderer();

        // CRTP methods
        void create_impl(
			core::gl_context& glctx,
			const uint8_t* ttf_file_buff,
			size_t ttf_file_buff_size,
			uint32_t font_size
		);

        void destroy_impl();

        void render_impl(
            const render_context& render_ctx,
            const text_render_options& render_opts,
			GLuint depth_tex_id,
            const std::list<std::shared_ptr<text_2d_object>>& text_2d_objects,
            const std::list<std::shared_ptr<text_3d_object>>& text_3d_objects
		);

	private:
		void _draw_text(
			std::string_view text,
			vec2_f32 viewport_space_pos,
			float scale,
			text_alignment_type text_align
		);

		vec2_f32 _measure_text_size(
			std::string_view text,
			float scale
		) const;

    }; // class

} // namespace