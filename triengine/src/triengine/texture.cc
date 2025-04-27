#include <triengine/texture.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>
#include <triengine/extern/stb_image.h>

#include <optional>
#include <array>

namespace triengine
{
    namespace {

        image_format_type _try_map_image_format_from_num_channels(
            const size_t num_channels)
        {
            switch (num_channels) {
            case 1:  return image_format_type::greyscale;
            case 2:  return image_format_type::rg;
            case 3:  return image_format_type::rgb;
            case 4:  return image_format_type::rgba;
            default: return image_format_type::invalid;
            }
        }

        bool _try_convert_image_format_to_texture_format(
            const image_format_type image_format,
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
                texture_internal_format = GL_RGB8;
                texture_format = GL_RGB;
                return true;
            case image_format_type::rgba:
                // Using GL_RGBA8 as an 8-bit internal format.
                texture_internal_format = GL_RGBA8; 
                texture_format = GL_RGBA;
                return true;
            default:
                return false;
            }
        }

    } // namespace

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
            std::swap(_texture_id, rhs._texture_id);
            std::swap(_image_format, rhs._image_format);
            std::swap(_width_pixels, rhs._width_pixels);
            std::swap(_height_pixels, rhs._height_pixels);
            std::swap(_params, rhs._params);
        }

        return *this;
    }

    bool texture_2d::is_valid() const noexcept {
        return _texture_id != kInvalidTextureID;
    }

    void texture_2d::create_from_memory(
        const uint8_t* const image_buffer,
        const image_format_type image_format,
        const int32_t width_pixels,
        const int32_t height_pixels,
        const texture_params_t& params,
        const bool generate_mipmap)
    {
        if (!image_buffer) {
            TRIENGINE_PANIC("invalid image buffer");
        }

        if (!width_pixels || !height_pixels) {
            TRIENGINE_PANIC("invalid image size");
        }

        GLenum internal_format{}, format{};
        if (!_try_convert_image_format_to_texture_format(
            image_format,
            internal_format,
            format))
        {
            TRIENGINE_PANIC("unsupported image format.");
        }

        // Create & Bind Texture
        GLuint new_tex_id{ kInvalidTextureID };
        GLCall(::glGenTextures(1, &new_tex_id));
        GLCall(::glBindTexture(GL_TEXTURE_2D, new_tex_id));

        // If user wants mipmaps but min_filter is not a mipmap-based filter, adjust it automatically
        GLint adjusted_min_filter{ params.min_filter };
        if (generate_mipmap && (params.min_filter == GL_LINEAR || params.min_filter == GL_NEAREST)) {
            TRIENGINE_TRACE("Warning: Override texture min filter options to GL_LINEAR_MIPMAP_LINEAR");
            adjusted_min_filter = GL_LINEAR_MIPMAP_LINEAR;
        }

        // Set texture parameters (wrap, filter) for display
        GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, params.wrap_s));
        GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, params.wrap_t));
        GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, adjusted_min_filter));
        GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, params.mag_filter));

        // Upload pixels to GPU
        // Ref: https://docs.gl/gl4/glTexImage2D
        GLCall(::glTexImage2D(
            GL_TEXTURE_2D,                     /*GLenum target*/
            0,                                 /*GLint level*/
            internal_format,                   /*GLint internalformat*/
            width_pixels,                      /*GLsizei width*/
            height_pixels,                     /*GLsizei height*/
            0,                                 /*GLint border*/
            format,                            /*GLenum format*/
            GL_UNSIGNED_BYTE,                  /*GLenum type*/
            image_buffer                       /*const void *pixels*/
        ));

        // Generate mipmaps if requested
        if (generate_mipmap) {
            GLCall(::glGenerateMipmap(GL_TEXTURE_2D));
        }

        GLCall(::glBindTexture(GL_TEXTURE_2D, 0));

        if (this->is_valid()) {
            this->destroy();
        }

        _texture_id = new_tex_id;
        _image_format = image_format;
        _width_pixels = width_pixels;
        _height_pixels = height_pixels;
    }

    void texture_2d::create_from_file(
        const std::filesystem::path& path,
        const texture_params_t& params,
        const bool generate_mipmap,
        const bool flip_image)
    {
        if (!std::filesystem::is_regular_file(path)) {
            TRIENGINE_PANIC("invalid texture file path");
        }

        int32_t width_pixels{}, height_pixels{}, num_channels{};
        ::stbi_set_flip_vertically_on_load(flip_image); // TODO: consider thread-safety
        std::unique_ptr<stbi_uc, decltype(&::stbi_image_free)> image_data{
            ::stbi_load(
                path.string().c_str(),
                &width_pixels,
                &height_pixels,
                &num_channels,
                0
            ),
            ::stbi_image_free
        };

        if (!image_data) {
            TRIENGINE_PANIC("failed to load texture file: %s", path.generic_u8string().c_str());
        }

        this->create_from_memory(
            image_data.get(),
            _try_map_image_format_from_num_channels(num_channels),
            width_pixels,
            height_pixels,
            params,
            generate_mipmap
        );
    }

    void texture_2d::create_from_uniform_color(
        const color3_f32& color)
    {
        // Convert float color to 0-255 range
        const std::array<uint8_t, 3> pixel_buff{
            static_cast<uint8_t>(std::clamp(color.r() * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(color.g() * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(color.b() * 255.0f, 0.0f, 255.0f))
        };

        this->create_from_memory(
            pixel_buff.data(),
            image_format_type::rgb,
            1,
            1,
            texture_params_t{},
            false
        );
    }

    void texture_2d::destroy() noexcept
    {
        if (this->is_valid()) {
            ::glDeleteTextures(1, &_texture_id);
        }
        _texture_id = kInvalidTextureID;
        _image_format = image_format_type::invalid;
        _width_pixels = _height_pixels = 0;
    }

    std::string texture_2d::dump() const
    {
        return utility::string::c_format(""
            "texture id: 0x%X\n"
            "image format: %d\n"
            "image size: %dx%d\n"
            , _texture_id
            , static_cast<GLenum>(_image_format)
            , _width_pixels
            , _height_pixels
        );
    }

} // namespace