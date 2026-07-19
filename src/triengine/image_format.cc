#include "image_format.hh"

namespace triengine
{
    uint32_t get_image_format_channel_size(image_format_type format) noexcept {
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

} // namespace