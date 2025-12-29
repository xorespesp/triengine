#include "frame_buffer.hh"
#include <triengine/utility/gl_utils.hh>
#include <triengine/utility/debug_utils.hh>

namespace triengine::core
{
    frame_buffer::~frame_buffer()
    {
        if (this->is_valid()) {
            this->destroy();
        }
    }

    frame_buffer::frame_buffer(frame_buffer&& rhs) noexcept
    {
        *this = std::move(rhs);
    }

    frame_buffer& frame_buffer::operator=(frame_buffer&& rhs) noexcept
    {
        if (this != &rhs) {
            if (this->is_valid()) {
                this->destroy();
            }
            std::swap(_fbo_id, rhs._fbo_id);
            std::swap(_color_attachments, rhs._color_attachments);
            std::swap(_depth_attachment, rhs._depth_attachment);
            std::swap(_stencil_attachment, rhs._stencil_attachment);
            std::swap(_depth_stencil_attachment, rhs._depth_stencil_attachment);
            std::swap(_width_pixels, rhs._width_pixels);
            std::swap(_height_pixels, rhs._height_pixels);
            std::swap(_sample_count, rhs._sample_count);
        }

        return *this;
    }

    GLuint frame_buffer::fbo_id() const noexcept {
        return _fbo_id;
    }

    const frame_buffer::attachment_info_t* frame_buffer::color_attachment(size_t attachment_index) const noexcept {
        return (attachment_index < _color_attachments.size()) ? &_color_attachments[attachment_index] : nullptr;
    }

    const frame_buffer::attachment_info_t* frame_buffer::color_attachment() const noexcept {
        return this->color_attachment(0);
    }

    const frame_buffer::attachment_info_t* frame_buffer::depth_attachment() const noexcept {
        return _depth_attachment.has_value() ? &_depth_attachment.value() : nullptr;
    }

    const frame_buffer::attachment_info_t* frame_buffer::stencil_attachment() const noexcept {
        return _stencil_attachment.has_value() ? &_stencil_attachment.value() : nullptr;
    }

    const frame_buffer::attachment_info_t* frame_buffer::depth_stencil_attachment() const noexcept {
        return _depth_stencil_attachment.has_value() ? &_depth_stencil_attachment.value() : nullptr;
    }

    int32_t frame_buffer::width_pixels() const noexcept {
        return _width_pixels;
    }

    int32_t frame_buffer::height_pixels() const noexcept {
        return _height_pixels;
    }

    int32_t frame_buffer::sample_count() const noexcept {
        return _sample_count;
    }

    size_t frame_buffer::color_attachments_size() const noexcept
    {
        return _color_attachments.size();
    }

    bool frame_buffer::has_color_attachment() const noexcept {
        return _color_attachments.size() > 0;
    }

    bool frame_buffer::has_depth_attachment() const noexcept {
        return _depth_attachment.has_value();
    }

    bool frame_buffer::has_stencil_attachment() const noexcept {
        return _stencil_attachment.has_value();
    }

    bool frame_buffer::has_depth_stencil_attachment() const noexcept {
        return _depth_stencil_attachment.has_value();
    }

    bool frame_buffer::is_multisampled() const noexcept {
        return _sample_count > 1;
    }

    bool frame_buffer::is_valid() const noexcept {
        return _fbo_id != kInvalidGLFrameBufferID;
    }

    bool frame_buffer::is_MRT() const noexcept {
        return _color_attachments.size() > 1;
    }

    void frame_buffer::reallocate(
        const int32_t width_pixels,
        const int32_t height_pixels,
        const int32_t sample_count)
    {
        if (!this->is_valid()) {
            TRIENGINE_PANIC("attempting to reallocate an invalid framebuffer");
        }

        if (width_pixels <= 0 || height_pixels <= 0) {
            TRIENGINE_PANIC("invalid frame buffer size");
        }

        if (sample_count <= 0) {
            TRIENGINE_PANIC("invalid sample count");
        }

        // checks if we truly need to reallocate.
        const bool realloc_needed =
            _width_pixels != width_pixels ||
            _height_pixels != height_pixels ||
            _sample_count != sample_count;

        if (!realloc_needed) {
            return; // skip reallocation
        }

        // 1) Reallocate and attach color attachments
        if (!_color_attachments.empty())
        {
            for (size_t i = 0; i < _color_attachments.size(); ++i)
            {
                auto& curr_attach_info = _color_attachments[i];

                _allocate_attachment_buffer(
                    curr_attach_info,
                    width_pixels, height_pixels, 
                    sample_count
                );

                _attach_to_framebuffer(
                    _fbo_id,
                    curr_attach_info, 
                    static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i)
                );
            } // for
        }

