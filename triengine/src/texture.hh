#pragma once
#include "common.h"

#include "extern/glad/glad.h"
#include <GLFW/glfw3.h>

#include <filesystem>

namespace triengine
{
    // Ref: 
    // https://stackoverflow.com/a/4745945
    // https://stackoverflow.com/a/34497547
    enum class image_format_type : GLenum
    {
        invalid = 0,
        greyscale = GL_RED, // 1 channel (Ref: https://stackoverflow.com/a/69113182)
        rgb = GL_RGB, // 3 channel
        bgr = GL_BGR, // 3 channel (NOTE: not a internal format, just a format)
        rgba = GL_RGBA, // 4 channel
        bgra = GL_BGRA, // 4 channel (NOTE: not a internal format, just a format)
    };

    // Represents a texture (include material)
    // Refs:
    // https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples#example-for-opengl-users
    // https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples#about-texture-coordinates
    // https://learnopengl.com/Getting-started/Textures
    class texture_2d
    {
    private:
        static constexpr GLuint kInvalidTextureID{ static_cast<GLuint>(-1) };

    private:
        GLuint _texture_id{ kInvalidTextureID };
        image_format_type _image_format{ image_format_type::invalid };
        int32_t _width_pixels{}, _height_pixels{};

    public:
        texture_2d() = default;

        texture_2d(const uint8_t* image_buffer, image_format_type image_format, int32_t width_pixels, int32_t height_pixels) {
            this->create_from_memory(image_buffer, image_format, width_pixels, height_pixels);
        }

        texture_2d(const std::filesystem::path& path, bool flip_image = true) {
            this->create_from_file(path, flip_image);
        }

        texture_2d(const color3_f32& color) {
            this->create_from_uniform_color(color);
        }

        ~texture_2d();
        texture_2d(texture_2d&& rhs) noexcept;
        texture_2d& operator=(texture_2d&& rhs) noexcept;
        texture_2d(const texture_2d&) = delete;
        texture_2d& operator=(const texture_2d&) = delete;

        GLuint id() const noexcept { return _texture_id; }
        image_format_type image_format() const noexcept { return _image_format; }
        int32_t width_pixels() const noexcept { return _width_pixels; }
        int32_t height_pixels() const noexcept { return _height_pixels; }

        bool is_valid() const noexcept;

        void create_from_memory(
            const uint8_t* image_buffer,
            image_format_type image_format,
            int32_t width_pixels,
            int32_t height_pixels
        );

        void create_from_file(
            const std::filesystem::path& path,
            bool flip_image = true
        );

        // create single color texture
        void create_from_uniform_color(
            const color3_f32& color
        );

        void destroy() noexcept;

        std::string dump() const;

    }; // class

} // namespace