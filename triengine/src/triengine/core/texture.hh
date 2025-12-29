#pragma once
#include <triengine/common.h>
#include <triengine/image_buffer.hh>
#include <triengine/texture_params.hh>
#include <triengine/utility/noncopyable.hh>

#include <glad/gl.h>

namespace triengine::core
{
    class texture_2d;
    using texture_2d_ptr = std::shared_ptr<texture_2d>;

    static constexpr GLuint kInvalidGLTextureID{ 0u };

    // NOTE: `texture_2d` class MUST be created, used, and destroyed in the 
    // same thread where the OpenGL context is current
    class texture_2d
        : utility::noncopyable
    {
    public:
        texture_2d() = default;

        texture_2d(
            const image_buffer& tex_image,
            const texture_params_t& tex_params
        );

        ~texture_2d();
        texture_2d(texture_2d&& rhs) noexcept;
        texture_2d& operator=(texture_2d&& rhs) noexcept;

        GLuint id() const noexcept { return _tex_id; }
        image_format_type image_format() const noexcept { return _image_format; }
        int32_t width_pixels() const noexcept { return _width_pixels; }
        int32_t height_pixels() const noexcept { return _height_pixels; }
        const texture_params_t& params() const noexcept { return _tex_params; }

        bool is_valid() const noexcept;

        void destroy() noexcept;

    private:
        GLuint _tex_id{ kInvalidGLTextureID };
        image_format_type _image_format{ image_format_type::invalid };
        int32_t _width_pixels{}, _height_pixels{};
        texture_params_t _tex_params{};
    }; // class

} // namespace