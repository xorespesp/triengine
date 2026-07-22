#include "text_renderer.hh"

#include <triengine/camera.hh>

#include <fstream>
#include <unordered_map>
#include <array>

#include <ft2build.h>
#include FT_FREETYPE_H

#define FT_SUCCEEDED(X) ((X) == FT_Err_Ok)
#define FT_FAILED(X)    ((X) != FT_Err_Ok)

namespace triengine::renderer
{
    namespace detail
    {
		struct scoped_gl_blend {
			scoped_gl_blend(GLenum src, GLenum dst) {
				_prev_enabled = ::glIsEnabled(GL_BLEND);
				::glGetIntegerv(GL_BLEND_SRC_RGB, &_prev_src_rgb);
				::glGetIntegerv(GL_BLEND_DST_RGB, &_prev_dst_rgb);
				::glGetIntegerv(GL_BLEND_SRC_ALPHA, &_prev_src_alpha);
				::glGetIntegerv(GL_BLEND_DST_ALPHA, &_prev_dst_alpha);
				if (!_prev_enabled) { ::glEnable(GL_BLEND); }
				::glBlendFunc(src, dst);
			}

			~scoped_gl_blend() {
				if (!_prev_enabled) { ::glDisable(GL_BLEND); }
				::glBlendFuncSeparate(
					_prev_src_rgb,
					_prev_dst_rgb,
					_prev_src_alpha,
					_prev_dst_alpha
				);
			}

			scoped_gl_blend(const scoped_gl_blend&) = delete;
			scoped_gl_blend& operator=(const scoped_gl_blend&) = delete;

		private:
			GLboolean _prev_enabled{ GL_FALSE };
			GLint _prev_src_rgb{}, _prev_dst_rgb{};
			GLint _prev_src_alpha{}, _prev_dst_alpha{};
		};

		struct scoped_gl_unpack_alignment {
			explicit scoped_gl_unpack_alignment(GLint new_alignment) {
				::glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_alignment_);
				::glPixelStorei(GL_UNPACK_ALIGNMENT, new_alignment);
			}

			~scoped_gl_unpack_alignment() {
				::glPixelStorei(GL_UNPACK_ALIGNMENT, prev_alignment_);
			}

			scoped_gl_unpack_alignment(const scoped_gl_unpack_alignment&) = delete;
			scoped_gl_unpack_alignment& operator=(const scoped_gl_unpack_alignment&) = delete;

		private:
			GLint prev_alignment_{};
		};

		/// Holds all state information relevant to a character as loaded using FreeType
		struct font_glyph_metrics_t
		{
			FT_ULong char_code{}; // Unicode code point of the character
			vec2_u32 size{}; // Size of glyph (Unit: [pixel])
			vec2_i32 bearing{}; // Offset from baseline to left/top of glyph (Unit: [pixel])
			int32_t advance_x{}; // Horizontal offset to advance to next glyph (Unit: [1/64 pixel])
			std::vector<uint8_t> bitmap_data{}; // Raw bitmap data for the glyph (1 byte per pixel, grayscale)
			vec2_u32 start_offset{}; // Start position in the atlas texture (Unit: [pixel])
			vec4_f32 uv_rect{}; // UV rect(.x = u_min, .y = v_min, .z = width, .w = height) in atlas texture (Unit: [0.0 ... 1.0])
		};

		/// Manages a font atlas texture created from a font file using FreeType.
		class font_atlas_texture
		{
		public:
			font_atlas_texture() = default;
			~font_atlas_texture() { this->reset(); }
			font_atlas_texture(const font_atlas_texture&) = delete;
			font_atlas_texture& operator=(const font_atlas_texture&) = delete;
			font_atlas_texture(font_atlas_texture&& rhs) noexcept { *this = std::move(rhs); }
			font_atlas_texture& operator=(font_atlas_texture&& rhs) noexcept {
				if (this != &rhs) {
					this->reset();
					std::swap(_tex_id, rhs._tex_id);
					std::swap(_tex_size, rhs._tex_size);
					std::swap(_glyph_info_map, rhs._glyph_info_map);
					std::swap(_max_glyph_size, rhs._max_glyph_size);
				}
				return *this;
			}

			[[nodiscard]]
			static std::shared_ptr<font_atlas_texture> create_from_ttf_file(
				const std::filesystem::path& ttf_file_path,
				const uint32_t font_size_in_pixels)
			{
				if (!std::filesystem::is_regular_file(ttf_file_path)) {
					TRIENGINE_ERROR("Invalid ttf file path");
					return nullptr;
				}

				std::vector<uint8_t> file_buff;
				std::ifstream f{ ttf_file_path, std::ios::binary };
				if (!f) {
					TRIENGINE_ERROR("Failed to open ttf file");
					return nullptr;
				}

				f.seekg(0, std::ios::end);
				const size_t file_size = static_cast<size_t>(f.tellg());
				if (!file_size) {
					TRIENGINE_ERROR("Empty ttf file");
					return nullptr;
				}
				f.seekg(0, std::ios::beg);

				file_buff.resize(file_size);
				f.read(reinterpret_cast<char*>(file_buff.data()), static_cast<std::streamsize>(file_size));

				return create_from_ttf_memory(
					file_buff.data(),
					file_buff.size(),
					font_size_in_pixels
				);
			}

