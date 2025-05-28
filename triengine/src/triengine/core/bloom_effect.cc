#include "bloom_effect.hh"

#include <triengine/utility/string_format.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

#include <triengine/shaders/bloom_shaders.h>

namespace triengine::core
{
    namespace
    {
        struct quad_vertex_t {
            vec3_f32 position; // vertex position
            vec2_f32 uv; // texture coordinate (uv coordinate)
        };
        static_assert(sizeof(quad_vertex_t) == 5 * sizeof(float), "!!");

        const std::array<quad_vertex_t, 6> quadVertices{
            quad_vertex_t{ vec3_f32{ -1.0f, -1.0f, 0.0f }, vec2_f32{ 0.0f, 0.0f } },
            quad_vertex_t{ vec3_f32{  1.0f, -1.0f, 0.0f }, vec2_f32{ 1.0f, 0.0f } },
            quad_vertex_t{ vec3_f32{  1.0f,  1.0f, 0.0f }, vec2_f32{ 1.0f, 1.0f } },
            quad_vertex_t{ vec3_f32{  1.0f,  1.0f, 0.0f }, vec2_f32{ 1.0f, 1.0f } },
            quad_vertex_t{ vec3_f32{ -1.0f,  1.0f, 0.0f }, vec2_f32{ 0.0f, 1.0f } },
            quad_vertex_t{ vec3_f32{ -1.0f, -1.0f, 0.0f }, vec2_f32{ 0.0f, 0.0f } }
        };
    } // namespace

    phys_bloom_effect::phys_bloom_effect()
    {
    }

    phys_bloom_effect::~phys_bloom_effect()
    {
        if (_initialized) { this->destroy(); }
    }

