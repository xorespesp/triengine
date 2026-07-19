#pragma once
#include <triengine/common.h>
#include <glad/gl.h>

namespace triengine
{
    // Structure that holds texture parameters 
    struct texture_params_t
    {
        // Wrapping mode for S and T axes
        GLint wrap_s{ GL_CLAMP_TO_EDGE };
        GLint wrap_t{ GL_CLAMP_TO_EDGE };

        // Filtering mode for minification and magnification
        GLint min_filter{ GL_LINEAR };
        GLint mag_filter{ GL_LINEAR };

        bool gamma_correction{ false }; // Whether to apply gamma correction
        bool generate_mipmap{ true }; // Whether to generate mipmaps

        texture_params_t() = default;
        texture_params_t(
            GLint wrap_s_,
            GLint wrap_t_,
            GLint min_filter_,
            GLint mag_filter_,
            bool gamma_correction_,
            bool generate_mipmap_)
            : wrap_s{ wrap_s_ }
            , wrap_t{ wrap_t_ }
            , min_filter{ min_filter_ }
            , mag_filter{ mag_filter_ }
            , gamma_correction{ gamma_correction_ }
            , generate_mipmap{ generate_mipmap_ }
        {}
    };

    using texture_handle_t = uint64_t;
    static constexpr texture_handle_t kInvalidTextureHandle{ static_cast<texture_handle_t>(-1) };

} // namespace