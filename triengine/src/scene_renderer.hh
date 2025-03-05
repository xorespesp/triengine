#pragma once
#include "common.h"

#include "gl_context.hh"
#include "scene.hh"
#include "frame_buffer.hh"

#include "renderer/infinite_grid_renderer.hh"
#include "renderer/light_source_renderer.hh"
#include "renderer/triangle_mesh_renderer.hh"
#include "renderer/lineset_renderer.hh"
#include "renderer/pcd_renderer.hh"
#include "renderer/skeleton_renderer.hh"

#include <memory>

namespace triengine
{
    class scene_renderer
    {
    public:
        scene_renderer() = default;
        
        void create(gl_context* glctx);
        void destroy();

        void render_scene(scene& target_scn);

    private:
        // Sub-renderers
        renderer::infinite_grid_renderer _infgrid_renderer;
        renderer::light_source_renderer _light_source_renderer;
        renderer::triangle_mesh_renderer _mesh_renderer;
        renderer::lineset_renderer _lineset_renderer;
        renderer::pcd_renderer _pcd_renderer;
        renderer::skeleton_renderer _skeleton_renderer;

        std::shared_ptr<geometry::light_source_object> _point_light_source_object;

    }; // class

} // namespace