    bool phys_bloom_effect::create(const vec2_i32 initial_window_size)
    {
        TRIENGINE_TRACE("Creating bloom effect");
        if (_initialized) {
            TRIENGINE_WARN("already created");
            return true;
        }

        _src_texture_size = initial_window_size;
        _src_texture_texel_size = initial_window_size.cast<float>().cwiseInverse().eval();
        _src_texture_aspect_ratio = static_cast<float>(initial_window_size.x()) / static_cast<float>(initial_window_size.y());

        // Create screen-quad VAO
        // ------------------------------------------------------------------
        GLCall(::glGenVertexArrays(1, &_screen_quad_vao));
        GLCall(::glGenBuffers(1, &_screen_quad_vbo));
        GLCall(::glBindVertexArray(_screen_quad_vao));
        GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _screen_quad_vbo));
        GLCall(::glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertex_t) * quadVertices.size(), quadVertices.data(), GL_STATIC_DRAW));
        GLCall(::glEnableVertexAttribArray(0));
        GLCall(::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(quad_vertex_t), (void*)offsetof(quad_vertex_t, position)));
        GLCall(::glEnableVertexAttribArray(1));
        GLCall(::glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(quad_vertex_t), (void*)offsetof(quad_vertex_t, uv)));
        GLCall(::glBindVertexArray(0));

        // Create Framebuffer & mip chain
        // ------------------------------------------------------------------
        GLCall(::glGenFramebuffers(1, &_bloom_fbo));
        GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _bloom_fbo));

        vec2_i32 new_mip_size_i32{ initial_window_size };
        vec2_f32 new_mip_size_f32{ static_cast<float>(initial_window_size.x()), static_cast<float>(initial_window_size.y()) };

        _mip_chain.reserve(kMipChainSize);
        for (size_t i = 0; i < kMipChainSize; ++i)
        {
            new_mip_size_i32 /= 2;
            new_mip_size_f32 *= 0.5f;

            TRIENGINE_TRACE("Create bloom mip %dx%d"
                , new_mip_size_i32.x()
                , new_mip_size_i32.y()
            );

            mip_info_t new_mip;
            new_mip.size_i32 = new_mip_size_i32;
            new_mip.size_f32 = new_mip_size_f32;

            GLCall(::glGenTextures(1, &new_mip.texture));
            GLCall(::glBindTexture(GL_TEXTURE_2D, new_mip.texture));

            // we are downscaling an HDR color buffer, so we need a float texture format
            GLCall(::glTexImage2D(GL_TEXTURE_2D, 0,
                GL_R11F_G11F_B10F,
                new_mip.size_i32.x(),
                new_mip.size_i32.y(),
                0, GL_RGB, GL_FLOAT, nullptr));

            GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
            GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
            GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

            _mip_chain.emplace_back(new_mip);
        } // for

        // 첫 번재 mip을 bloom fbo의 color buffer에 attach
        // (이후 downsample, upsample 패스에서 순차적으로 mip 순회하면서 attach 버퍼 변경 예정)
        GLCall(::glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, _mip_chain[0].texture, 0));

        // check completion status
        if (const GLenum status{ ::glCheckFramebufferStatus(GL_FRAMEBUFFER) };
            status != GL_FRAMEBUFFER_COMPLETE) {
            TRIENGINE_ERROR("gbuffer FBO error, status: 0x%X\n", status);
            GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, 0));
            return false;
        }

        // Shaders
        // ------------------------------------------------------------------
        _downsample_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shaders::kPhysBloomVertexShader })
            .attach_fragment_shader({ shaders::glslShaderVersion, shaders::kPhysBloomDownsampleFragmentShader })
            .link();

        _upsample_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shaders::kPhysBloomVertexShader })
            .attach_fragment_shader({ shaders::glslShaderVersion, shaders::kPhysBloomUpsampleFragmentShader })
            .link();

        _composite_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shaders::kPhysBloomVertexShader })
            .attach_fragment_shader({ shaders::glslShaderVersion, shaders::kPhysBloomCompositeFragmentShader })
            .link();

        return true;
    }

    void phys_bloom_effect::destroy()
    {
        for (auto& curr_mip : _mip_chain) {
            ::glDeleteTextures(1, &curr_mip.texture);
        }
        _mip_chain.clear();
        ::glDeleteFramebuffers(1, &_bloom_fbo);
        _bloom_fbo = 0;

        _downsample_shader.destroy();
        _upsample_shader.destroy();
        _composite_shader.destroy();
    }

    const phys_bloom_effect::mip_info_t& phys_bloom_effect::get_bloom_mip(size_t index) const
    {
        return _mip_chain.at(index);
    }

    void phys_bloom_effect::apply(
        const GLuint src_texture_id,
        const float upsample_filter_radius,
        const float bloom_strength)
    {
        struct GLStateBackupRAIIContext final {
            GLboolean depth_test_enabled{};
            GLboolean blend_enabled{};
            GLint blend_equation_rgb{}, blend_equation_alpha{};
            GLint src_rgb{}, dst_rgb{}, src_alpha{}, dst_alpha{};

            // Backup GL state
            GLStateBackupRAIIContext() {
                depth_test_enabled = ::glIsEnabled(GL_DEPTH_TEST);
                blend_enabled = ::glIsEnabled(GL_BLEND);
                GLCall(::glGetIntegerv(GL_BLEND_EQUATION_RGB, &blend_equation_rgb));
                GLCall(::glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blend_equation_alpha));
                GLCall(::glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb));
                GLCall(::glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb));
                GLCall(::glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha));
                GLCall(::glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha));
            }

            // Restore GL state
            ~GLStateBackupRAIIContext() {
                if (depth_test_enabled) { GLCall(::glEnable(GL_DEPTH_TEST)); }
                else { GLCall(::glDisable(GL_DEPTH_TEST)); }
                if (blend_enabled) { GLCall(::glEnable(GL_BLEND)); }
                else { GLCall(::glDisable(GL_BLEND)); }
                GLCall(::glBlendEquationSeparate(blend_equation_rgb, blend_equation_alpha));
                GLCall(::glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha));
            }
        };

        GLStateBackupRAIIContext state_backup_{};

        GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _bloom_fbo));

        // setup draw buffer
        const GLenum attachments[1] = { GL_COLOR_ATTACHMENT0 };
        GLCall(::glDrawBuffers(1, attachments));

        ///////////////////////////////////////////////////////////////////////////
        // Downsample pass
        ///////////////////////////////////////////////////////////////////////////

        GLCall(::glDisable(GL_DEPTH_TEST)); // No need for depth testing since we are drawing a 2D quad on the screen.
        GLCall(::glDisable(GL_BLEND));

        _downsample_shader.use();
        _downsample_shader.set_uniform_vec2("u_srcTexelSize", _src_texture_texel_size);
        if (_apply_karis_avg_on_downsample) {
            _downsample_shader.set_uniform_int("u_mipLevel", 0);
        }

        // Bind srcTexture (HDR color buffer) as initial texture input
        GLCall(::glActiveTexture(GL_TEXTURE0));
        GLCall(::glBindTexture(GL_TEXTURE_2D, src_texture_id));

        // Progressively downsample through the mip chain
        for (size_t i = 0; i < _mip_chain.size(); ++i)
        {
            const mip_info_t& curr_mip = _mip_chain[i];

            GLCall(::glViewport(0, 0, curr_mip.size_f32.x(), curr_mip.size_f32.y()));
            GLCall(::glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D,
                curr_mip.texture,
                0));

            // Render screen-filled quad of resolution of current mip
            this->_render_screen_quad();

            // Set current mip resolution as srcResolution for next iteration
            _downsample_shader.set_uniform_vec2("u_srcTexelSize", curr_mip.size_f32.cwiseInverse().eval());

            // Set current mip as texture input for next iteration
            GLCall(::glBindTexture(GL_TEXTURE_2D, curr_mip.texture)); // u_srcTexture

            // Disable Karis average for consequent downsamples
            if (i == 0) {
                _downsample_shader.set_uniform_int("u_mipLevel", 1);
            }
        }

        ///////////////////////////////////////////////////////////////////////////
        // Upsample pass
        ///////////////////////////////////////////////////////////////////////////

        // Enable additive blending
        GLCall(::glEnable(GL_BLEND));
        GLCall(::glBlendFunc(GL_ONE, GL_ONE));
        GLCall(::glBlendEquation(GL_FUNC_ADD));

        _upsample_shader.use();
        _upsample_shader.set_uniform_float("u_filterRadius", upsample_filter_radius);
        _upsample_shader.set_uniform_float("u_aspectRatio", _src_texture_aspect_ratio);

        for (int64_t i = static_cast<int64_t>(_mip_chain.size()) - 1; i > 0; --i)
        {
            const mip_info_t& curr_mip = _mip_chain[i];
            const mip_info_t& next_mip = _mip_chain[i - 1];

            // Bind viewport and texture from where to read
            GLCall(::glBindTexture(GL_TEXTURE_2D, curr_mip.texture)); // u_srcTexture

            // Set framebuffer render target (we write to this texture)
            GLCall(::glViewport(0, 0, next_mip.size_f32.x(), next_mip.size_f32.y()));
            GLCall(::glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D,
                next_mip.texture,
                0));

            // Render screen-filled quad of resolution of current mip
            this->_render_screen_quad();
        }

        ///////////////////////////////////////////////////////////////////////////
        // Final Composite(Mixin) pass
        ///////////////////////////////////////////////////////////////////////////

        GLCall(::glDisable(GL_BLEND));

        // Restore viewport
        GLCall(::glViewport(0, 0, _src_texture_size.x(), _src_texture_size.y()));

        GLCall(::glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            src_texture_id,
            0));

        _composite_shader.use();
        _composite_shader.set_uniform_float("u_bloomStrength", bloom_strength);

        GLCall(::glBindTexture(GL_TEXTURE_2D, src_texture_id)); // u_sceneTexture

        GLCall(::glActiveTexture(GL_TEXTURE1));
        GLCall(::glBindTexture(GL_TEXTURE_2D, this->get_bloom_mip(0).texture)); // u_bloomBlurTexture

        this->_render_screen_quad();

        // Cleanup
        GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }

    void phys_bloom_effect::resize(const vec2_i32 new_window_size)
    {
        if (_src_texture_size == new_window_size) { return; }

        _src_texture_size = new_window_size;
        _src_texture_texel_size = new_window_size.cast<float>().cwiseInverse().eval();
        _src_texture_aspect_ratio = static_cast<float>(new_window_size.x()) / static_cast<float>(new_window_size.y());

        vec2_i32 new_mip_size_i32{ new_window_size };
        vec2_f32 new_mip_size_f32{ static_cast<float>(new_window_size.x()), static_cast<float>(new_window_size.y()) };
        for (auto& curr_mip : _mip_chain)
        {
            new_mip_size_i32 /= 2;
            new_mip_size_f32 *= 0.5f;

            TRIENGINE_TRACE("Resize bloom mip %dx%d -> %dx%d"
                , curr_mip.size_i32.x()
                , curr_mip.size_i32.y()
                , new_mip_size_i32.x()
                , new_mip_size_i32.y()
            );

            // update mip size
            curr_mip.size_f32 = new_mip_size_f32;
            curr_mip.size_i32 = new_mip_size_i32;

            // reallocate texture buffer
            GLCall(::glBindTexture(GL_TEXTURE_2D, curr_mip.texture));
            GLCall(::glTexImage2D(GL_TEXTURE_2D, 0,
                GL_R11F_G11F_B10F,
                curr_mip.size_i32.x(), curr_mip.size_i32.y(),
                0, GL_RGB, GL_FLOAT, nullptr));
        } // for
    }

    void phys_bloom_effect::_render_screen_quad()
    {
        // draw screen quad
        GLCall(::glBindVertexArray(_screen_quad_vao));
        GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
    }

} // namespace