			[[nodiscard]]
			static std::shared_ptr<font_atlas_texture> create_from_ttf_memory(
				const uint8_t* const ttf_file_buff,
				const size_t ttf_file_buff_size,
				const uint32_t font_size_in_pixels)
			{
				if (!ttf_file_buff || !ttf_file_buff_size) {
					TRIENGINE_ERROR("Invalid ttf file buffer");
					return nullptr;
				}

				std::shared_ptr<std::remove_pointer_t<FT_Library>> ft;
				if (FT_Library ft_; FT_SUCCEEDED(::FT_Init_FreeType(&ft_))) {
					ft.reset(ft_, ::FT_Done_FreeType);
				} else {
					TRIENGINE_ERROR("Failed to init freetype");
					return nullptr;
				}

				// load font as face
				std::shared_ptr<std::remove_pointer_t<FT_Face>> ft_face;
				if (FT_Face face_; FT_SUCCEEDED(::FT_New_Memory_Face(ft.get(),
					ttf_file_buff,
					static_cast<FT_Long>(ttf_file_buff_size),
					0,
					&face_))) {
					ft_face.reset(face_, ::FT_Done_Face);
				} else {
					TRIENGINE_ERROR("Failed to create font face from memory");
					return nullptr;
				}

				// set size to load glyphs as
				::FT_Set_Pixel_Sizes(ft_face.get(),
					0/* pixel_width; pass 0 to dynamically calculate based on height */,
					font_size_in_pixels/* pixel_height */
				);

				// ---------------------------------------------------------
				// first pass: Load glyphs metrics and calculate atlas texture size
				// ---------------------------------------------------------
				std::unordered_map<FT_ULong/* char code */, font_glyph_metrics_t> new_glyph_info_map;
				vec2_u32 max_glyph_size{};

				constexpr uint32_t kNumChars = 128;
				constexpr uint32_t kAtlasGlyphBitmapPadding = 1; // A pixel padding between glyphs in the atlas texture
				constexpr uint32_t kMaxAtlasTextureWidth = 1024; // A maximum width of the atlas texture

				uint32_t curr_x = kAtlasGlyphBitmapPadding;
				uint32_t curr_y = kAtlasGlyphBitmapPadding;
				uint32_t max_row_width = 0;
				uint32_t max_row_height = 0;

				for (FT_ULong char_code = 0; char_code < kNumChars; ++char_code)
				{
					if (FT_FAILED(::FT_Load_Char(ft_face.get(), char_code, FT_LOAD_RENDER))) {
						TRIENGINE_WARN("Failed to load char code 0x%X from font face", char_code);
						continue;
					}

					const vec2_u32 glyph_size{ ft_face->glyph->bitmap.width, ft_face->glyph->bitmap.rows };
					const vec2_i32 glyph_bearing{ ft_face->glyph->bitmap_left, ft_face->glyph->bitmap_top };

					// Calculate max glyph size (for line height calculation)
					if (glyph_size.x() > max_glyph_size.x()) { max_glyph_size.x() = glyph_size.x(); }
					if (glyph_size.y() > max_glyph_size.y()) { max_glyph_size.y() = glyph_size.y(); }

					// Check if we need to move to the next row in the atlas texture
					if (curr_x + glyph_size.x() + kAtlasGlyphBitmapPadding > kMaxAtlasTextureWidth) {
						curr_x = kAtlasGlyphBitmapPadding;
						curr_y += max_row_height + kAtlasGlyphBitmapPadding;
						max_row_height = 0;
					}

					// Cache glyph metrics for use in second pass
					font_glyph_metrics_t& new_glyph_info = new_glyph_info_map[char_code];
					new_glyph_info.char_code = char_code;
					new_glyph_info.size = glyph_size;
					new_glyph_info.bearing = glyph_bearing;
					new_glyph_info.advance_x = ft_face->glyph->advance.x;
					if (glyph_size.x() > 0 && glyph_size.y() > 0) {
						new_glyph_info.bitmap_data.resize(glyph_size.x() * glyph_size.y());
						std::copy_n(
							ft_face->glyph->bitmap.buffer,
							new_glyph_info.bitmap_data.size(),
							new_glyph_info.bitmap_data.data()
						);
					} else {
						TRIENGINE_WARN("Loaded char code 0x%X has no bitmap data", char_code);
					}
					new_glyph_info.start_offset = vec2_u32{ curr_x, curr_y };
					// NOTE: `new_glyph_info.uv_rect` is calculated in second pass

					curr_x += glyph_size.x() + kAtlasGlyphBitmapPadding;
					max_row_width = std::max(max_row_width, curr_x);
					max_row_height = std::max(max_row_height, glyph_size.y());
				} // for

				// Calculate final atlas texture size
				const vec2_u32 new_tex_size{
					max_row_width,
					curr_y + max_row_height + kAtlasGlyphBitmapPadding
				};

				// ---------------------------------------------------------
				// second pass: Create atlas texture and upload glyph bitmaps
				// ---------------------------------------------------------

				GLuint new_tex_id{};
				::glCreateTextures(GL_TEXTURE_2D, 1, &new_tex_id);
				::glTextureStorage2D(new_tex_id,
					1,              // mipmap levels
					GL_R8,          // internal format
					new_tex_size.x(), // width
					new_tex_size.y()  // height
				);

				::glTextureParameteri(new_tex_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				::glTextureParameteri(new_tex_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				::glTextureParameteri(new_tex_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				::glTextureParameteri(new_tex_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

				// Since FreeType produces 1-byte aligned bitmap data, set unpack alignment to 1
				::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

				for (auto& [char_code, glyph_info] : new_glyph_info_map)
				{
					const uint32_t xoffset = glyph_info.start_offset.x();
					const uint32_t yoffset = glyph_info.start_offset.y();
					const uint32_t width = glyph_info.size.x();
					const uint32_t height = glyph_info.size.y();

					const bool has_bitmap_data = (width > 0 && height > 0 && !glyph_info.bitmap_data.empty());
					if (has_bitmap_data) {
						// Upload glyph bitmap data to the atlas texture, at the pre-calculated offset
						::glTextureSubImage2D(new_tex_id,
							0,                // mip level
							xoffset,          // xoffset
							yoffset,          // yoffset
							width,            // width
							height,           // height
							GL_RED,           // format
							GL_UNSIGNED_BYTE, // type
							glyph_info.bitmap_data.data()
						);
					}

					// Calculate UV rect in atlas texture for the glyph (Unit: [0.0 ... 1.0])
					const float u_min = static_cast<float>(xoffset) / new_tex_size.x();
					const float v_min = static_cast<float>(yoffset) / new_tex_size.y();
					const float u_width = static_cast<float>(width) / new_tex_size.x();
					const float v_height = static_cast<float>(height) / new_tex_size.y();
					glyph_info.uv_rect = vec4_f32{ u_min, v_min, u_width, v_height };
				} // for

				auto result = std::make_shared<font_atlas_texture>();
				result->_tex_id = new_tex_id;
				result->_tex_size = new_tex_size;
				result->_max_glyph_size = max_glyph_size;
				result->_glyph_info_map = std::move(new_glyph_info_map);

				TRIENGINE_DEBUG("Font loaded successfully! (%zu charset, atlas texture size: %ux%u, max glyph size: %ux%u"
					, result->_glyph_info_map.size()
					, result->_tex_size.x(), result->_tex_size.y()
					, result->_max_glyph_size.x(), result->_max_glyph_size.y()
				);

				return result;
			}

			GLuint texture_id() const noexcept { return _tex_id; }
			const vec2_u32 texture_size() const noexcept { return _tex_size; }
			const vec2_u32 max_glyph_size() const noexcept { return _max_glyph_size; }

			const font_glyph_metrics_t* find_glyph_metrics(const FT_ULong char_code) const noexcept {
				const auto it = _glyph_info_map.find(char_code);
				return (it != _glyph_info_map.end()) ? &it->second : nullptr;
			}

			void reset() {
				if (_tex_id) {
					::glDeleteTextures(1, &_tex_id);
					_tex_id = 0;
				}
				_tex_size = {};
				_glyph_info_map.clear();
				_max_glyph_size = {};
			}

		private:
			GLuint _tex_id{}; // A large 2d texture that holds the font glyph bitmaps as packed 2D array
			vec2_u32 _tex_size{}; // Size of the atlas texture (Unit: [pixel])
			std::unordered_map<FT_ULong/* char code */, font_glyph_metrics_t> _glyph_info_map;
			vec2_u32 _max_glyph_size{}; // Max size of the glyph bitmap (Unit: [pixel])
		};

		/// Manages a SSBO queue for text rendering.
		class text_render_ssbo_queue {
		public:
			// The minimum value for the SSBO size, which is guaranteed by the specification, is 2^27 (128MB).
			// See: https://wikis.khronos.org/opengl/Shader_Storage_Buffer_Object
			static constexpr size_t kDefaultQueueSize = 1024; // NOTE: This must be same as the value defined in the text shader!!
			static constexpr size_t kQueueElementSize = sizeof(mat4_f32) + sizeof(vec4_f32);

		public:
			text_render_ssbo_queue() {
				const size_t max_supported_q_size = []() -> size_t {
					// Get the maximum SSBO size supported by the system
					GLint64 max_supported_ssbo_size{};
					::glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &max_supported_ssbo_size);

					return static_cast<size_t>(max_supported_ssbo_size) / kQueueElementSize;
				}();

				if (max_supported_q_size < kDefaultQueueSize) {
					throw std::runtime_error("Requested SSBO queue size exceeds the maximum supported size");
				}

				_q_size = kDefaultQueueSize;
				_curr_enqueued_size = 0;
				_glyph_quad_models_q_buffer.resize(_q_size);
				_glyph_tex_uv_rects_q_buffer.resize(_q_size);

				::glCreateBuffers(1, &_ssbo_id);

				// Allocate SSBO storage
				const size_t allocated_ssbo_size = kQueueElementSize * _q_size;
				::glNamedBufferStorage(_ssbo_id,
					static_cast<GLsizeiptr>(allocated_ssbo_size),
					nullptr,
					GL_DYNAMIC_STORAGE_BIT // allow dynamic updates
				);

				TRIENGINE_DEBUG("Text Render SSBO created. (queue size %zu, allocated %zu bytes)"
					, _q_size
					, allocated_ssbo_size
				);
			}

			~text_render_ssbo_queue() { this->reset(); }
			text_render_ssbo_queue(const text_render_ssbo_queue&) = delete;
			text_render_ssbo_queue& operator=(const text_render_ssbo_queue&) = delete;
			text_render_ssbo_queue(text_render_ssbo_queue&& rhs) noexcept { *this = std::move(rhs); }
			text_render_ssbo_queue& operator=(text_render_ssbo_queue&& rhs) noexcept {
				if (this != &rhs) {
					this->reset();
					std::swap(_q_size, rhs._q_size);
					std::swap(_curr_enqueued_size, rhs._curr_enqueued_size);
					std::swap(_glyph_quad_models_q_buffer, rhs._glyph_quad_models_q_buffer);
					std::swap(_glyph_tex_uv_rects_q_buffer, rhs._glyph_tex_uv_rects_q_buffer);
					std::swap(_ssbo_id, rhs._ssbo_id);
				}
				return *this;
			}

			GLuint ssbo_id() const noexcept { return _ssbo_id; }
			size_t max_size() const noexcept { return _q_size; }
			size_t size() const noexcept { return _curr_enqueued_size; }
			bool is_full() const noexcept { return this->size() >= this->max_size(); }
			bool is_empty() const noexcept { return this->size() == 0; }

			void enqueue_data(
				const mat4_f32& glyph_quad_model,
				const vec4_f32& glyph_tex_uv_rect) {
				if (this->is_full()) {
					throw std::runtime_error("SSBO index out of bounds");
				}

				_glyph_quad_models_q_buffer[_curr_enqueued_size] = glyph_quad_model;
				_glyph_tex_uv_rects_q_buffer[_curr_enqueued_size] = glyph_tex_uv_rect;

				++_curr_enqueued_size;
			}

			size_t flush() {
				const size_t flush_count = _curr_enqueued_size;

				if (flush_count > 0) {
					// Upload matrices to SSBO
					::glNamedBufferSubData(_ssbo_id,
						0, // offset
						sizeof(mat4_f32) * flush_count, // size
						_glyph_quad_models_q_buffer.data() // data ptr
					);

					// Upload UV rects to SSBO
					::glNamedBufferSubData(_ssbo_id,
						sizeof(mat4_f32) * _q_size, // offset
						sizeof(vec4_f32) * flush_count, // size
						_glyph_tex_uv_rects_q_buffer.data() // data ptr
					);

					_curr_enqueued_size = 0; // reset index
				}

				return flush_count;
			}

			void bind(GLuint binding_point = 0) const {
				::glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding_point, _ssbo_id);
			}

		private:
			void reset() {
				_q_size = 0;
				_curr_enqueued_size = 0;
				_glyph_quad_models_q_buffer.clear();
				_glyph_tex_uv_rects_q_buffer.clear();
				if (_ssbo_id) {
					::glDeleteBuffers(1, &_ssbo_id);
					_ssbo_id = 0;
				}
			}

		private:
			size_t _q_size{};
			size_t _curr_enqueued_size{}; // current working index (instance count)
			std::vector<mat4_f32> _glyph_quad_models_q_buffer;
			std::vector<vec4_f32> _glyph_tex_uv_rects_q_buffer;
			GLuint _ssbo_id{};
		}; // class

		[[nodiscard]] bool project_to_ndc_space(
			const mat4_f32& view,
			const mat4_f32& proj,
			const vec3_f32& target_world_pos,
			vec3_f32& ndc_pos_out/* out */)
		{
			// world space -> view space -> clip space
			const vec4_f32 clip_pos = proj * view * target_world_pos.homogeneous();
			if (clip_pos.w() <= 0.0f) {
				return false; // The target point is behind the camera or outside the view frustum
			}

			// clip space -> ndc space (perspective divition)
			const vec3_f32 ndc_pos = clip_pos.hnormalized(); //clip_pos.head<3>() / clip_pos.w();
			if (ndc_pos.x() < -1.0f || ndc_pos.x() > 1.0f ||
				ndc_pos.y() < -1.0f || ndc_pos.y() > 1.0f ||
				ndc_pos.z() < -1.0f || ndc_pos.z() > 1.0f) {
				return false; // The target point is outside the view frustum boundary
			}

			ndc_pos_out = ndc_pos;
			return true;
		}

		[[nodiscard]] bool project_to_viewport_space(
			const view_port& viewport,
			const mat4_f32& view,
			const mat4_f32& proj,
			const vec3_f32& target_world_pos,
			vec2_f32& projected_viewport_pos/* out */)
		{
			vec3_f32 ndc_pos{};
			if (!project_to_ndc_space(view, proj, target_world_pos, ndc_pos)) {
				return false; // The target point is outside the view frustum
			}

			// ndc space [-1, 1] -> screen space [0, viewport_dimension] (OpenGL convention)
			// Ref: https://www.gamedev.net/forums/topic/685104-ndc-to-pixel-space/
			//      https://msdn.microsoft.com/en-us/library/windows/desktop/bb205126(v=vs.85).aspx
			projected_viewport_pos.x() = ((ndc_pos.x() + 1.0f) * 0.5f * static_cast<float>(viewport.width)) + viewport.x;
			projected_viewport_pos.y() = ((ndc_pos.y() + 1.0f) * 0.5f * static_cast<float>(viewport.height)) + viewport.y;
			return true;
		}

    } // namespace

