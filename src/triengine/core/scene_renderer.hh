#pragma once
#include <triengine/common.h>

#include <triengine/scene.hh>

#include <triengine/core/gl_context.hh>
#include <triengine/core/texture.hh>
#include <triengine/core/frame_buffer.hh>
#include <triengine/core/bloom_effect.hh>
#include <triengine/core/gpu_frame_profiler.hh>

#include <triengine/renderer/infinite_plane_renderer.hh>
#include <triengine/renderer/light_source_renderer.hh>
#include <triengine/renderer/mesh_renderer.hh>
#include <triengine/renderer/lineset_renderer.hh>
#include <triengine/renderer/pcd_renderer.hh>
#include <triengine/renderer/skeleton_renderer.hh>
#include <triengine/renderer/text_renderer.hh>

#include <triengine/utility/cpu_frame_profiler.hh>
#include <triengine/utility/gl_utils.hh>

#include <cstdint>
#include <memory>

namespace triengine::core
{
    class scene_renderer
    {
    public:
        // Render passes, timed on the CPU (command submission) and on the GPU (execution).
        // Registered in this order on both pass profilers, so a stage has the same ID on each.
        struct profiling_stages {
            using descriptor = utility::cpu_frame_profiler::stage_descriptor;
            static constexpr descriptor kResourceUploads{ "Resource Uploads",
                "Pending GPU resource creations and updates." };
            static constexpr descriptor kDeferredGeometry{ "Deferred Geometry",
                "Opaque lit geometry into the G-buffer." };
            static constexpr descriptor kDeferredLighting{ "Deferred Lighting",
                "Lighting from the G-buffer." };
            static constexpr descriptor kForward{ "Forward",
                "Background, unlit and transparent geometry, and the WBOIT composite." };
            static constexpr descriptor kOverlay{ "Overlay",
                "Skeleton overlay, when enabled." };
            static constexpr descriptor kBloom{ "Bloom",
                "Bloom, when enabled." };
            static constexpr descriptor kSmaa{ "SMAA",
                "Anti-aliasing, when enabled." };
            static constexpr descriptor kText{ "Text",
                "2D and 3D labels, when enabled." };
            static constexpr descriptor kFinal{ "Final",
                "Tone mapping into the target framebuffer." };
        };

    public:
        scene_renderer() = default;
        
        void create(gl_context* glctx);
        void destroy();

        void render(
            GLuint target_fbo_id,
            int32_t frame_width_pixels,
            int32_t frame_height_pixels,
            scene& target_scn,
            uint64_t frame_id // host's frame number, recorded by the pass profilers.
        );

        // Per-pass timings of `render()`, one frame per call. Disabled until the host enables them.
        const utility::cpu_frame_profiler& get_pass_cpu_profiler() const noexcept { return _pass_cpu_profiler; }
        utility::cpu_frame_profiler& get_pass_cpu_profiler() noexcept { return _pass_cpu_profiler; }

        const gpu_frame_profiler& get_pass_gpu_profiler() const noexcept { return _pass_gpu_profiler; }
        gpu_frame_profiler& get_pass_gpu_profiler() noexcept { return _pass_gpu_profiler; }

#if defined(TRIENGINE_DEBUG_MODE)
        // `GLCall()` calls made by the last `render()`, per call site.
        const utility::gl_call_stats& get_last_frame_gl_calls() const noexcept { return _frame_gl_calls; }
#endif

    private:
        using stage_id_t = utility::cpu_frame_profiler::stage_id_t;

        // Registers `descriptor` on both pass profilers.
        stage_id_t _register_pass_stage(const utility::cpu_frame_profiler::stage_descriptor& descriptor);

    private:
        gl_context* _glctx{ nullptr };

        // Pass profilers, then their stages in display order
        utility::cpu_frame_profiler _pass_cpu_profiler;
        gpu_frame_profiler _pass_gpu_profiler;
        const stage_id_t _resource_uploads_stage{ this->_register_pass_stage(profiling_stages::kResourceUploads) };
        const stage_id_t _deferred_geometry_stage{ this->_register_pass_stage(profiling_stages::kDeferredGeometry) };
        const stage_id_t _deferred_lighting_stage{ this->_register_pass_stage(profiling_stages::kDeferredLighting) };
        const stage_id_t _forward_stage{ this->_register_pass_stage(profiling_stages::kForward) };
        const stage_id_t _overlay_stage{ this->_register_pass_stage(profiling_stages::kOverlay) };
        const stage_id_t _bloom_stage{ this->_register_pass_stage(profiling_stages::kBloom) };
        const stage_id_t _smaa_stage{ this->_register_pass_stage(profiling_stages::kSmaa) };
        const stage_id_t _text_stage{ this->_register_pass_stage(profiling_stages::kText) };
        const stage_id_t _final_stage{ this->_register_pass_stage(profiling_stages::kFinal) };

#if defined(TRIENGINE_DEBUG_MODE)
        utility::gl_call_stats _frame_gl_calls;
#endif

        renderer::infinite_plane_renderer _inf_plane_renderer;
        renderer::light_source_renderer _light_source_renderer;
        renderer::mesh_renderer _mesh_renderer;
        renderer::lineset_renderer _lineset_renderer;
        renderer::pcd_renderer _pcd_renderer;
        renderer::skeleton_renderer _skeleton_renderer;
        renderer::text_renderer _text_renderer;

        phys_bloom_effect _bloom_effect;

        frame_buffer _wboit_fb;
        frame_buffer _gbuffer_fb;
        frame_buffer _overlay_fb;
        frame_buffer _smaa_fb;

        shader_program _deferred_lighting_shader;
        shader_program _wboit_composite_shader;
        shader_program _overlay_composite_shader;
        shader_program _screen_quad_shader;
        shader_program _hdr_screen_quad_shader;
        shader_program _clear_color_screen_quad_shader;
        shader_program _screen_quad_bg_img_shader;

        // SMAA pass shaders
        shader_program _smaa_edge_detect_shader;
        shader_program _smaa_blend_weight_shader;
        shader_program _smaa_neighbor_blend_shader;

        // needed for the 'SMAABlendingWeightCalculation' pass.
        texture_2d _smaa_area_tex, _smaa_search_tex;

        GLuint _vao_screen_quad{};
        GLuint _vbo_screen_quad{};

    }; // class

} // namespace