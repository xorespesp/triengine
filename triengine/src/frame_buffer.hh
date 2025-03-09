#pragma once
#include "common.h"
#include "texture.hh"

#include <optional>

namespace triengine
{
    struct frame_buffer_texture_params_t
        : public texture_params_t
    {
        // TODO: add more options..
    };

    /**
     * @brief A simple OpenGL 4.2+ framebuffer wrapper
     *        supporting single-color/multiple-color attachments(MRT),
     *        optional depth/stencil attachments, single/multi-sampling,
     *        dynamic framebuffer size reallocation, and blitting between framebuffers.
     *        Main usage: post-processing, weighted-blended OIT, physics based BLOOM, shadow map, PBR, deferred shading, etc...
     */
    class frame_buffer
    {
    public:
        static constexpr GLuint kInvalidBufferID{ 0u };

        enum class attachment_type {
            invalid = 0,
            color,
            depth,
            stencil,
            // TODO: add `depth_stencil` (optional)
        };

        struct attachment_info_t {  
            attachment_type type{ attachment_type::invalid }; // attachment type  
            GLuint buffer_id{ kInvalidBufferID }; // texture / renderbuffer id  
            GLenum internal_format{ 0 }; // texture / renderbuffer internal format  
            bool is_render_buffer{ false }; // is texture or renderbuffer  
            frame_buffer_texture_params_t tex_params{}; // used only if `is_render_buffer == false`  

            // make move-only
            attachment_info_t() = default;  
            attachment_info_t(attachment_info_t&&) = default;  
            attachment_info_t& operator=(attachment_info_t&&) = default;  
            attachment_info_t(const attachment_info_t&) = delete;  
            attachment_info_t& operator=(const attachment_info_t&) = delete;  
        };

    private:
        GLuint _fbo_id{ kInvalidBufferID };
        std::vector<attachment_info_t> _color_attachments;
        std::optional<attachment_info_t> _depth_attachment;
        std::optional<attachment_info_t> _stencil_attachment;

        // FBO resolution and multisample count
        // NOTE: All attachments within an FBO must have the same multisample count.
        int32_t _width_pixels{}, _height_pixels{};
        int32_t _sample_count{ 1 }; // 1 means Non-MSAA, 1 >= means MSAA
    
    public:
        frame_buffer() = default;
        ~frame_buffer();

        frame_buffer(frame_buffer&& rhs) noexcept;
        frame_buffer& operator=(frame_buffer&& rhs) noexcept;
        frame_buffer(const frame_buffer&) = delete;
        frame_buffer& operator=(const frame_buffer&) = delete;

        GLuint fbo_id() const noexcept;

        // ---- Attachment accessors ----
        const attachment_info_t* color_attachment(size_t attachment_index) const noexcept;
        const attachment_info_t* color_attachment() const noexcept;
        const attachment_info_t* depth_attachment() const noexcept;
        const attachment_info_t* stencil_attachment() const noexcept;

        // ---- Framebuffer properties ----
        int32_t width_pixels() const noexcept;
        int32_t height_pixels() const noexcept;
        int32_t sample_count() const noexcept;
        size_t color_attachments_size() const noexcept;

        bool has_color_attachment() const noexcept;
        bool has_depth_attachment() const noexcept;
        bool has_stencil_attachment() const noexcept;

        bool is_valid() const noexcept;
        bool is_multisampled() const noexcept;
        bool is_MRT() const noexcept;
        bool is_depth_only() const noexcept;
        bool is_stencil_only() const noexcept;

        /**
         * @brief Reallocates all attachments with the specified size and sample count,
         *        regenerating textures/renderbuffers as needed.
         */
        void reallocate(
            int32_t width_pixels,
            int32_t height_pixels,
            int32_t sample_count = 1
        );

        /**
         * @brief Safely destroys the FBO and releases GPU resources of its attachments.
         */
        void destroy() noexcept;

        /**
         * @brief Binds this FBO to GL_FRAMEBUFFER target.
         *        If there are multiple color attachments, sets up glDrawBuffers accordingly.
         *        If there are no color attachments, sets glDrawBuffer(GL_NONE).
         * @throws std::runtime_error if the FBO is invalid.
         */
        void bind();
        void unbind();

        /**
         * @brief Blits the content of this FBO into another FBO.
         *        It is mainly used for multisample -> single sample blit, but
         *        single sample -> single sample / multisample -> multisample are also possible
         * @param target_fb The target FBO to blit into.
         * @param blit_color Whether to blit color buffer.
         * @param blit_depth Whether to blit depth buffer.
         * @param blit_stencil Whether to blit stencil buffer.
         * @param blit_filter Usually GL_NEAREST or GL_LINEAR.
         * 
         */
        void blit_to(
            frame_buffer& target_fb,
            bool blit_color = true,
            bool blit_depth = true,
            bool blit_stencil = true,
            GLenum blit_filter = GL_NEAREST // `GL_NEAREST` or `GL_LINEAR`
        );

    public:

        // ---- Static builder methods ----