    struct text_renderer::renderer_context_t final
    {
        std::shared_ptr<detail::font_atlas_texture> font_atlas_texture;
        std::shared_ptr<detail::text_render_ssbo_queue> text_render_ssbo_q;
        mat4_f32 glyph_quad_proj{};
        GLuint glyph_quad_vao{}, glyph_quad_vbo{};

        core::shader_program text_shader;
        core::shader_program text_depth_test_shader;

        std::unordered_map<text_object_id_type, std::shared_ptr<text_3d_object>> label_3d_map;
        std::unordered_map<text_object_id_type, std::shared_ptr<text_2d_object>> label_2d_map;

        float line_spacing_factor{ 1.0f }; // line spacing factor (multiplier of font size)

        renderer_context_t() = default;
        ~renderer_context_t() {
            if (glyph_quad_vao) {
                ::glDeleteVertexArrays(1, &glyph_quad_vao);
                glyph_quad_vao = 0;
            }
            if (glyph_quad_vbo) {
                ::glDeleteBuffers(1, &glyph_quad_vbo);
                glyph_quad_vbo = 0;
            }
        }
    };

    void text_renderer::renderer_context_deleter::operator()(text_renderer::renderer_context_t* p) const
    {
        delete p;
    }

    text_renderer::text_renderer()
    { }

