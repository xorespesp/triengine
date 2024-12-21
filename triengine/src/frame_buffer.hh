#pragma once
#include "common.h"
#include "texture.hh"
#include "extern/glad/glad.h"
#include <GLFW/glfw3.h>

namespace triengine
{
    class frame_buffer
    {
    private:
        static constexpr GLuint kInvalidBufferObjectID{ static_cast<GLuint>(-1) };

    private:
        GLuint _fbo{ kInvalidBufferObjectID };
        texture_2d _tex_color;
        GLuint _rbo_depth_stencil{ kInvalidBufferObjectID };
        int32_t _width_pixels{}, _height_pixels{};

    public:
        frame_buffer() = default;
        ~frame_buffer();

        frame_buffer(frame_buffer&& rhs) noexcept;
        frame_buffer& operator=(frame_buffer&& rhs) noexcept;
        frame_buffer(const frame_buffer&) = delete;
        frame_buffer& operator=(const frame_buffer&) = delete;

        GLuint id() const noexcept { return _fbo; }
        const texture_2d* color_texture() const noexcept { return &_tex_color; }
        int32_t width_pixels() const noexcept { return _width_pixels; }
        int32_t height_pixels() const noexcept { return _height_pixels; }

        bool is_valid() const noexcept;
        void reserve(int32_t width_pixels, int32_t height_pixels);
        void destroy() noexcept;

        void bind();
        void unbind();

    };

} // namespace