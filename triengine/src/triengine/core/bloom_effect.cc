#include "bloom_effect.hh"

#include <triengine/utility/string_format.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

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

        inline void create_screen_quad_vao(
            GLuint& vao_id/* out */,
            GLuint& vbo_id/* out */)
        {
            GLCall(::glCreateVertexArrays(1, &vao_id));
            GLCall(::glCreateBuffers(1, &vbo_id));

            // Link VAO's binding index 0 to the VBO
            GLCall(::glVertexArrayVertexBuffer(
                vao_id/*vao id*/,
                0/*bindingindex*/,
                vbo_id/*vbo id*/,
                0/*startoffset*/,
                sizeof(quad_vertex_t)/*stride*/
            ));

            // Allocate (immutable) GPU storage & upload data to VBO
            GLCall(::glNamedBufferStorage(
                vbo_id,
                sizeof(quad_vertex_t) * quadVertices.size(),
                quadVertices.data(),
                GL_MAP_READ_BIT
            ));

            // Setup vertex attribute index 0
            GLCall(::glEnableVertexArrayAttrib(vao_id, 0/*attribindex*/)); // attrib 0 = position
            GLCall(::glVertexArrayAttribBinding(vao_id, 0/*attribindex*/, 0/*bindingindex*/)); // attrib 0 <- binding 0
            GLCall(::glVertexArrayAttribFormat(
                vao_id,
                0/*attribindex*/,
                decltype(quad_vertex_t::position)::SizeAtCompileTime/*size*/,
                GL_FLOAT/*type*/,
                GL_FALSE/*normalize*/,
                offsetof(quad_vertex_t, position)/*relativeoffset*/
            ));

            // Setup vertex attribute index 1
            GLCall(::glEnableVertexArrayAttrib(vao_id, 1/*attribindex*/)); // attrib 1 = uv
            GLCall(::glVertexArrayAttribBinding(vao_id, 1/*attribindex*/, 0/*bindingindex*/)); // attrib 1 <- binding 0
            GLCall(::glVertexArrayAttribFormat(
                vao_id,
                1/*attribindex*/,
                decltype(quad_vertex_t::uv)::SizeAtCompileTime/*size*/,
                GL_FLOAT/*type*/,
                GL_FALSE/*normalize*/,
                offsetof(quad_vertex_t, uv)/*relativeoffset*/
            ));
        }

    } // namespace

    phys_bloom_effect::mip_texture::mip_texture(const vec2_f32 mip_size_f32)
    {
        this->resize(mip_size_f32);
    }

    phys_bloom_effect::mip_texture::~mip_texture()
    {
        if (_mip_tex_id != 0) {
            GLCall(::glDeleteTextures(1, &_mip_tex_id));
            _mip_tex_id = 0;
        }
    }

    bool phys_bloom_effect::mip_texture::is_valid() const noexcept
    {
        return _mip_tex_id != 0;
    }

    vec2_f32 phys_bloom_effect::mip_texture::get_size_f32() const noexcept
    {
        return _mip_size_f32;
    }

    vec2_i32 phys_bloom_effect::mip_texture::get_size_i32() const noexcept
    {
        return _cast_f32_to_i32_trunc(_mip_size_f32);
    }

    GLuint phys_bloom_effect::mip_texture::get_texture_id() const noexcept
    {
        return _mip_tex_id;
    }

    void phys_bloom_effect::mip_texture::resize(const vec2_f32 new_mip_size_f32)
    {
        const vec2_f32 old_mip_size_f32 = this->get_size_f32();
        const vec2_i32 old_mip_size_i32 = this->get_size_i32();
        const vec2_i32 new_mip_size_i32 = _cast_f32_to_i32_trunc(new_mip_size_f32);

        if (new_mip_size_i32 != old_mip_size_i32)
        {
            // size is changed, we need to recreate the texture storage

            GLuint new_mip_tex_id{ 0 };
            GLCall(::glCreateTextures(GL_TEXTURE_2D, 1, &new_mip_tex_id));

            GLCall(::glTextureParameteri(new_mip_tex_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
            GLCall(::glTextureParameteri(new_mip_tex_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            GLCall(::glTextureParameteri(new_mip_tex_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
            GLCall(::glTextureParameteri(new_mip_tex_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

            // Allocate texture storage
            // NOTE: We are downscaling an HDR color buffer, so we need a float texture format
            GLCall(::glTextureStorage2D(
                new_mip_tex_id,                             /* GLuint texture */
                1,                                          /* GLsizei levels (1 for no mipmaps) */
                GL_R11F_G11F_B10F,                          /* GLenum internalformat */
                static_cast<GLsizei>(new_mip_size_i32.x()), /* GLsizei width */
                static_cast<GLsizei>(new_mip_size_i32.y())  /* GLsizei height */
            ));

            if (this->is_valid()) {
                GLCall(::glDeleteTextures(1, &_mip_tex_id));
            }
            _mip_tex_id = new_mip_tex_id;
            _mip_size_f32 = new_mip_size_f32;
        }
        else
        {
            // size is not changed, just update the mip size (fractional part)
            _mip_size_f32 = new_mip_size_f32;
        }

        TRIENGINE_TRACE("Resize mip texture: (%.3f, %.3f) -> (%.3f, %.3f)"
            , old_mip_size_f32.x()
            , old_mip_size_f32.y()
            , new_mip_size_f32.x()
            , new_mip_size_f32.y()
        );
    }

    phys_bloom_effect::phys_bloom_effect()
    {
    }

    phys_bloom_effect::~phys_bloom_effect()
    {
        if (_initialized) { this->destroy(); }
    }

    bool phys_bloom_effect::create(
        gl_context& glctx)
    {
        const vec2_i32 initial_window_size = glctx.get_window_size();
        auto shader_ldr = glctx.get_shader_loader();

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
        create_screen_quad_vao(_screen_quad_vao, _screen_quad_vbo);

        // Create Framebuffer & mip chain
        // ------------------------------------------------------------------
        GLCall(::glCreateFramebuffers(1, &_bloom_fbo));

        vec2_f32 new_mip_size_f32{ static_cast<float>(initial_window_size.x()), static_cast<float>(initial_window_size.y()) };

        _mip_chain.reserve(kMipChainSize);
        for (size_t i = 0; i < kMipChainSize; ++i)
        {
            new_mip_size_f32 *= 0.5f;

            TRIENGINE_TRACE("Create bloom mip (%.3f, %.3f)"
                , new_mip_size_f32.x()
                , new_mip_size_f32.y()
            );

            _mip_chain.emplace_back(new_mip_size_f32);
        } // for

        // 첫 번재 mip을 bloom fbo의 color buffer에 attach
        // (이후 downsample, upsample 패스에서 순차적으로 mip 순회하면서 attach 버퍼 변경 예정)
        GLCall(::glNamedFramebufferTexture(
            _bloom_fbo,                     /* GLuint framebuffer */
            GL_COLOR_ATTACHMENT0,           /* GLenum attachment */
            _mip_chain[0].get_texture_id(), /* GLuint texture */
            0                               /* GLint level */
        ));

        // check completion status
        if (const GLenum status{ ::glCheckNamedFramebufferStatus(_bloom_fbo, GL_FRAMEBUFFER) };
            status != GL_FRAMEBUFFER_COMPLETE) {
            TRIENGINE_ERROR("Failed to configure bloom fbo (status: 0x%X)", status);
            return false;
        }

        // Shaders
        // ------------------------------------------------------------------
        _downsample_shader
            .attach_vertex_shader({ shader_ldr->load("bloom.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("bloom_downsample_pass.frag")->c_str() })
            .link();

        _upsample_shader
            .attach_vertex_shader({ shader_ldr->load("bloom.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("bloom_upsample_pass.frag")->c_str() })
            .link();

        _composite_shader
            .attach_vertex_shader({ shader_ldr->load("bloom.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("bloom_composite_pass.frag")->c_str() })
            .link();

        return true;
    }

    void phys_bloom_effect::destroy()
    {
        _mip_chain.clear();

        ::glDeleteFramebuffers(1, &_bloom_fbo);
        _bloom_fbo = 0;

        _downsample_shader.destroy();
        _upsample_shader.destroy();
        _composite_shader.destroy();
    }

    const phys_bloom_effect::mip_texture& phys_bloom_effect::get_bloom_mip(size_t index) const
    {
        return _mip_chain.at(index);
    }

    void phys_bloom_effect::apply(
        const GLuint src_texture_id,
        const bloom_options& bloom_opts)
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
            const mip_texture& curr_mip = _mip_chain[i];

            // Resize viewport to current mip size
            const vec2_i32 curr_mip_size_i32 = curr_mip.get_size_i32();
            GLCall(::glViewport(0, 0, static_cast<GLsizei>(curr_mip_size_i32.x()), static_cast<GLsizei>(curr_mip_size_i32.y())));

            // Reattach framebuffer color texture to the current mip texture (we write to this texture)
            GLCall(::glNamedFramebufferTexture(
                _bloom_fbo,                /* GLuint framebuffer */
                GL_COLOR_ATTACHMENT0,      /* GLenum attachment */
                curr_mip.get_texture_id(), /* GLuint texture */
                0                          /* GLint level */
            ));

            // Render screen-filled quad of resolution of current mip
            this->_render_screen_quad();

            // Set current mip resolution as srcResolution for next iteration
            _downsample_shader.set_uniform_vec2("u_srcTexelSize", curr_mip.get_size_f32().cwiseInverse().eval());

            // Set current mip as texture input for next iteration
            GLCall(::glBindTexture(GL_TEXTURE_2D, curr_mip.get_texture_id())); // u_srcTexture

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
        _upsample_shader.set_uniform_float("u_filterRadius", bloom_opts.upsample_filter_radius);
        _upsample_shader.set_uniform_float("u_aspectRatio", _src_texture_aspect_ratio);

        for (int64_t i = static_cast<int64_t>(_mip_chain.size()) - 1; i > 0; --i)
        {
            const mip_texture& curr_mip = _mip_chain[i];
            const mip_texture& next_mip = _mip_chain[i - 1];

            // Resize viewport to next mip size
            const vec2_i32 next_mip_size_i32 = next_mip.get_size_i32();
            GLCall(::glViewport(0, 0, static_cast<GLsizei>(next_mip_size_i32.x()), static_cast<GLsizei>(next_mip_size_i32.y())));

            // Reattach framebuffer color texture to the next mip texture (we write to this texture)
            GLCall(::glNamedFramebufferTexture(
                _bloom_fbo,                /* GLuint framebuffer */
                GL_COLOR_ATTACHMENT0,      /* GLenum attachment */
                next_mip.get_texture_id(), /* GLuint texture */
                0                          /* GLint level */
            ));

            // Bind current mip texture (we read from this texture)
            GLCall(::glBindTexture(GL_TEXTURE_2D, curr_mip.get_texture_id())); // u_srcTexture

            // Render screen-filled quad of resolution of current mip
            this->_render_screen_quad();
        }

        ///////////////////////////////////////////////////////////////////////////
        // Final Composite(Mixin) pass
        ///////////////////////////////////////////////////////////////////////////

        GLCall(::glDisable(GL_BLEND));

        // Restore viewport
        GLCall(::glViewport(0, 0, _src_texture_size.x(), _src_texture_size.y()));

        GLCall(::glNamedFramebufferTexture(
            _bloom_fbo,            /* GLuint framebuffer */
            GL_COLOR_ATTACHMENT0,  /* GLenum attachment */
            src_texture_id,        /* GLuint texture */
            0                      /* GLint level */
        ));

        _composite_shader.use();
        _composite_shader.set_uniform_float("u_bloomStrength", bloom_opts.strength);

        GLCall(::glBindTexture(GL_TEXTURE_2D, src_texture_id)); // u_sceneTexture

        GLCall(::glActiveTexture(GL_TEXTURE1));
        GLCall(::glBindTexture(GL_TEXTURE_2D, this->get_bloom_mip(0).get_texture_id())); // u_bloomBlurTexture

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

        vec2_f32 new_mip_size_f32 = new_window_size.cast<float>();
        for (auto& curr_mip : _mip_chain)
        {
            new_mip_size_f32 *= 0.5f;

            // update mip size & reallocate mip texture storage
            curr_mip.resize(new_mip_size_f32);
        } // for
    }

    void phys_bloom_effect::_render_screen_quad()
    {
        // draw screen quad
        GLCall(::glBindVertexArray(_screen_quad_vao));
        GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
    }

} // namespace