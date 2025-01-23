#pragma once
#include "common.h"
#include "camera.hh"
#include "scene.hh"
#include "frame_buffer.hh"
#include "image.hh"

#include "renderer/infinite_grid_renderer.hh"
#include "renderer/light_source_renderer.hh"
#include "renderer/triangle_mesh_renderer.hh"
#include "renderer/lineset_renderer.hh"
#include "renderer/pcd_renderer.hh"
#include "renderer/skeleton_renderer.hh"
#include "gui/gui_manager.hh"

#include "extern/glad/glad.h"
#include <GLFW/glfw3.h>

#include <functional>
#include <array>

namespace triengine
{
    class offscreen_window
    {
    public:
        offscreen_window();
        virtual ~offscreen_window() = default;

        offscreen_window(const offscreen_window&) = delete;
        offscreen_window& operator=(const offscreen_window&) = delete;

        GLFWwindow* get_glfw_window() const { return _glfw_window.get(); }

        const scene* get_default_scene() const { return _default_scene.get(); }
        scene* get_default_scene() { return _default_scene.get(); }

        const camera* get_current_camera() const { return _curr_focused_camera; }
        camera* get_current_camera() { return _curr_focused_camera; }

        void set_fovy(float fovy_deg);

        void enable_mirror_mode(bool enable);

        void create_window(
            int32_t width,
            int32_t height,
            bool multisample = true
        );

        void destroy_window();

        bool update_window();

        void render(
            image& frame_image/* out */
        );

    private:
        void _begin_frame();
        void _end_frame();
        void _render_viewport(
            camera& target_camera,
            view_port viewport
        );
        
    private:
        bool _flag_initialized{ false };
        bool _flag_invalidate_fbo{ true };

        std::shared_ptr<GLFWwindow> _glfw_window;

        camera _top_left_camera;
        camera _top_right_camera;
        camera _bottom_right_camera;
        const std::array<camera*, 3> _camera_list = {
            &_top_left_camera, &_top_right_camera, &_bottom_right_camera
        };

        int32_t _curr_window_width{};
        int32_t _curr_window_height{};
        camera* _curr_focused_camera{ &_top_left_camera };

        renderer::infinite_grid_renderer _infgrid_renderer;
        renderer::light_source_renderer _light_source_renderer;
        renderer::triangle_mesh_renderer _mesh_renderer;
        renderer::lineset_renderer _lineset_renderer;
        renderer::pcd_renderer _pcd_renderer;
        renderer::skeleton_renderer _skeleton_renderer;

        std::shared_ptr<scene> _default_scene;

        int32_t _fb_sample_count{ 1 };
        frame_buffer _fb_main;
        frame_buffer _fb_msaa_copy; // only used in msaa rendering

    }; // class

} // namespace