        // 2) Reallocate and attach depth attachment
        if (_depth_attachment.has_value())
        {
            auto& curr_attach_info = *_depth_attachment;

            _allocate_attachment_buffer(
                curr_attach_info,
                width_pixels, height_pixels,
                sample_count
            );

            _attach_to_framebuffer(
                _fbo_id,
                curr_attach_info,
                GL_DEPTH_ATTACHMENT
            );
        }

        // 3) Reallocate and attach stencil attachment
        if (_stencil_attachment.has_value())
        {
            auto& curr_attach_info = *_stencil_attachment;

            // reallocate attachment buffer
            _allocate_attachment_buffer(
                curr_attach_info,
                width_pixels, height_pixels,
                sample_count
            );

            // reattach attachment to framebuffer
            _attach_to_framebuffer(
                _fbo_id,
                curr_attach_info,
                GL_STENCIL_ATTACHMENT
            );
        }

        // 4) Reallocate and attach depth-stencil attachment
        if (_depth_stencil_attachment.has_value())
        {
            auto& curr_attach_info = *_depth_stencil_attachment;

            // reallocate attachment buffer
            _allocate_attachment_buffer(
                curr_attach_info,
                width_pixels,
                height_pixels,
                sample_count
            );

            // reattach attachment to framebuffer
            _attach_to_framebuffer(
                _fbo_id,
                curr_attach_info,
                GL_DEPTH_STENCIL_ATTACHMENT
            );
        }

        // Check FBO completeness
        if (::glCheckNamedFramebufferStatus(_fbo_id, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            TRIENGINE_PANIC("Framebuffer reallocation has not been completed!");
        }

#if FALSE
        // (Optional) Set up draw buffers for MRT
        if (this->has_color_attachment())
        {
            // If we have color attachments, set up the draw buffers for MRT
            std::vector<GLenum> draw_buffers;
            draw_buffers.reserve(_color_attachments.size());
            for (size_t i = 0; i < _color_attachments.size(); ++i) {
                draw_buffers.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i));
            }
            GLCall(::glNamedFramebufferDrawBuffers(_fbo_id, static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data()));
        }
        else
        {
            // No color attachments => depth/stencil only. No color output.
            GLCall(::glNamedFramebufferDrawBuffer(_fbo_id, GL_NONE));
            GLCall(::glNamedFramebufferReadBuffer(_fbo_id, GL_NONE));
        }