    text_renderer::~text_renderer()
    {
        this->destroy_impl();
    }

    void text_renderer::create_impl(
        core::gl_context& glctx,
		const uint8_t* const ttf_file_buff,
		const size_t ttf_file_buff_size,
        const uint32_t font_size)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        glctx.make_context_current();

        const auto shader_ldr = glctx.get_shader_loader();

        //
        // Context Settings
        //

        auto new_ctx = renderer_context_unique_ptr{ new renderer_context_t{} };
        
        new_ctx->font_atlas_texture = detail::font_atlas_texture::create_from_ttf_memory(
            ttf_file_buff,
			ttf_file_buff_size,
            font_size
        );

        if (!new_ctx->font_atlas_texture) {
            TRIENGINE_PANIC("Failed to create font atlas texture");
        }

        new_ctx->text_render_ssbo_q = std::make_shared<detail::text_render_ssbo_queue>();

        /**
         * (0,1)        (1,1)
         *   +-----------+
         *   |         + |
         *   |       +   |
         *   |     +     |
         *   |   +       |
         *   | +         |
         *   +-----------+
         * (0,0)       (1,0)
         *
         * GL_TRIANGLE_STRIP order: (0,1) -> (0,0) -> (1,1) -> (1,0)
         */
        constexpr GLfloat kLocalGlyphQuadVertices[] = {
            0.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 1.0f,
            1.0f, 0.0f,
        };

