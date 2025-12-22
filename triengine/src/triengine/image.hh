#pragma once
#include <glad/gl.h>
#include <vector>

namespace triengine
{
    // Ref:
    // https://docs.gl/gl4/glTexImage2D (Table 1, Table 2)
    // https://stackoverflow.com/a/4745945
    // https://stackoverflow.com/a/34497547
    enum class image_format_type : GLenum {
        invalid = 0,
        greyscale = GL_RED, // 1 channel (Ref: https://stackoverflow.com/a/69113182)
        rg = GL_RG, // 2 channel
        rgb = GL_RGB, // 3 channel
        bgr = GL_BGR, // 3 channel (NOTE: not a internal format, just a format)
        rgba = GL_RGBA, // 4 channel
        bgra = GL_BGRA, // 4 channel (NOTE: not a internal format, just a format)
    };

    /// \brief Get the channel size (in bytes) for the given image format. 0 if invalid.
    uint32_t get_image_format_channel_size(
        image_format_type format
    ) noexcept;

    class image {
    public:
        image() = default;
        ~image() = default;

        image_format_type format() const noexcept { return _format; }
        int32_t width_pixels() const noexcept { return _width_pixels; }
        int32_t height_pixels() const noexcept { return _height_pixels; }
        uint32_t stride_bytes() const noexcept { return _stride_bytes; }

        bool empty() const noexcept { return _image_buffer.empty(); }

        const uint8_t* buffer() const noexcept { return _image_buffer.data(); }
        uint8_t* buffer() noexcept { return _image_buffer.data(); }
        size_t buffer_size() const noexcept { return _image_buffer.size(); }

        /// \brief Prepare Image properties and allocate Image buffer.
        image& prepare(int32_t width, int32_t height, image_format_type format);

        /// \brief Clear Image data and reset properties.
        void clear() noexcept;

    private:
        image_format_type _format{ image_format_type::invalid };
        int32_t _width_pixels{}, _height_pixels{};
        uint32_t _stride_bytes{};
        std::vector<uint8_t> _image_buffer;
    }; // class

} // namespace