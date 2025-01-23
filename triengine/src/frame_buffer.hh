#pragma once
#include "common.h"

#include "extern/glad/glad.h"
#include <GLFW/glfw3.h>

namespace triengine
{
    class frame_buffer
    {
    private:
        static constexpr GLuint kInvalidObjectID{ static_cast<GLuint>(-1) };

    private:
        GLuint _fbo{ kInvalidObjectID };
        GLuint _color_texture{ kInvalidObjectID };
        GLuint _depth_stencil_rbo{ kInvalidObjectID };
        int32_t _width_pixels{}, _height_pixels{};
        int32_t _sample_count{ 1 }; // 1 means Non-MSAA, 1 >= means MSAA
                                    // NOTE: All attachments within an FBO must have the same multisample count.
    public:
        frame_buffer() = default;
        ~frame_buffer();

        frame_buffer(frame_buffer&& rhs) noexcept;
        frame_buffer& operator=(frame_buffer&& rhs) noexcept;
        frame_buffer(const frame_buffer&) = delete;
        frame_buffer& operator=(const frame_buffer&) = delete;

        GLuint id() const noexcept { return _fbo; }
        GLuint color_texture_id() const noexcept { return _color_texture; }

        int32_t width_pixels() const noexcept { return _width_pixels; }
        int32_t height_pixels() const noexcept { return _height_pixels; }
        int32_t sample_count() const noexcept { return _sample_count; }

        bool is_multisampled() const noexcept { return _sample_count > 1; }
        bool is_valid() const noexcept;

        void reserve(
            int32_t width_pixels, 
            int32_t height_pixels, 
            int32_t sample_count = 1
        );

        void destroy() noexcept;

        void bind();
        void unbind();

        // It is mainly used for multisample -> single sample blit, but
        // single sample -> single sample / multisample -> multisample are also possible
        void blit_to(
            frame_buffer& target_fb, 
            bool blit_color = true, 
            bool blit_depth = true,
            bool blit_stencil = true
        );

    private:
        static void _allocate_framebuffer(
            GLuint fbo,
            GLuint color_texture,
            GLuint depth_stencil_rbo,
            int32_t width_pixels,
            int32_t height_pixels,
            int32_t sample_count
        );
    };

} // namespace