        // setup VBO
        ::glCreateBuffers(1, &new_ctx->glyph_quad_vbo);
        ::glNamedBufferStorage(new_ctx->glyph_quad_vbo,
            sizeof(float) * 2 * 4,
            kLocalGlyphQuadVertices,
            0/*flags*/
        );

        // setup VAO
        ::glCreateVertexArrays(1, &new_ctx->glyph_quad_vao);
        ::glVertexArrayVertexBuffer(new_ctx->glyph_quad_vao, 0/*bindingindex*/, new_ctx->glyph_quad_vbo, 0/*start offset*/, sizeof(float) * 2/*stride*/);

        // attribute 0 (location = 0) -> `layout (location = 0) in vec2 vi_glyphQuadVertPos`
        ::glEnableVertexArrayAttrib(new_ctx->glyph_quad_vao, 0/*attribindex*/);
        ::glVertexArrayAttribFormat(new_ctx->glyph_quad_vao, 0/*attribindex*/, 2/*size*/, GL_FLOAT/*type*/, GL_FALSE/*normalized*/, 0/*offset*/);
        ::glVertexArrayAttribBinding(new_ctx->glyph_quad_vao, 0/*attribindex*/, 0/*bindingindex*/);

        // attribute 1 (location = 1) -> `layout (location = 1) in vec2 vi_glyphQuadTexCoord`
        ::glEnableVertexArrayAttrib(new_ctx->glyph_quad_vao, 1);
        ::glVertexArrayAttribFormat(new_ctx->glyph_quad_vao, 1/*attribindex*/, 2/*size*/, GL_FLOAT, GL_FALSE, 0/*offset*/);
        ::glVertexArrayAttribBinding(new_ctx->glyph_quad_vao, 1/*attribindex*/, 0/*bindingindex*/);

