#pragma once
#include "common.h"

#include "gl_context.hh"
#include "frame_buffer.hh"
#include "scene.hh"
#include "shader.hh"
#include "shader_preprocessor.hh"

#include "renderer/infinite_grid_renderer.hh"
#include "renderer/light_source_renderer.hh"
#include "renderer/triangle_mesh_renderer.hh"
#include "renderer/lineset_renderer.hh"
#include "renderer/pcd_renderer.hh"
#include "renderer/skeleton_renderer.hh"

#include "shaders/wboit_composite_shaders.h"
#include "shaders/screen_quad_shaders.h"

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

        shader_program _wboit_composite_shader;
        shader_program _overlay_composite_shader;
        shader_program _screen_quad_shader;
        
        GLuint _vao_screen_quad{};
        GLuint _vbo_screen_quad{};
        
        std::list<std::shared_ptr<geometry::light_source_object>> _light_source_objects;

    }; // class

} // namespace