        /**
         * @brief Creates a new FBO with one or more color-only attachments.
         * @param internal_color_formats List of internal color formats (e.g., `GL_RGBA8`, `GL_R8`).
         * @param width_pixels FBO width.
         * @param height_pixels FBO height.
         * @param sample_count Multisample count (>1 for MSAA).
         * @param color_tex_params  Texture parameters (wrap/filter) for color attachments.
         * @param use_renderbuffer  If true, attachment(color attachment) is a renderbuffer, else a texture.
         */
        static frame_buffer create_color_only_buffer(
            std::initializer_list<GLenum> internal_color_formats, // e.g., GL_RGBA16F, GL_R8
            int32_t width_pixels,
            int32_t height_pixels,
            int32_t sample_count = 1,
            const frame_buffer_texture_params_t& color_tex_params = {},
            bool use_renderbuffer = false
        );

        /**
         * @brief Creates a new FBO with a depth-only attachment.
         * @param internal_depth_format e.g., `GL_DEPTH_COMPONENT24`, `GL_DEPTH_COMPONENT32F`
         * @param use_renderbuffer  If true, attachment(depth attachment) is a renderbuffer, else a texture.
         */
        static frame_buffer create_depth_only_buffer(
            GLenum internal_depth_format, // e.g., GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32F
            int32_t width_pixels,
            int32_t height_pixels,
            int32_t sample_count = 1,
            bool use_renderbuffer = false
        );

        /**
         * @brief Creates a new FBO with a stencil-only attachment.
         * @param internal_stencil_format e.g., `GL_STENCIL_INDEX8`
         * @param use_renderbuffer  If true, attachment(stencil attachment) is a renderbuffer, else a texture.
         */
        static frame_buffer create_stencil_only_buffer(
            GLenum internal_stencil_format, // e.g., GL_STENCIL_INDEX8
            int32_t width_pixels,
            int32_t height_pixels,
            int32_t sample_count = 1,
            bool use_renderbuffer = false
        );

        /**
         * @brief Creates a new FBO with one or more color attachments + a depth attachment.
         * @param internal_color_formats  List of internal color formats (e.g., { GL_RGBA8, GL_RGBA16F, ... }).
         * @param internal_depth_format   Internal format for the depth buffer (e.g., GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32F).
         * @param width          Framebuffer width in pixels.
         * @param height         Framebuffer height in pixels.
         * @param samples        MSAA sample count (>1 for MSAA).
         * @param color_tex_params  Texture parameters (wrap/filter) for color attachments.
         * @param use_renderbuffer_for_color  If true, color attachment is a renderbuffer, else a texture.
         * @param use_renderbuffer_for_depth  If true, depth attachment is a renderbuffer, else a texture.
         */
        static frame_buffer create_color_depth_only_buffer(
            std::initializer_list<GLenum> internal_color_formats,
            GLenum internal_depth_format,
            int32_t width,
            int32_t height,
            int32_t samples = 1,
            const frame_buffer_texture_params_t& color_tex_params = {},
            bool use_renderbuffer_for_color = false,
            bool use_renderbuffer_for_depth = true
        );

        /**
         * @brief Creates a framebuffer with single color / multiple color(MRT) + depth + stencil attachments.
         * @param internal_color_formats  List of internal color format(s) (e.g. { `GL_RGBA8`, `GL_RGBA16F`, ... }).
         * @param internal_depth_format   Internal format for depth (e.g., `GL_DEPTH_COMPONENT24`).
         * @param internal_stencil_format Internal format for stencil (e.g., `GL_STENCIL_INDEX8`).
         * @param width_pixels   FBO width in pixels.
         * @param height_pixels  FBO height in pixels.
         * @param sample_count   Multisample count (>1 for MSAA).
         * @param color_tex_params  Texture parameters (wrap/filter) for color attachments.
         * @param use_renderbuffer_for_color  If true, color attachment is a renderbuffer, else a texture.
         * @param use_renderbuffer_for_depth  If true, depth attachment is a renderbuffer, else a texture.
         * @param use_renderbuffer_for_stencil  If true, stencil attachment is a renderbuffer, else a texture.
         */
        static frame_buffer create_color_depth_stencil_buffer(
            std::initializer_list<GLenum> internal_color_formats,
            GLenum internal_depth_format,
            GLenum internal_stencil_format,
            int32_t width_pixels,
            int32_t height_pixels,
            int32_t sample_count = 1,
            const frame_buffer_texture_params_t& color_tex_params = {},
            bool use_renderbuffer_for_color = false,
            bool use_renderbuffer_for_depth = true,
            bool use_renderbuffer_for_stencil = true
        );

    private:

        // Allocates (or reallocates) the underlying GPU buffer (texture or renderbuffer)
        // for the given `attachment_info_t`.
        static void _allocate_attachment_buffer(
            attachment_info_t& cinfo/* in-out */,
            int32_t width_pixels,
            int32_t height_pixels,
            int32_t sample_count
        );

        // Deletes the GPU resource (texture or renderbuffer) associated with attach_info.
        static void _deallocate_attachment_buffer(
            attachment_info_t& attach_info/* in-out */
        );

        // Attaches the given attachment buffer to the currently bound FBO
        // at the specified attachment point (e.g., `GL_COLOR_ATTACHMENT0`, `GL_DEPTH_ATTACHMENT`).
        static void _attach_to_framebuffer(
            const attachment_info_t& attach_info,
            GLenum attachment_point,
            bool multisampled
        );

    }; // class

} // namespace