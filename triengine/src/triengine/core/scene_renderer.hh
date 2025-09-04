#pragma once
#include <triengine/common.h>

#include <triengine/scene.hh>

#include <triengine/core/gl_context.hh>
#include <triengine/core/frame_buffer.hh>
#include <triengine/core/bloom_effect.hh>

#include <triengine/renderer/infinite_plane_renderer.hh>
#include <triengine/renderer/light_source_renderer.hh>
#include <triengine/renderer/mesh_renderer.hh>
#include <triengine/renderer/lineset_renderer.hh>
#include <triengine/renderer/pcd_renderer.hh>
#include <triengine/renderer/skeleton_renderer.hh>

#include <memory>

namespace triengine::core
{
    class scene_renderer
    {
    public:
        scene_renderer() = default;
        
        void create(gl_context* glctx);
        void destroy();

        void render(
            GLuint target_fbo_id,
            int32_t frame_width_pixels,
            int32_t frame_height_pixels,
            scene& target_scn
        );

    private:
        gl_context* _glctx{ nullptr };

        renderer::infinite_plane_renderer _inf_plane_renderer;
        renderer::light_source_renderer _light_source_renderer;
        renderer::mesh_renderer _mesh_renderer;
        renderer::lineset_renderer _lineset_renderer;
        renderer::pcd_renderer _pcd_renderer;
        renderer::skeleton_renderer _skeleton_renderer;

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