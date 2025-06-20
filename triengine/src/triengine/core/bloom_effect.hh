#pragma once
#include <triengine/common.h>
#include <triengine/lighting_options.hh>
#include <triengine/core/shader.hh>

#include <vector>

namespace triengine::core
{
    // TODO: apply optimization (e.g: compute shader) & implement soft threshold
    class phys_bloom_effect
    {
    public:
        static constexpr size_t kMipChainSize{ 6 }; // Total size of bloom mip chain

        // bloom mip texture
        class mip_texture
        {
        private:
            vec2_f32 _mip_size_f32{ 0.0f, 0.0f }; // mip size in f32
            GLuint _mip_tex_id{ 0 }; // mip texture id

        private:
            static inline vec2_i32 _cast_f32_to_i32_trunc(const vec2_f32 size_f32) noexcept {
                // cast f32 size to i32 size
                // IMPORTANT NOTE: To avoid an out-of-memory boundary issue, we need to truncate the fractional part.
                return vec2_i32{
                    static_cast<int32_t>(size_f32.x()),
                    static_cast<int32_t>(size_f32.y())
                };
            }

        public:
            mip_texture() = default;
            mip_texture(vec2_f32 mip_size_f32);
            ~mip_texture();

            bool is_valid() const noexcept;
            vec2_f32 get_size_f32() const noexcept;
            vec2_i32 get_size_i32() const noexcept;
            GLuint get_texture_id() const noexcept;

            void resize(vec2_f32 new_mip_size_f32);
        };

    public:
        phys_bloom_effect();
        ~phys_bloom_effect();

        bool create(const vec2_i32 initial_window_size);
        void destroy();

        const mip_texture& get_bloom_mip(size_t index) const;

        void apply(
            const GLuint src_texture_id,
            const bloom_options& bloom_opts
        );

        void resize(const vec2_i32 new_window_size);

    private:
        void _render_screen_quad();

    private:
        bool _initialized{ false };
        bool _apply_karis_avg_on_downsample{ false };

        vec2_i32 _src_texture_size{}; // srcViewportSize
        vec2_f32 _src_texture_texel_size{}; // 1.0 / srcViewportSize
        float _src_texture_aspect_ratio{}; // srcViewportSize.x / srcViewportSize.y

        GLuint _screen_quad_vao{ 0 }, _screen_quad_vbo{ 0 };
        GLuint _bloom_fbo{ 0 };
        std::vector<mip_texture> _mip_chain;

        shader_program
            _downsample_shader,
            _upsample_shader,
            _composite_shader;
    };

} // namespace