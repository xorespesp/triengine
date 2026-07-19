#pragma once
#include <triengine/common.h>
#include <glad/gl.h>

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

} // namespace