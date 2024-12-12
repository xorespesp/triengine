#include "texture.hh"
#include "misc/debug_utils.hh"
#include "misc/opengl_utils.hh"

#define STB_IMAGE_IMPLEMENTATION
#include "extern/stb_image.h"

#include <optional>
#include <array>

namespace triengine
{
    texture_2d::~texture_2d()
    {
        if (this->is_valid()) {
            this->destroy();
        }
    }

    texture_2d::texture_2d(texture_2d&& rhs) noexcept
    {
        std::swap(_texture_id, rhs._texture_id);
        std::swap(_image_format, rhs._image_format);
        std::swap(_width_pixels, rhs._width_pixels);
        std::swap(_height_pixels, rhs._height_pixels);
        rhs.destroy();
    }

    texture_2d& texture_2d::operator=(texture_2d&& rhs) noexcept
    {
        if (this != &rhs) {
            std::swap(_texture_id, rhs._texture_id);
            std::swap(_image_format, rhs._image_format);
            std::swap(_width_pixels, rhs._width_pixels);
            std::swap(_height_pixels, rhs._height_pixels);
            rhs.destroy();
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
        const int32_t height_pixels)
    {
        if (!image_buffer) {
            TRIENGINE_PANIC("invalid image buffer");
        }

        if (image_format == image_format_type::invalid) {
            TRIENGINE_PANIC("invalid image format");
        }

        if (!width_pixels || !height_pixels) {
            TRIENGINE_PANIC("invalid image size");
        }

        // Create & Bind Texture
        GLuint new_tex_id{ kInvalidTextureID };
        GLCall(::glGenTextures(1, &new_tex_id));
        GLCall(::glBindTexture(GL_TEXTURE_2D, new_tex_id));

        // Setup texture wrapping/filtering options for display
        GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
        GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
        GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GLCall(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

        // Ref: https://docs.gl/gl4/glTexImage2D
        GLCall(::glTexImage2D(
            GL_TEXTURE_2D,                     /*GLenum target*/
            0,                                 /*GLint level*/
            GL_RGB,                            /*GLint internalformat*/
            width_pixels,                      /*GLsizei width*/
            height_pixels,                     /*GLsizei height*/
            0,                                 /*GLint border*/
            static_cast<GLenum>(image_format), /*GLenum format*/
            GL_UNSIGNED_BYTE,                  /*GLenum type*/
            image_buffer                       /*const void *pixels*/
        ));
        GLCall(::glGenerateMipmap(GL_TEXTURE_2D));

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
        const bool flip_image)
    {
        if (!std::filesystem::is_regular_file(path)) {
            TRIENGINE_PANIC("invalid texture file path");
        }

        int32_t width_pixels{}, height_pixels{}, num_channels{};
        ::stbi_set_flip_vertically_on_load(flip_image);
        std::unique_ptr<stbi_uc, void(*)(stbi_uc*)> image_data{
            ::stbi_load(
                path.string().c_str(),
                &width_pixels,
                &height_pixels,
                &num_channels,
                0
            ),
            +[](stbi_uc* p) { ::stbi_image_free(p); }
        };

        if (!image_data) {
            TRIENGINE_PANIC("failed to load texture file: %s", path.generic_u8string().c_str());
        }

        const auto image_format = [num_channels]() -> image_format_type {
            switch (num_channels) {
            case 1: return image_format_type::greyscale;
            case 3: return image_format_type::rgb;
            case 4: return image_format_type::rgba;
            default: return image_format_type::invalid;
            }
            }();

        this->create_from_memory(
            image_data.get(),
            image_format,
            width_pixels,
            height_pixels
        );
    }

    void texture_2d::create_from_uniform_color(
        const color3_f32& color)
    {
        const std::array<uint8_t, 3> pixel_buff{
            static_cast<uint8_t>(std::clamp(color.r() * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(color.g() * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(color.b() * 255.0f, 0.0f, 255.0f))
        };

        this->create_from_memory(
            pixel_buff.data(),
            image_format_type::rgb,
            1,
            1
        );
    }

    void texture_2d::destroy() noexcept
    {
        if (this->is_valid()) {
            //GLCall(::glDeleteTextures(1, &_texture_id));
            ::glDeleteTextures(1, &_texture_id);
        }
        _texture_id = kInvalidTextureID;
        _image_format = image_format_type::invalid;
        _width_pixels = _height_pixels = 0;
    }

    std::string texture_2d::dump() const
    {
        return misc::string::c_format(""
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