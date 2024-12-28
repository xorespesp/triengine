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
        *this = std::move(rhs);
    }

    frame_buffer& frame_buffer::operator=(frame_buffer&& rhs) noexcept
    {
        if (this != &rhs) {
            std::swap(_fbo, rhs._fbo);
            std::swap(_color_texture, rhs._color_texture);
            std::swap(_depth_stencil_rbo, rhs._depth_stencil_rbo);
            std::swap(_width_pixels, rhs._width_pixels);
            std::swap(_height_pixels, rhs._height_pixels);
            std::swap(_sample_count, rhs._sample_count);
            rhs.destroy();
        }

        return *this;
    }

    bool frame_buffer::is_valid() const noexcept
    {
        return _fbo != kInvalidObjectID;
    }

    void frame_buffer::reserve(
        const int32_t width_pixels, 
        const int32_t height_pixels,
        const int32_t sample_count)
    {
        if (!width_pixels || !height_pixels) {
            TRIENGINE_PANIC("invalid frame buffer size");
        }

        if (sample_count <= 0) {
            TRIENGINE_PANIC("invalid sample count");
        }

        const bool realloc_needed =
            _width_pixels != width_pixels ||
            _height_pixels != height_pixels ||
            _sample_count != sample_count;

        if (realloc_needed)
        {
            if (this->is_valid()) {
                this->destroy();
            }

            // new fbo
            GLuint new_fbo{};
            GLCall(::glGenFramebuffers(1, &new_fbo));

            // new texture (color buffer)
            GLuint new_color_texture{};
            GLCall(::glGenTextures(1, &new_color_texture));

            // new renderbuffer (depth, stencil buffer)
            GLuint new_depth_stencil_rbo{};
            GLCall(::glGenRenderbuffers(1, &new_depth_stencil_rbo));

            _allocate_framebuffer(
                new_fbo,
                new_color_texture,
                new_depth_stencil_rbo,
                width_pixels,
                height_pixels,
                sample_count
            );

            _fbo = new_fbo;
            _color_texture = new_color_texture;
            _depth_stencil_rbo = new_depth_stencil_rbo;
            _width_pixels = width_pixels;
            _height_pixels = height_pixels;
            _sample_count = sample_count;
        }

    }

    void frame_buffer::destroy() noexcept
    {
        if (this->is_valid()) {
            ::glDeleteTextures(1, &_color_texture);
            ::glDeleteRenderbuffers(1, &_depth_stencil_rbo);
            ::glDeleteFramebuffers(1, &_fbo);
        }
        _fbo = _color_texture = _depth_stencil_rbo = kInvalidObjectID;
        _width_pixels = _height_pixels = 0;
        _sample_count = 1;
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

    void frame_buffer::blit_to(
        frame_buffer& target_fb,
        const bool blit_color,
        const bool blit_depth,
        const bool blit_stencil)
    {
        if (!this->is_valid() || !target_fb.is_valid()) {
            TRIENGINE_PANIC("Cannot blit from/to invalid framebuffer!");
        }

        GLCall(::glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo));
        GLCall(::glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target_fb._fbo));

        GLbitfield mask{};
        if (blit_color) { mask |= GL_COLOR_BUFFER_BIT; }
        if (blit_depth) { mask |= GL_DEPTH_BUFFER_BIT; }
        if (blit_stencil) { mask |= GL_STENCIL_BUFFER_BIT; }

        GLCall(::glBlitFramebuffer(
            0, 0, _width_pixels, _height_pixels,                     /* GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1 */
            0, 0, target_fb._width_pixels, target_fb._height_pixels, /* GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1 */
            mask,                                                    /* GLbitfield mask */
            GL_NEAREST                                               /* GLenum filter */
        ));

        GLCall(::glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));
        GLCall(::glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
    }

    void frame_buffer::_allocate_framebuffer(
        const GLuint fbo,
        const GLuint color_texture, 
        const GLuint depth_stencil_rbo,
        const int32_t width_pixels, 
        const int32_t height_pixels,
        const int32_t sample_count)
    {
        const bool multisampled = sample_count > 1;

        // bind current fbo
        GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, fbo));

        /// TODO: Support MRT(Multi Render Target)

        // create(or rescale) texture (color buffer)
        if (!multisampled)
        {
            GLCall(::glBindTexture(GL_TEXTURE_2D, color_texture));

            // Setup texture wrapping/filtering options for display
            GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
            GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
            GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
            GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

            // Ref: https://docs.gl/gl4/glTexImage2D
            // NOTE: Passing a NULL pointer as the `pixels` parameter (the last argument) to `glTexImage2D` is a valid usage scenario. 
            //       In this case, no actual texture image data is uploaded to the GPU;
            //       instead, the function simply allocates memory for an "empty" texture with the specified dimensions and format.
            constexpr GLenum tex_format = GL_RGB;
            GLCall(::glTexImage2D(
                GL_TEXTURE_2D,                     /*GLenum target*/
                0,                                 /*GLint level*/
                GL_RGB,                            /*GLint internalformat*/
                width_pixels,                      /*GLsizei width*/
                height_pixels,                     /*GLsizei height*/
                0,                                 /*GLint border*/
                tex_format,                        /*GLenum format*/
                GL_UNSIGNED_BYTE,                  /*GLenum type*/
                nullptr                            /*const void *pixels*/
            ));

            GLCall(::glBindTexture(GL_TEXTURE_2D, 0));
        }
        else
        {
            GLCall(::glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, color_texture));

            GLCall(::glTexImage2DMultisample(
                GL_TEXTURE_2D_MULTISAMPLE, /* GLenum target */
                sample_count,              /* GLsizei samples */
                GL_RGB,                    /* GLenum internalformat */
                width_pixels,              /* GLsizei width */
                height_pixels,             /* GLsizei height */
                GL_TRUE                    /* GLboolean fixedsamplelocations */
            ));

            GLCall(::glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0));
        }

        // create(or rescale) renderbuffer (depth, stencil buffer)
        {
            GLCall(::glBindRenderbuffer(GL_RENDERBUFFER, depth_stencil_rbo));

            if (!multisampled)
            {
                GLCall(::glRenderbufferStorage(
                    GL_RENDERBUFFER,     /* GLenum target */
                    GL_DEPTH24_STENCIL8, /* GLenum internalformat */
                    width_pixels,        /* GLsizei width */
                    height_pixels        /* GLsizei height */
                ));
            }
            else
            {
                GLCall(::glRenderbufferStorageMultisample(
                    GL_RENDERBUFFER,     /* GLenum target */
                    sample_count,        /* GLsizei samples */
                    GL_DEPTH24_STENCIL8, /* GLenum internalformat */
                    width_pixels,        /* GLsizei width */
                    height_pixels        /* GLsizei height */
                ));
            }

            GLCall(::glBindRenderbuffer(GL_RENDERBUFFER, 0));
        }

        // attach(or reattach) texture (color buffer)
        const GLenum tex_target = (!multisampled) ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
        GLCall(::glFramebufferTexture2D(
            GL_FRAMEBUFFER,       /* GLenum target */
            GL_COLOR_ATTACHMENT0, /* GLenum attachment */
            tex_target,           /* GLenum textarget */
            color_texture,        /* GLuint texture */
            0                     /* GLint level */
        ));

        // attach(or reattach) renderbuffer (depth, stencil buffer)
        GLCall(::glFramebufferRenderbuffer(
            GL_FRAMEBUFFER,              /* GLenum target */
            GL_DEPTH_STENCIL_ATTACHMENT, /* GLenum attachment */
            GL_RENDERBUFFER,             /* GLenum renderbuffertarget */
            depth_stencil_rbo            /* GLuint renderbuffer */
        ));

        if (::glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            TRIENGINE_PANIC("Framebuffer creation has not been completed!");
        }

        // restore to prev fbo
        GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }

} // namespace