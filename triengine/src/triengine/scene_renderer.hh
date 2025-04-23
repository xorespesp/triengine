#pragma once
#include <triengine/common.h>

#include <triengine/gl_context.hh>
#include <triengine/frame_buffer.hh>
#include <triengine/scene.hh>
#include <triengine/shader.hh>
#include <triengine/shader_preprocessor.hh>

#include <triengine/renderer/infinite_grid_renderer.hh>
#include <triengine/renderer/light_source_renderer.hh>
#include <triengine/renderer/triangle_mesh_renderer.hh>
#include <triengine/renderer/lineset_renderer.hh>
#include <triengine/renderer/pcd_renderer.hh>
#include <triengine/renderer/skeleton_renderer.hh>

#include <triengine/shaders/wboit_composite_shaders.h>
#include <triengine/shaders/screen_quad_shaders.h>

#include <memory>

namespace triengine
{
    class scene_renderer
    {
    public:
        scene_renderer() = default;
        
        void create(gl_context* glctx);
        void destroy();

        void render(
            const frame_buffer& target_fb,
            scene& target_scn
        );

    private:
        std::unique_ptr<shader_preprocessor> _shader_prep;

        renderer::infinite_grid_renderer _infgrid_renderer;
        renderer::light_source_renderer _light_source_renderer;
        renderer::triangle_mesh_renderer _mesh_renderer;
        renderer::lineset_renderer _lineset_renderer;
        renderer::pcd_renderer _pcd_renderer;
        renderer::skeleton_renderer _skeleton_renderer;

        frame_buffer _wboit_fb;
        frame_buffer _overlay_fb;
        frame_buffer _smaa_fb;

        shader_program _wboit_composite_shader;
        shader_program _overlay_composite_shader;
        shader_program _screen_quad_shader;

        // SMAA pass shaders
        shader_program _smaa_edge_detect_shader;
        shader_program _smaa_blend_weight_shader;
        shader_program _smaa_neighbor_blend_shader;

        // needed for the 'SMAABlendingWeightCalculation' pass.
        texture_2d _smaa_area_tex, _smaa_search_tex;

        GLuint _vao_screen_quad{};
        GLuint _vbo_screen_quad{};
        
        std::list<std::shared_ptr<geometry::light_source_object>> _light_source_objects;

    }; // class

} // namespace