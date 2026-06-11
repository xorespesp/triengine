#pragma once
#include <triengine/visualization/visualizer.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/scene.hh>
#include <triengine/texture_params.hh>

#include <array>
#include <cstdint>
#include <memory>

namespace demo
{
    enum class screen_mode {
        screen_3d = 0, // quad inside an ordinary 3D scene
        screen_2d,     // quad as a flat, fullscreen 2D screen
        count,         // keep last
    };

    class screen_quad_control_window;

    // Background worker that generates the animated CPU images (defined in the .cc).
    class async_pattern_producer;

    // Emulates "2D screen rendering" inside the 3D visualizer: a textured quad whose
    // diffuse texture is refreshed every frame. The CPU image is generated on a worker
    // thread; the render loop uploads only finished frames.
    class screen_quad_demo_app
    {
    public:
        // Defined in the .cc, where async_pattern_producer is complete (required to
        // destroy the unique_ptr member).
        screen_quad_demo_app();
        ~screen_quad_demo_app();

        void create();
        void destroy();
        void run();

    private:
        // Fit the 2D screen texture + quad to the given scene frame size.
        void _fit_screen2d_to_frame(triengine::vec2_i32 frame_size);

        // Restore the fixed square texture for the square 3D quad (2D mode may have left
        // the shared texture at a non-square size).
        void _fit_screen3d();

    private:
        std::unique_ptr<triengine::visualization::visualizer> _vis;
        std::shared_ptr<screen_quad_control_window> _screen_ctrl_window;

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

    }; // class

} // namespace