#endif

        _width_pixels = width_pixels;
        _height_pixels = height_pixels;
        _sample_count = sample_count;
    }

    void frame_buffer::destroy() noexcept
    {
        if (_fbo_id != kInvalidGLFrameBufferID) {
            ::glDeleteFramebuffers(1, &_fbo_id);
            _fbo_id = kInvalidGLFrameBufferID;
        }

        // Release color attachments
        for (auto& ci : _color_attachments) {
            _deallocate_attachment_buffer(ci);
        }
        _color_attachments.clear();

        // Release depth attachment
        if (_depth_attachment.has_value()) {
            _deallocate_attachment_buffer(_depth_attachment.value());
            _depth_attachment.reset();
        }

        // Release stencil attachment
        if (_stencil_attachment.has_value()) {
            _deallocate_attachment_buffer(_stencil_attachment.value());
            _stencil_attachment.reset();
        }

        // Release depth-stencil attachment
        if (_depth_stencil_attachment.has_value()) {
            _deallocate_attachment_buffer(_depth_stencil_attachment.value());
            _depth_stencil_attachment.reset();
        }

        _width_pixels = _height_pixels = 0;
        _sample_count = 1;
    }

    void frame_buffer::bind() const
    {
        if (!this->is_valid()) {
            TRIENGINE_PANIC("attempting to bind an invalid framebuffer");
        }

        GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _fbo_id));
    }

    void frame_buffer::unbind() const
    {
        GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }

    void frame_buffer::blit_to(
        frame_buffer& target_fb,
        const bool blit_color,
        const bool blit_depth,
        const bool blit_stencil,
        const GLenum blit_filter)
    {
        if (!this->is_valid() || !target_fb.is_valid()) {
            TRIENGINE_PANIC("Cannot blit from/to invalid framebuffer!");
        }
    
        TRIENGINE_ASSERT(!(blit_filter == GL_LINEAR && (blit_depth || blit_stencil)));

        GLbitfield mask{};
        if (blit_color) { mask |= GL_COLOR_BUFFER_BIT; }
        if (blit_depth) { mask |= GL_DEPTH_BUFFER_BIT; }
        if (blit_stencil) { mask |= GL_STENCIL_BUFFER_BIT; }

        GLCall(::glBlitNamedFramebuffer(
            _fbo_id,                                                 /* GLuint readFramebuffer */
            target_fb._fbo_id,                                       /* GLuint drawFramebuffer */
            0, 0, _width_pixels, _height_pixels,                     /* GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1 */
            0, 0, target_fb._width_pixels, target_fb._height_pixels, /* GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1 */
            mask,                                                    /* GLbitfield mask */
            blit_filter                                              /* GLenum filter */
        ));
    }

    void frame_buffer::swap_depth_attachment(frame_buffer& target_fb)
    {
        if (!this->is_valid() || !target_fb.is_valid()) {
            TRIENGINE_PANIC("Cannot swap depth attachment with invalid framebuffer");
        }

        if (_width_pixels != target_fb._width_pixels ||
            _height_pixels != target_fb._height_pixels ||
            _sample_count != target_fb._sample_count)
        {
            TRIENGINE_PANIC("Cannot swap depth attachment between framebuffers with different sizes or sample count");
        }

        if (!_depth_attachment.has_value() || !target_fb._depth_attachment.has_value()) {
            TRIENGINE_PANIC("Cannot swap depth attachment with framebuffer without depth attachment");
        }

        // Swap depth attachments
        std::swap(_depth_attachment.value(), target_fb._depth_attachment.value());

        // Reattach swapped depth attachments to the respective framebuffers
        _attach_to_framebuffer(
            _fbo_id,
            _depth_attachment.value(),
            GL_DEPTH_ATTACHMENT
        );

        _attach_to_framebuffer(
            target_fb._fbo_id,
            target_fb._depth_attachment.value(),
            GL_DEPTH_ATTACHMENT
        );
    }

    void frame_buffer::swap_stencil_attachment(frame_buffer& target_fb)
    {
        if (!this->is_valid() || !target_fb.is_valid()) {
            TRIENGINE_PANIC("Cannot swap stencil attachment with invalid framebuffer");
        }

        if (_width_pixels != target_fb._width_pixels ||
            _height_pixels != target_fb._height_pixels ||
            _sample_count != target_fb._sample_count)
        {
            TRIENGINE_PANIC("Cannot swap stencil attachments between framebuffers with different sizes or sample count");
        }

        if (!_stencil_attachment.has_value() || !target_fb._stencil_attachment.has_value()) {
            TRIENGINE_PANIC("Cannot swap stencil attachment with framebuffer without stencil attachment!");
        }

        // Swap stencil attachments
        std::swap(_stencil_attachment.value(), target_fb._stencil_attachment.value());

        // Reattach swapped stencil attachments to the respective framebuffers
        _attach_to_framebuffer(
            _fbo_id,
            _stencil_attachment.value(),
            GL_STENCIL_ATTACHMENT
        );

        _attach_to_framebuffer(
            target_fb._fbo_id,
            target_fb._stencil_attachment.value(),
            GL_STENCIL_ATTACHMENT
        );
    }

    void frame_buffer::swap_depth_stencil_attachment(frame_buffer& target_fb)
    {
        if (!this->is_valid() || !target_fb.is_valid()) {
            TRIENGINE_PANIC("Cannot swap depth-stencil attachment with invalid framebuffer");
        }

        if (_width_pixels != target_fb._width_pixels ||
            _height_pixels != target_fb._height_pixels ||
            _sample_count != target_fb._sample_count)
        {
            TRIENGINE_PANIC("Cannot swap depth-stencil attachments with different sizes or sample count");
        }

        if (!_depth_stencil_attachment.has_value() || !target_fb._depth_stencil_attachment.has_value()) {
            TRIENGINE_PANIC("Cannot swap depth-stencil attachment without depth-stencil attachment");
        }

        // Swap depth-stencil attachments
        std::swap(_depth_stencil_attachment.value(), target_fb._depth_stencil_attachment.value());

        // Reattach swapped depth-stencil attachments to the respective framebuffers
        _attach_to_framebuffer(
            _fbo_id,
            _depth_stencil_attachment.value(),
            GL_DEPTH_STENCIL_ATTACHMENT
        );

        _attach_to_framebuffer(
            target_fb._fbo_id,
            target_fb._depth_stencil_attachment.value(),
            GL_DEPTH_STENCIL_ATTACHMENT
        );
    }

    // ------------------------------
    // Static builder methods
    // ------------------------------

    frame_buffer frame_buffer::create_color_only_buffer(
        const std::initializer_list<GLenum> internal_color_formats, 
        const int32_t width_pixels,
        const int32_t height_pixels,
        const int32_t sample_count,
        const frame_buffer_texture_params_t& color_tex_params,
        const bool use_renderbuffer)
    {
        if (!internal_color_formats.size()) {
            TRIENGINE_PANIC("empty internal color formats");
        }

        frame_buffer new_fb;
        GLCall(::glCreateFramebuffers(1, &new_fb._fbo_id));

        // Single / Multiple(MRT) color attachment(s)
        new_fb._color_attachments.reserve(internal_color_formats.size());
        for (auto cfmt : internal_color_formats) {
            attachment_info_t ci;
            ci.type = attachment_type::color;
            ci.internal_format = cfmt;
            ci.is_render_buffer = use_renderbuffer;
            ci.tex_params = color_tex_params;
            new_fb._color_attachments.push_back(std::move(ci));
        }

        // Allocate & attach
        new_fb.reallocate(width_pixels, height_pixels, sample_count);
        return new_fb;
    }

    frame_buffer frame_buffer::create_depth_only_buffer(
        const GLenum internal_depth_format,
        const int32_t width_pixels,
        const int32_t height_pixels,
        const int32_t sample_count,
        const bool use_renderbuffer)
    {
        frame_buffer new_fb;
        GLCall(::glCreateFramebuffers(1, &new_fb._fbo_id));

        // Depth attachment
        {
            attachment_info_t di;
            di.type = attachment_type::depth;
            di.internal_format = internal_depth_format;
            di.is_render_buffer = use_renderbuffer;
            new_fb._depth_attachment = std::move(di);
        }

        // Allocate & attach
        new_fb.reallocate(width_pixels, height_pixels, sample_count);
        return new_fb;
    }

    frame_buffer frame_buffer::create_stencil_only_buffer(
        const GLenum internal_stencil_format,
        const int32_t width_pixels,
        const int32_t height_pixels,
        const int32_t sample_count,
        const bool use_renderbuffer)
    {
        frame_buffer new_fb;
        GLCall(::glCreateFramebuffers(1, &new_fb._fbo_id));

        // Stencil attachment
        {
            attachment_info_t si;
            si.type = attachment_type::stencil;
            si.internal_format = internal_stencil_format;
            si.is_render_buffer = use_renderbuffer;
            new_fb._stencil_attachment = std::move(si);
        }

        // Allocate & attach
        new_fb.reallocate(width_pixels, height_pixels, sample_count);
        return new_fb;
    }

    frame_buffer frame_buffer::create_color_depth_only_buffer(
        const std::initializer_list<GLenum> internal_color_formats,
        const GLenum internal_depth_format,
        const int32_t width_pixels,
        const int32_t height_pixels,
        const int32_t sample_count,
        const frame_buffer_texture_params_t& color_tex_params,
        const bool use_renderbuffer_for_color,
        const bool use_renderbuffer_for_depth)
    {
        if (!internal_color_formats.size()) {
            TRIENGINE_PANIC("empty internal color formats");
        }

        frame_buffer new_fb;
        GLCall(::glCreateFramebuffers(1, &new_fb._fbo_id));

        // Single / Multiple(MRT) color attachment(s)
        new_fb._color_attachments.reserve(internal_color_formats.size());
        for (auto cfmt : internal_color_formats) {
            attachment_info_t ci;
            ci.type = attachment_type::color;
            ci.internal_format = cfmt;
            ci.is_render_buffer = use_renderbuffer_for_color;
            ci.tex_params = color_tex_params;
            new_fb._color_attachments.push_back(std::move(ci));
        }

        // Depth attachment
        {
            attachment_info_t di;
            di.type = attachment_type::depth;
            di.internal_format = internal_depth_format;
            di.is_render_buffer = use_renderbuffer_for_depth;
            new_fb._depth_attachment = std::move(di);
        }

        // Allocate & attach
        new_fb.reallocate(width_pixels, height_pixels, sample_count);
        return new_fb;
    }

    frame_buffer frame_buffer::create_color_stencil_only_buffer(
        const std::initializer_list<GLenum> internal_color_formats,
        const GLenum internal_stencil_format,
        const int32_t width_pixels,
        const int32_t height_pixels,
        const int32_t sample_count,
        const frame_buffer_texture_params_t& color_tex_params,
        const bool use_renderbuffer_for_color,
        const bool use_renderbuffer_for_stencil)
    {
        if (!internal_color_formats.size()) {
            TRIENGINE_PANIC("empty internal color formats");
        }

        frame_buffer new_fb;
        GLCall(::glCreateFramebuffers(1, &new_fb._fbo_id));

        // Single / Multiple(MRT) color attachment(s)
        new_fb._color_attachments.reserve(internal_color_formats.size());
        for (auto cfmt : internal_color_formats) {
            attachment_info_t ci;
            ci.type = attachment_type::color;
            ci.internal_format = cfmt;
            ci.is_render_buffer = use_renderbuffer_for_color;
            ci.tex_params = color_tex_params;
            new_fb._color_attachments.push_back(std::move(ci));
        }

        // Stencil attachment
        {
            attachment_info_t si;
            si.type = attachment_type::stencil;
            si.internal_format = internal_stencil_format;
            si.is_render_buffer = use_renderbuffer_for_stencil;
            new_fb._stencil_attachment = std::move(si);
        }

        // Allocate & attach
        new_fb.reallocate(width_pixels, height_pixels, sample_count);
        return new_fb;
    }

    frame_buffer frame_buffer::create_color_depth_stencil_buffer(
        const std::initializer_list<GLenum> internal_color_formats,
        const GLenum internal_depth_stencil_format,
        const int32_t width_pixels,
        const int32_t height_pixels,
        const int32_t sample_count, 
        const frame_buffer_texture_params_t& color_tex_params,
        const bool use_renderbuffer_for_color,
        const bool use_renderbuffer_for_depth_stencil)
    {
        if (!internal_color_formats.size()) {
            TRIENGINE_PANIC("empty internal color formats");
        }

        if (internal_depth_stencil_format != GL_DEPTH24_STENCIL8 &&
            internal_depth_stencil_format != GL_DEPTH32F_STENCIL8) {
            TRIENGINE_PANIC("invalid internal depth-stencil format");
        }

        frame_buffer new_fb;
        GLCall(::glCreateFramebuffers(1, &new_fb._fbo_id));

        // Single / Multiple(MRT) color attachment(s)
        new_fb._color_attachments.reserve(internal_color_formats.size());
        for (auto cfmt : internal_color_formats) {
            attachment_info_t ci;
            ci.type = attachment_type::color;
            ci.internal_format = cfmt;
            ci.is_render_buffer = use_renderbuffer_for_color;
            ci.tex_params = color_tex_params;
            new_fb._color_attachments.push_back(std::move(ci));
        }

        // Depth-Stencil attachment
        {
            attachment_info_t dsi;
            dsi.type = attachment_type::depth_stencil;
            dsi.internal_format = internal_depth_stencil_format;
            dsi.is_render_buffer = use_renderbuffer_for_depth_stencil;
            new_fb._depth_stencil_attachment = std::move(dsi);
        }

        // Allocate & attach
        new_fb.reallocate(width_pixels, height_pixels, sample_count);
        return new_fb;
    }

    // ------------------------------
    // Private utility methods
    // ------------------------------

    void frame_buffer::_allocate_attachment_buffer(
        frame_buffer::attachment_info_t& attach_info, 
        const int32_t width_pixels, 
        const int32_t height_pixels, 
        const int32_t sample_count)
    {
        // Deallocate if there's an existing resource
        if (attach_info.buffer_id != kInvalidGLFrameBufferID) {
            _deallocate_attachment_buffer(attach_info);
        }

        const bool multisampled = sample_count > 1;

        if (!attach_info.is_render_buffer)
        {
            //
            // create new texture attachment
            //

            if (!multisampled)
            {
                GLCall(::glCreateTextures(GL_TEXTURE_2D, 1, &attach_info.buffer_id));

                // Set texture parameters (wrap, filter) for display
                GLCall(::glTextureParameteri(attach_info.buffer_id, GL_TEXTURE_WRAP_S, attach_info.tex_params.wrap_s));
                GLCall(::glTextureParameteri(attach_info.buffer_id, GL_TEXTURE_WRAP_T, attach_info.tex_params.wrap_t));
                GLCall(::glTextureParameteri(attach_info.buffer_id, GL_TEXTURE_MIN_FILTER, attach_info.tex_params.min_filter));
                GLCall(::glTextureParameteri(attach_info.buffer_id, GL_TEXTURE_MAG_FILTER, attach_info.tex_params.mag_filter));

                // Allocate texture storage
                GLCall(::glTextureStorage2D(
                    attach_info.buffer_id,       /* GLuint texture */
                    1,                           /* GLsizei levels (1 for no mipmaps) */
                    attach_info.internal_format, /* GLenum internalformat */
                    width_pixels,                /* GLsizei width */
                    height_pixels                /* GLsizei height */
                ));
            }
            else
            {
                GLCall(::glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &attach_info.buffer_id));

                // Allocate multisample texture storage
                GLCall(::glTextureStorage2DMultisample(
                    attach_info.buffer_id,       /* GLuint texture */
                    sample_count,                /* GLsizei samples */
                    attach_info.internal_format, /* GLenum internalformat */
                    width_pixels,                /* GLsizei width */
                    height_pixels,               /* GLsizei height */
                    GL_TRUE                      /* GLboolean fixedsamplelocations */
                ));
            }

            /** 
             * For combined depth-stencil formats, ensure that sampling operations
             * in shaders retrieve depth data by default. Although `GL_DEPTH_COMPONENT`
             * is the standard initial state, we explicitly set it here to guarantee
             * consistent behavior when the texture is accessed via a sampler.
             */
            switch (attach_info.internal_format) {
            case GL_DEPTH24_STENCIL8:
            case GL_DEPTH32F_STENCIL8:
                GLCall(::glTextureParameteri(attach_info.buffer_id, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT));
                break;
            }
        }
        else
        {
            //
            // create new renderbuffer attachment
            //

            GLCall(::glCreateRenderbuffers(1, &attach_info.buffer_id));

            if (!multisampled)
            {
                GLCall(::glNamedRenderbufferStorage(
                    attach_info.buffer_id,       /* GLuint renderbuffer */
                    attach_info.internal_format, /* GLenum internalformat */
                    width_pixels,                /* GLsizei width */
                    height_pixels                /* GLsizei height */
                ));
            }
            else
            {
                GLCall(::glNamedRenderbufferStorageMultisample(
                    attach_info.buffer_id,       /* GLuint renderbuffer */
                    sample_count,                /* GLsizei samples */
                    attach_info.internal_format, /* GLenum internalformat */
                    width_pixels,                /* GLsizei width */
                    height_pixels                /* GLsizei height */
                ));
            }
        }
    }

    void frame_buffer::_deallocate_attachment_buffer(
        attachment_info_t& attach_info)
    {
        if (attach_info.buffer_id != kInvalidGLFrameBufferID) {
            if (attach_info.is_render_buffer) {
                ::glDeleteRenderbuffers(1, &attach_info.buffer_id);
            } else {
                ::glDeleteTextures(1, &attach_info.buffer_id);
            }
            attach_info.buffer_id = kInvalidGLFrameBufferID;
        }
    }

    void frame_buffer::_attach_to_framebuffer(
        const GLuint target_fbo_id,
        const attachment_info_t& attach_info, 
        const GLenum attach_point)
    {
        if (!attach_info.is_render_buffer)
        {
            // GLuint framebuffer, GLenum attachment, GLuint texture, GLint level
            GLCall(::glNamedFramebufferTexture(
                target_fbo_id,             /* GLuint framebuffer */
                attach_point,              /* GLenum attachment */
                attach_info.buffer_id,     /* GLuint texture */
                0                          /* GLint level */
            ));
        }
        else
        {
            GLCall(::glNamedFramebufferRenderbuffer(
                target_fbo_id,             /* GLuint framebuffer */
                attach_point,              /* GLenum attachment */
                GL_RENDERBUFFER,           /* GLenum renderbuffertarget */
                attach_info.buffer_id      /* GLuint renderbuffer */
            ));
        }
    }

} // namespace