#include "texture.hh"

#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

namespace triengine::core
{
    namespace {

        bool _try_map_image_format_to_texture_format(
            const image_format_type image_format,
            const bool apply_gamma_correction,
            GLenum& texture_internal_format/* out */,
            GLenum& texture_format/* out */)
        {
            switch (image_format) {
            case image_format_type::greyscale:
                // Internal format GL_R8 (OpenGL 3.0+).
                // For older OpenGL, consider using GL_RED or GL_LUMINANCE.
                texture_internal_format = GL_R8;
                texture_format = GL_RED;
                return true;
            case image_format_type::rg:
                texture_internal_format = GL_RG8;
                texture_format = GL_RG;
                return true;
            case image_format_type::rgb:
                // Using GL_RGB8 as a recommended 8-bit internal format.
                // GL_RGB is the "classic" but for modern usage, GL_RGB8 is more explicit.
                texture_internal_format = apply_gamma_correction ? GL_SRGB8 : GL_RGB8;
                texture_format = GL_RGB;
                return true;
            case image_format_type::bgr:
                // Note: GL_BGR is not a valid internal format, just a transfer format.
                texture_internal_format = apply_gamma_correction ? GL_SRGB8 : GL_RGB8;
                texture_format = GL_BGR;
                return true;
            case image_format_type::rgba:
                // Using GL_RGBA8 as an 8-bit internal format.
                texture_internal_format = apply_gamma_correction ? GL_SRGB8_ALPHA8 : GL_RGBA8;
                texture_format = GL_RGBA;
                return true;
            case image_format_type::bgra:
                // Note: GL_BGRA is not a valid internal format, just a transfer format.
                texture_internal_format = apply_gamma_correction ? GL_SRGB8_ALPHA8 : GL_RGBA8;
                texture_format = GL_BGRA;
                return true;
            default:
                return false;
            }
        }

        class scoped_gl_pixel_store_guard
            : utility::noncopyable
        {
        public:
            scoped_gl_pixel_store_guard(GLenum pname, GLint new_value)
                : _pname{ pname }
            {
                ::glGetIntegerv(_pname, &_prev_value);
                if (_prev_value != new_value) {
                    ::glPixelStorei(_pname, new_value);
                }
            }

            ~scoped_gl_pixel_store_guard()
            {
                ::glPixelStorei(_pname, _prev_value);
            }

        private:
            GLenum _pname{};
            GLint _prev_value{};
        };

    } // namespace

    texture_2d::texture_2d(
        const image_buffer& tex_image,
        const texture_params_t& tex_params)
    {
        if (tex_image.empty()) {
            TRIENGINE_PANIC("invalid texture image");
        }

        GLenum internal_format{}, format{};
        if (!_try_map_image_format_to_texture_format(
            tex_image.format(),
            tex_params.gamma_correction,
            internal_format,
            format))
        {
            TRIENGINE_PANIC("unsupported image format.");
        }

        // Create & Bind Texture
        GLuint new_tex_id{ kInvalidGLTextureID };
        GLCall(::glCreateTextures(GL_TEXTURE_2D, 1, &new_tex_id));

        // If user wants mipmaps but min_filter is not a mipmap-based filter, adjust it automatically
        GLint adjusted_min_filter{ tex_params.min_filter };
        if (tex_params.generate_mipmap && (tex_params.min_filter == GL_LINEAR || tex_params.min_filter == GL_NEAREST)) {
            TRIENGINE_WARN("Override texture min filter options to GL_LINEAR_MIPMAP_LINEAR");
            adjusted_min_filter = GL_LINEAR_MIPMAP_LINEAR;
        }

        // Set texture parameters (wrap, filter) for display
        GLCall(::glTextureParameteri(new_tex_id, GL_TEXTURE_WRAP_S, tex_params.wrap_s));
        GLCall(::glTextureParameteri(new_tex_id, GL_TEXTURE_WRAP_T, tex_params.wrap_t));
        GLCall(::glTextureParameteri(new_tex_id, GL_TEXTURE_MIN_FILTER, adjusted_min_filter));
        GLCall(::glTextureParameteri(new_tex_id, GL_TEXTURE_MAG_FILTER, tex_params.mag_filter));

        // Allocate (immutable) GPU storage for the texture
        // Ref: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexStorage2D.xhtml
        const GLsizei num_mipmap_levels = tex_params.generate_mipmap
            ? static_cast<GLsizei>(std::floor(std::log2f(static_cast<float>(std::max(tex_image.width_pixels(), tex_image.height_pixels()))))) + 1
            : static_cast<GLsizei>(1); // 1 means no mipmaps

        GLCall(::glTextureStorage2D(
            new_tex_id,                                     /* GLuint texture */
            num_mipmap_levels,                              /* GLsizei levels */
            internal_format,                                /* GLenum internalformat */
            static_cast<GLsizei>(tex_image.width_pixels()), /* GLsizei width */
            static_cast<GLsizei>(tex_image.height_pixels()) /* GLsizei height */
        ));

        {
            // Temporarily set pixel unpack alignment to 1 to avoid issues with image buffer alignment
            scoped_gl_pixel_store_guard unpack_alignment_guard{ GL_UNPACK_ALIGNMENT, 1 };

            // Upload the image data to the texture GPU storage
            GLCall(::glTextureSubImage2D(
                new_tex_id,                                      /* GLuint texture */
                0,                                               /* GLint level */
                0,                                               /* GLint xoffset */
                0,                                               /* GLint yoffset */
                static_cast<GLsizei>(tex_image.width_pixels()),  /* GLsizei width */
                static_cast<GLsizei>(tex_image.height_pixels()), /* GLsizei height */
                format,                                          /* GLenum format */
                GL_UNSIGNED_BYTE,                                /* GLenum type */
                tex_image.data()                                 /* const void* pixels */
            ));
        }

        // Generate mipmaps if requested
        if (tex_params.generate_mipmap) {
            GLCall(::glGenerateTextureMipmap(new_tex_id));
        }

        if (this->is_valid()) {
            this->destroy();
        }

        _tex_id = new_tex_id;
        _image_format = tex_image.format();
        _width_pixels = tex_image.width_pixels();
        _height_pixels = tex_image.height_pixels();
        _tex_params = tex_params;
    }

    texture_2d::~texture_2d()
    {
        if (this->is_valid()) {
            this->destroy();
        }
    }

    texture_2d::texture_2d(texture_2d&& rhs) noexcept
    {
        *this = std::move(rhs);
    }

    texture_2d& texture_2d::operator=(texture_2d&& rhs) noexcept
    {
        if (this != &rhs) {
            if (this->is_valid()) {
                this->destroy();
            }
            std::swap(_tex_id, rhs._tex_id);
            std::swap(_image_format, rhs._image_format);
            std::swap(_width_pixels, rhs._width_pixels);
            std::swap(_height_pixels, rhs._height_pixels);
            std::swap(_tex_params, rhs._tex_params);
        }

        return *this;
    }

    bool texture_2d::is_valid() const noexcept {
        return _tex_id != kInvalidGLTextureID;
    }

    void texture_2d::destroy() noexcept
    {
        if (this->is_valid()) {
            ::glDeleteTextures(1, &_tex_id);
        }
        _tex_id = kInvalidGLTextureID;
        _image_format = image_format_type::invalid;
        _width_pixels = _height_pixels = 0;
    }

} // namespace