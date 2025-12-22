#include "image.hh"

#include <triengine/utility/debug_utils.hh>

namespace triengine
{
    uint32_t get_image_format_channel_size(image_format_type format) noexcept
    {
        switch (format) {
        case image_format_type::greyscale: return 1u;
        case image_format_type::rg: return 2u;
        case image_format_type::rgb: return 3u;
        case image_format_type::bgr: return 3u;
        case image_format_type::rgba: return 4u;
        case image_format_type::bgra: return 4u;
        default: return 0u;
        }
    }

    image& image::prepare(
        const int32_t width, 
        const int32_t height, 
        const image_format_type format)
    {
        TRIENGINE_ASSERT(width > 0);
        TRIENGINE_ASSERT(height > 0);

        const uint32_t channel_size = get_image_format_channel_size(format);
        TRIENGINE_ASSERT(channel_size > 0);

        _format = format;
        _width_pixels = width;
        _height_pixels = height;
        _stride_bytes = _width_pixels * channel_size;
        _image_buffer.resize(_height_pixels * _stride_bytes);

        return *this;
    }

    void image::clear() noexcept
    {
        _format = image_format_type::invalid;
        _width_pixels = 0;
        _height_pixels = 0;
        _stride_bytes = 0;
        _image_buffer.clear();
    }

} // namespace