        // Create shader program
        new_ctx->text_shader
            .attach_vertex_shader({ shader_ldr->load("text.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("text.frag")->c_str() })
            .link();

        new_ctx->text_depth_test_shader
            .attach_vertex_shader({ shader_ldr->load("text_depth_test.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("text_depth_test.frag")->c_str() })
            .link();

        _ctx = std::move(new_ctx);
    }

    void text_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            _ctx.reset();
        }
    }

    void text_renderer::render_impl(
        const render_context& render_ctx,
        const text_render_options& render_opts,
		const GLuint depth_tex_id,
		const std::list<std::shared_ptr<text_2d_object>>& text_2d_objects,
		const std::list<std::shared_ptr<text_3d_object>>& text_3d_objects)
    {
		TRIENGINE_ASSERT(this->is_created());

		const view_port& viewport = render_ctx.camera->get_viewport();
		const mat4_f32& view = render_ctx.view;
		const mat4_f32& proj = render_ctx.projection;
		const float camera_near = camera_constants::kNearPlane;
		const float camera_far = camera_constants::kFarPlane;

		// activate corresponding render state
		detail::scoped_gl_blend blend_guard{ GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA };

		// [IMPORTANT]
		// Disable depth testing for all text rendering cases (both 2D and 3D).
		// For 3D text rendering, depth testing is handled manually at the application level.
		// Enabling OpenGL's depth testing may cause conflicts with the manual logic,
		// which can result in flickering.
		::glDisable(GL_DEPTH_TEST);

		// bind glyph quad VAO
		::glBindVertexArray(_ctx->glyph_quad_vao);

		// bind font atlas texture
		::glBindTextureUnit(0, _ctx->font_atlas_texture->texture_id());

		// bind SSBO block to binding point 0
		_ctx->text_render_ssbo_q->bind(0);

		// Update orthographic projection matrix for glyph quad
		// (Consider viewport position and dimensions)
		_ctx->glyph_quad_proj = math::ortho(
			static_cast<float>(viewport.x), // left
			static_cast<float>(viewport.x + viewport.width), // right
			static_cast<float>(viewport.y), // bottom
			static_cast<float>(viewport.y + viewport.height) // top
		);

		// --- Render 3D labels ---
		if (!text_3d_objects.empty())
		{
			const bool perform_depth_testing = (depth_tex_id != 0 && render_opts.depth_test_opts.enabled);

			auto& active_text_shader = (perform_depth_testing)
				? _ctx->text_depth_test_shader
				: _ctx->text_shader;

			active_text_shader.use();
			active_text_shader.set_uniform_mat4("u_glyphQuadProj", _ctx->glyph_quad_proj);
			if (perform_depth_testing) {
				active_text_shader.set_uniform_mat4("u_cameraProj", proj);
				active_text_shader.set_uniform_float("u_cameraNear", camera_near);
				active_text_shader.set_uniform_float("u_cameraFar", camera_far);
				active_text_shader.set_uniform_bool("u_isOrtho", render_ctx.camera->is_ortho());
				active_text_shader.set_uniform_float("u_depthBias", render_opts.depth_test_opts.depth_bias);
				active_text_shader.set_uniform_float("u_occlusionAlpha", render_opts.depth_test_opts.occlusion_alpha);

				// Bind depth texture
				::glBindTextureUnit(1, depth_tex_id);
			}

			for (const auto& label : text_3d_objects)
			{
				if (!label->is_visible() || label->is_empty()) {
					continue;
				}

				active_text_shader.set_uniform_vec3("u_textColor", label->get_color().to_eigen());

				const vec3_f32& world_space_pos = label->get_position();

				vec2_f32 projected_viewport_pos{};
				if (!detail::project_to_viewport_space(
					viewport,
					view,
					proj,
					world_space_pos,
					projected_viewport_pos
				)) {
					continue; // The label is outside the view frustum
				}

				// Transform label position to view space (camera is at origin in view space)
				const vec4_f32 view_space_pos = view * world_space_pos.homogeneous().eval();
				if (perform_depth_testing) {
					// Set 3D world position for depth testing
					active_text_shader.set_uniform_vec3("u_textViewSpacePos", view_space_pos.head<3>());
				}

				// Calculate distance-based scale factor
				float distance_scale_factor = 1.0f;
				if (render_opts.dist_scale_opts.enabled)
				{
					// Eye-to-label distance in view space, shared by both distance-scale modes.
					const float distance = [&view_space_pos, &render_ctx]() -> float {
						const float raw_distance = view_space_pos.head<3>().norm();

						// [Orthographic camera]
						// In ortho the eye stays at a fixed distance from the pivot and scroll-zoom
						// changes the view height instead of moving the eye, so the raw eye-to-label
						// distance does not change when zooming. Scale it by the zoom factor (current
						// view height relative to the default) so that BOTH distance-scale modes
						// (fade_out and perspective) respond to zoom the same way they do under a
						// perspective camera. At the default view height the factor is 1, so the
						// per-label distances (and the perspective mode's near-big/far-small cue) are
						// preserved.
						if (render_ctx.camera->is_ortho()) {
							const float view_height = render_ctx.camera->as<ortho_camera>()->get_ortho_view_height();
							return raw_distance * (view_height / camera_constants::kDefaultOrthoViewHeight);
						}

						return raw_distance;
					}();

					switch (render_opts.dist_scale_opts.scale_mode) {
					case text_render_options::dist_scale_mode_type::fade_out:
					{
						// === FADE OUT SCALING ===
						// 선형 보간을 통한 페이드 아웃
						// - ref_distance 이내: scale = 1.0 (완전한 크기 유지)
						// - ref_distance ~ max_distance: 선형적으로 1.0 → 0.0 으로 페이드
						// - max_distance 이상: scale = 0.0 (완전히 사라짐)
						// 
						// 공식: scale = 1.0 - smoothstep((distance - ref_dist) / (max_dist - ref_dist))

						if (distance <= render_opts.dist_scale_opts.ref_distance)
						{
							distance_scale_factor = 1.0f;  // 기준 거리 이내에서는 완전한 크기 유지
						}
						else if (distance >= render_opts.dist_scale_opts.max_distance)
						{
							distance_scale_factor = 0.0f;  // 최대 거리 이상에서는 완전히 사라짐
						}
						else
						{
							// 부드러운 페이드 아웃: smoothstep 함수 사용
							const float distance_range = render_opts.dist_scale_opts.max_distance - render_opts.dist_scale_opts.ref_distance;
							const float normalized_distance = (distance - render_opts.dist_scale_opts.ref_distance) / distance_range;

							// Hermite 보간 (smoothstep): f(t) = 3t^2 - 2t^3
							// 0에서 1로 부드럽게 변화하는 S-curve 생성
							const float smooth_factor = 1.0f - (normalized_distance * normalized_distance * (3.0f - 2.0f * normalized_distance));
							distance_scale_factor = std::max(0.0f, smooth_factor);
						}
						break;
					}
					case text_render_options::dist_scale_mode_type::perspective:
					{
						// === PERSPECTIVE SCALING ===
						// 원근법 시뮬레이션 (거리 반비례 스케일링)
						// - 실제 3D 월드에서 고정 크기 객체가 거리에 따라 보이는 방식 모방
						// - 가까운 객체는 크게, 먼 객체는 작게 보이는 자연스러운 원근감
						//
						// 기본 공식: scale = reference_distance / distance
						// 결과:
						//   - distance < reference_distance → scale > 1.0 (가까워서 크게 보임)
						//   - distance = reference_distance → scale = 1.0 (기준 크기)
						//   - distance > reference_distance → scale < 1.0 (멀어서 작게 보임)

						const float safe_distance = std::max(distance, 0.001f); // zero division 방지
						distance_scale_factor = render_opts.dist_scale_opts.ref_distance / safe_distance;

						// 너무 멀어지면 페이드 아웃 시작
						if (distance >= render_opts.dist_scale_opts.max_distance)
						{
							distance_scale_factor = 0.0f; // 최대 거리 이상에서는 완전히 사라짐
						}
						else if (distance > render_opts.dist_scale_opts.ref_distance * 2.0f)
						{
							// 기준 거리의 2배부터 페이드 시작
							// (이유: 너무 멀리 있는 텍스트가 지나치게 작아지는 것을 방지)
							const float fade_start = render_opts.dist_scale_opts.ref_distance * 2.0f;
							const float fade_range = render_opts.dist_scale_opts.max_distance - fade_start;
							const float fade_factor = (distance - fade_start) / fade_range;

							// 제곱 함수를 사용한 부드러운 페이드: f(t) = (1-t)^2
							// -> 선형 페이드보다 더 자연스러운 감소 곡선 제공
							const float alpha = 1.0f - std::clamp(fade_factor * fade_factor, 0.0f, 1.0f);
							distance_scale_factor *= alpha;
						}
						break;
					}
					}
				}

				// Apply distance-based scaling to the label's original scale
				const float final_scale = std::clamp(label->get_scale() * distance_scale_factor, 0.0f, 1.0f);

				// Skip rendering if scale is effectively zero
				if (final_scale < 0.001f) {
					continue;
				}

				this->_draw_text(
					label->get_text(),
					projected_viewport_pos,
					final_scale,
					label->get_text_alignment()
				);
			} // for
		}

		// --- Render 2D labels (no depth testing) ---
		if (!text_2d_objects.empty())
		{
			auto& active_text_shader = _ctx->text_shader;
			active_text_shader.use();
			active_text_shader.set_uniform_mat4("u_glyphQuadProj", _ctx->glyph_quad_proj);

			const vec2_f32 viewport_offset{ static_cast<float>(viewport.x), static_cast<float>(viewport.y) };
			for (const auto& label : text_2d_objects)
			{
				if (!label->is_visible() || label->is_empty()) {
					continue;
				}

				active_text_shader.set_uniform_vec3("u_textColor", label->get_color().to_eigen());
				this->_draw_text(
					label->get_text(),
					label->get_position() + viewport_offset,
					label->get_scale(),
					label->get_text_alignment()
				);
			} // for
		}
    }

	void text_renderer::_draw_text(
		const std::string_view text, 
		const vec2_f32 viewport_space_pos, 
		const float scale, 
		const text_alignment_type text_align)
	{	
		// Calculate text alignment offset
		vec2_f32 alignment_offset{ 0.0f, 0.0f };
		if (text_align == text_alignment_type::center) {
			const vec2_f32 text_size = this->_measure_text_size(text, scale);
			alignment_offset.x() = -text_size.x() * 0.5f;
			alignment_offset.y() = text_size.y() * 0.5f; // Note: text_size.y is positive, but text renders downward
		}
		// For top_left alignment, no offset is needed (default behavior)

		float x = viewport_space_pos.x() + alignment_offset.x();
		float y = viewport_space_pos.y() + alignment_offset.y();
		const float initial_x = x;

		// iterate through all characters
		for (const auto ch : text)
		{
			const detail::font_glyph_metrics_t* const glyph = _ctx->font_atlas_texture->find_glyph_metrics(static_cast<FT_ULong>(ch));
			if (!glyph) {
				continue; // skip unknown characters
			}

			if (ch == '\n') {
				x = initial_x;
				y -= static_cast<float>(_ctx->font_atlas_texture->max_glyph_size().y()) * _ctx->line_spacing_factor * scale;
				continue;
			}

			const bool is_whitespace = (ch == ' ');
			if (!is_whitespace) { // skip rendering for whitespace characters (minor optimization)
				const float xpos = x + static_cast<float>(glyph->bearing.x()) * scale;
				const float ypos = y - static_cast<float>(static_cast<int32_t>(glyph->size.y()) - glyph->bearing.y()) * scale;

				// calculate the model matrix for each "glyph quad" and pass it to shader before rendering
				//const glm::mat4 glyph_quad_model
				//	= glm::translate(glm::mat4(1.0f), glm::vec3(xpos, ypos, 0.0f))
				//	* glm::scale(glm::mat4(1.0f), glm::vec3(glyph->size.x() * scale, glyph->size.y() * scale, 0.0f));

				const Eigen::Matrix4f glyph_quad_model = (
					Eigen::Translation3f(xpos, ypos, 0.0f) * Eigen::Scaling(glyph->size.x() * scale, glyph->size.y() * scale, 0.0f)
				).matrix();

				// enqueue data to SSBO queue
				_ctx->text_render_ssbo_q->enqueue_data(
					glyph_quad_model,
					glyph->uv_rect
				);
			}

			// check if SSBO queue is full - if so, flush and render the enqueued glyphs
			if (_ctx->text_render_ssbo_q->is_full()) {
				// upload enqueued buffer data to GPU (SSBO) and get instance count
				const size_t instance_count = _ctx->text_render_ssbo_q->flush();

				// render quad (using instanced to avoid multiple calls to glDrawArrays)
				::glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4/*vertexcount*/, static_cast<GLsizei>(instance_count));
			}

			// now advance cursors for next glyph (note that advance is number of 1/64 pixels)
			x += (glyph->advance_x >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
		} // for

		// process any remaining enqueued data in SSBO queue
		if (!_ctx->text_render_ssbo_q->is_empty()) {
			// upload enqueued buffer data to GPU (SSBO) and get instance count
			const size_t instance_count = _ctx->text_render_ssbo_q->flush();

			// render quad (using instanced to avoid multiple calls to glDrawArrays)
			::glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4/*vertexcount*/, static_cast<GLsizei>(instance_count));
		}
	}

	vec2_f32 text_renderer::_measure_text_size(
		const std::string_view text, 
		const float scale) const
	{
		float x = 0.0f, y = 0.0f;

		// iterate through all characters
		for (const auto ch : text)
		{
			const detail::font_glyph_metrics_t* const glyph = _ctx->font_atlas_texture->find_glyph_metrics(static_cast<FT_ULong>(ch));
			if (!glyph) {
				continue; // skip unknown characters
			}

			if (ch == '\n') {
				x = 0.0f;
				y -= static_cast<float>(_ctx->font_atlas_texture->max_glyph_size().y()) * _ctx->line_spacing_factor * scale;
				continue;
			}

			// now advance cursors for next glyph (note that advance is number of 1/64 pixels)
			x += (glyph->advance_x >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
		} // for

		return vec2_f32{ x, -y };
	}

} // namespace