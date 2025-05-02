#pragma once
#include <triengine/common.h>
#include <triengine/image.hh>
#include <triengine/utility/noncopyable.hh>

#include <glad/glad.h>

#include <filesystem>

namespace triengine
{
    // Structure that holds texture wrapping and filtering parameters
    struct texture_params_t
    {
        // Wrapping mode for S and T axes
        GLint wrap_s{ GL_CLAMP_TO_EDGE };
        GLint wrap_t{ GL_CLAMP_TO_EDGE };

        // Filtering mode for minification and magnification
        GLint min_filter{ GL_LINEAR };
        GLint mag_filter{ GL_LINEAR };
    };

    class texture_2d
        : utility::noncopyable
    {
    private:
        static constexpr GLuint kInvalidTextureID{ 0u };

    private:
        GLuint _texture_id{ kInvalidTextureID };
        image_format_type _image_format{ image_format_type::invalid };
        int32_t _width_pixels{}, _height_pixels{};
        texture_params_t _params{};

    public:
        texture_2d() = default;

        texture_2d(
            const uint8_t* image_buffer, 
            image_format_type image_format, 
            int32_t width_pixels, int32_t height_pixels,
            const texture_params_t& params = {},
            bool generate_mipmap = true)
        {
            this->create_from_memory(
                image_buffer, 
                image_format, 
                width_pixels, height_pixels, 
                params,
                generate_mipmap
            );
        }

        texture_2d(
            const std::filesystem::path& path,
            const texture_params_t& params = {},
            bool generate_mipmap = true,
            bool flip_image = true)
        {
            this->create_from_file(
                path, 
                params,
                generate_mipmap,
                flip_image
            );
        }

        texture_2d(const color3_f32& color) {
            this->create_from_uniform_color(color);
        }

        ~texture_2d();
        texture_2d(texture_2d&& rhs) noexcept;
        texture_2d& operator=(texture_2d&& rhs) noexcept;

        GLuint id() const noexcept { return _texture_id; }
        image_format_type image_format() const noexcept { return _image_format; }
        int32_t width_pixels() const noexcept { return _width_pixels; }
        int32_t height_pixels() const noexcept { return _height_pixels; }
        const texture_params_t& params() const noexcept { return _params; }

        bool is_valid() const noexcept;

        // Create texture from raw memory
        void create_from_memory(
            const uint8_t* image_buffer,
            image_format_type image_format,
            int32_t width_pixels,
            int32_t height_pixels,
            const texture_params_t& params = {},
            bool generate_mipmap = true
        );

        // Create texture from file
        void create_from_file(
            const std::filesystem::path& path,
            const texture_params_t& params = {},
            bool generate_mipmap = true,
            bool flip_image = true
        );

        // Create a single-color texture
        void create_from_uniform_color(
            const color3_f32& color
        );

        void destroy() noexcept;

        std::string dump() const;

    }; // class

} // namespace