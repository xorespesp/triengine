#pragma once
#include "common.h"

#include "camera.hh"
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
        
        void create();

        void destroy();

    private:
        int32_t _scene_width{};
        int32_t _scene_height{};
        renderer::infinite_grid_renderer _infgrid_renderer;
        renderer::light_source_renderer _light_source_renderer;
        renderer::triangle_mesh_renderer _mesh_renderer;
        renderer::lineset_renderer _lineset_renderer;
        renderer::pcd_renderer _pcd_renderer;
        renderer::skeleton_renderer _skeleton_renderer;
        frame_buffer _fb_main;
        frame_buffer _fb_msaa_copy; // only used in msaa rendering

    }; // class

} // namespace