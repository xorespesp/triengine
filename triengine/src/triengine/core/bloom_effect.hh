#pragma once
#include <triengine/common.h>
#include <triengine/lighting_options.hh>
#include <triengine/core/shader.hh>

#include <vector>

namespace triengine::core
{
    // TODO: optimize (DSA, compute shader) & soft threshold
    class phys_bloom_effect
    {
    public:
        static constexpr size_t kMipChainSize{ 6 }; // Total size of bloom mip chain

        // bloom mip
        struct mip_info_t
        {
            vec2_i32 size_i32{};
            vec2_f32 size_f32{};
            GLuint texture{};
        };

    public:
        phys_bloom_effect();
        ~phys_bloom_effect();

        bool create(const vec2_i32 initial_window_size);
        void destroy();

        const mip_info_t& get_bloom_mip(size_t index) const;

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
        std::vector<mip_info_t> _mip_chain;

        shader_program
            _downsample_shader,
            _upsample_shader,
            _composite_shader;
    };

} // namespace