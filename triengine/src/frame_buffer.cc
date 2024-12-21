#include "frame_buffer.hh"
#include "misc/opengl_utils.hh"
#include "misc/debug_utils.hh"

namespace triengine
{
    frame_buffer::~frame_buffer()
    {
        if (this->is_valid()) {
            this->destroy();
        }
    }

    frame_buffer::frame_buffer(frame_buffer&& rhs) noexcept
    {
        std::swap(_fbo, rhs._fbo);
        std::swap(_tex_color, rhs._tex_color);
        std::swap(_rbo_depth_stencil, rhs._rbo_depth_stencil);
        std::swap(_width_pixels, rhs._width_pixels);
        std::swap(_height_pixels, rhs._height_pixels);
        rhs.destroy();
    }

    frame_buffer& frame_buffer::operator=(frame_buffer&& rhs) noexcept
    {
        if (this != &rhs) {
            std::swap(_fbo, rhs._fbo);
            std::swap(_tex_color, rhs._tex_color);
            std::swap(_rbo_depth_stencil, rhs._rbo_depth_stencil);
            std::swap(_width_pixels, rhs._width_pixels);
            std::swap(_height_pixels, rhs._height_pixels);
            rhs.destroy();
        }

        return *this;
    }

    bool frame_buffer::is_valid() const noexcept
    {
        return _fbo != kInvalidBufferObjectID;
    }

    void frame_buffer::reserve(
        const int32_t width_pixels, 
        const int32_t height_pixels)
    {
        if (!width_pixels || !height_pixels) {
            TRIENGINE_PANIC("invalid frame buffer size");
        }

        const auto initialize_framebuffer =
            [](
                GLuint fbo,
                texture_2d& tex_color,
                GLuint rbo_depth_stencil,
                int32_t width_pixels,
                int32_t height_pixels
                )
            {
                // bind fbo
                GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, fbo));

                // create(or rescale) texture (color buffer)
                tex_color.reserve(image_format_type::rgb, width_pixels, height_pixels);

                // create(or rescale) renderbuffer (depth, stencil buffer)
                GLCall(::glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth_stencil));
                GLCall(::glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width_pixels, height_pixels));
                GLCall(::glBindRenderbuffer(GL_RENDERBUFFER, 0));

                // attach(or reattach) texture (color buffer)
                GLCall(::glFramebufferTexture2D(
                    GL_FRAMEBUFFER,       /* GLenum target */
                    GL_COLOR_ATTACHMENT0, /* GLenum attachment */
                    GL_TEXTURE_2D,        /* GLenum textarget */
                    tex_color.id(),       /* GLuint texture */
                    0                     /* GLint level */
                ));

                // attach(or reattach) renderbuffer (depth, stencil buffer)
                GLCall(::glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER,              /* GLenum target */
                    GL_DEPTH_STENCIL_ATTACHMENT, /* GLenum attachment */
                    GL_RENDERBUFFER,             /* GLenum renderbuffertarget */
                    rbo_depth_stencil            /* GLuint renderbuffer */
                ));

                if (::glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                    TRIENGINE_PANIC("Framebuffer creation has not been completed!");
                }

                GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, 0));
            };

        if (!this->is_valid())
        {
            /// Create new fbo

            // new fbo
            GLuint new_fbo{};
            GLCall(::glGenFramebuffers(1, &new_fbo));

            // new texture (color buffer)
            texture_2d new_tex_color;

            // new renderbuffer (depth, stencil buffer)
            GLuint new_rbo_depth_stencil{};
            GLCall(::glGenRenderbuffers(1, &new_rbo_depth_stencil));

            initialize_framebuffer(
                new_fbo,
                new_tex_color,
                new_rbo_depth_stencil,
                width_pixels,
                height_pixels
            );

            _fbo = new_fbo;
            _tex_color = std::move(new_tex_color);
            _rbo_depth_stencil = new_rbo_depth_stencil;
        }
        else if (
            _width_pixels != width_pixels ||
            _height_pixels != height_pixels
            )
        {
            /// Rescale existing fbo

            initialize_framebuffer(
                _fbo,
                _tex_color,
                _rbo_depth_stencil,
                width_pixels,
                height_pixels
            );
        }

        _width_pixels = width_pixels;
        _height_pixels = height_pixels;
    }

    void frame_buffer::destroy() noexcept
    {
        if (this->is_valid()) {
            _tex_color.destroy();
            ::glDeleteRenderbuffers(1, &_rbo_depth_stencil);
            ::glDeleteFramebuffers(1, &_fbo);
        }
        _fbo = _rbo_depth_stencil = kInvalidBufferObjectID;
        _width_pixels = _height_pixels = 0;
    }

    void frame_buffer::bind()
    {
        TRIENGINE_ASSERT(this->is_valid());
        GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _fbo));
    }

    void frame_buffer::unbind()
    {
        GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }

} // namespace