#pragma once
#include <triengine/visualization/visualizer.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/scene.hh>
#include <triengine/texture_params.hh>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>

namespace demo
{
    enum class screen_mode {
        screen_3d = 0,  // quad inside an ordinary 3D scene
        screen_2d,      // quad as a flat, fullscreen 2D screen
        screen_overlay, // image as the scene background with a 3D mesh overlaid on top
        count,          // keep last
    };

    class screen_control_window;

    // Background worker that generates the animated CPU images (defined in the .cc).
    class async_pattern_producer;

    // Demonstrates three ways of presenting a streamed 2D image inside the visualizer:
    //   - screen_3d:      a textured quad sitting in a normal 3D scene
    //   - screen_2d:      a flat, fullscreen 2D screen (orthographic, unlit)
    //   - screen_overlay: the image as the scene background (core scene_render_config
    //                     bg_image feature) with a rotating 3D mesh drawn over it
    // The CPU image is generated on a worker thread; the render loop uploads only finished
    // frames. The same texture handle backs all three modes.
    class screen_demo_app
    {
    public:
        // Defined in the .cc, where async_pattern_producer is complete (required to
        // destroy the unique_ptr member).
        screen_demo_app();
        ~screen_demo_app();

        void create();
        void destroy();
        void run();

    private:
        // Fit the 2D screen texture + quad to the given scene frame size.
        void _fit_screen2d_to_frame(triengine::vec2_i32 frame_size);

        // Restore the fixed square texture for the square 3D quad (2D mode may have left
        // the shared texture at a non-square size).
        void _fit_screen3d();

        // Recreate the shared texture at the fixed background resolution and point the
        // overlay scene's background image at it. Called once when entering this mode; a
        // window resize while in this mode does NOT recreate the texture (the renderer
        // handles the aspect mismatch via its UV-fit, see scene_render_config::bg_image_fit).
        void _fit_screen_overlay();

    private:
        std::unique_ptr<triengine::visualization::visualizer> _vis;
        std::shared_ptr<screen_control_window> _screen_ctrl_window;

        // One scene per screen mode, indexed by screen_mode.
        std::array<
            std::shared_ptr<triengine::scene>,
            static_cast<size_t>(screen_mode::count)
        > _scenes;

        screen_mode _curr_screen_mode{ screen_mode::screen_3d };

        // Generates the animated CPU images off-thread; the render loop pulls the latest
        // finished frame and uploads it.
        std::unique_ptr<async_pattern_producer> _producer;

        triengine::texture_handle_t _screen_tex_handle{ triengine::kInvalidTextureHandle };
        // Current texture size. A produced frame is uploaded only if it matches, so frames
        // generated at a stale size (mid-resize) are dropped.
        triengine::vec2_i32 _tex_size{ 0, 0 };

        // Both quads share the texture; kept so they can be repointed when it is recreated.
        std::shared_ptr<triengine::geometry::mesh_object> _screen3d_quad;
        std::shared_ptr<triengine::geometry::mesh_object> _screen2d_quad;

        // Mesh overlaid on the background image; rotated over time in screen_overlay mode.
        std::shared_ptr<triengine::geometry::mesh_object> _overlay_mesh;

        // Start time used to drive the time-based overlay rotation.
        std::chrono::steady_clock::time_point _start_time{ std::chrono::steady_clock::now() };

    }; // class

} // namespace
