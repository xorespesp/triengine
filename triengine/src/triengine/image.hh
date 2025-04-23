#pragma once
#include <glad/glad.h>
#include <triengine/misc/debug_utils.hh>

#include <vector>

namespace triengine
{
    // Ref:
    // https://docs.gl/gl4/glTexImage2D (Table 1, Table 2)
    // https://stackoverflow.com/a/4745945
    // https://stackoverflow.com/a/34497547
    enum class image_format_type : GLenum
    {
        invalid = 0,
        greyscale = GL_RED, // 1 channel (Ref: https://stackoverflow.com/a/69113182)
        rg = GL_RG, // 2 channel
        rgb = GL_RGB, // 3 channel
        bgr = GL_BGR, // 3 channel (NOTE: not a internal format, just a format)
        rgba = GL_RGBA, // 4 channel
        bgra = GL_BGRA, // 4 channel (NOTE: not a internal format, just a format)
    };

    class image
    {
    private:
        image_format_type _format{ image_format_type::invalid };
        int32_t _width_pixels{}, _height_pixels{};
        uint32_t _stride_bytes{};
        std::vector<uint8_t> _image_buffer;

    public:
        image() = default;
        ~image() = default;

        image_format_type format() const noexcept {
            return _format;
        }

        int32_t width_pixels() const noexcept {
            return _width_pixels;
        }

        int32_t height_pixels() const noexcept {
            return _height_pixels;
        }

        uint32_t stride_bytes() const noexcept {
            return _stride_bytes;
        }

        bool empty() const noexcept {
            return _image_buffer.empty();
        }

        const uint8_t* data() const noexcept {
            return _image_buffer.data();
        }

        uint8_t* data() noexcept {
            return _image_buffer.data();
        }

        void clear() noexcept {
            _width_pixels = _height_pixels = 0;
            _stride_bytes = 0;
            _image_buffer.clear();
        }

        /// \brief Prepare Image properties and allocate Image buffer.
        image& prepare(
            const int32_t width,
            const int32_t height,
            const image_format_type format)
        {
            const uint32_t pixel_size_in_bytes = 
                [format]() -> uint32_t {
                    switch (format) {
                    case image_format_type::greyscale:
                        return 1u;
                    case image_format_type::rg:
                        return 2u;
                    case image_format_type::rgb:
                    case image_format_type::bgr:
                        return 3u;
                    case image_format_type::rgba:
                    case image_format_type::bgra:
                        return 4u;
                    default:
                        return 0u;
                    }
                }();

            TRIENGINE_ASSERT(width > 0);
            TRIENGINE_ASSERT(height > 0);
            TRIENGINE_ASSERT(pixel_size_in_bytes > 0);

            _format = format;
            _width_pixels = width;
            _height_pixels = height;
            _stride_bytes = _width_pixels * pixel_size_in_bytes;
            _image_buffer.resize(_height_pixels * _stride_bytes);

            return *this;
        }

    }; // class

} // namespace