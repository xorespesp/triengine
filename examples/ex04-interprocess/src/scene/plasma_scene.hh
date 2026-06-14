#pragma once
// offscreen_renderer_dx.hh pulls in <windows.h>; include it before any header that pulls in
// GLFW (via the scene/GL chain) so <windows.h> defines APIENTRY first and GLFW does not
// redefine it (avoids C4005).
#include <triengine/visualization/offscreen_renderer_dx.hh>

#include "../scene_wrapper.hh"

#include <triengine/geometry/mesh_object.hh>
#include <triengine/texture_params.hh>
#include <triengine/image_buffer.hh>

#include <memory>

namespace scene
{
    // An animated plasma pattern shown on a flat, unlit, fullscreen 2D screen quad. The CPU
    // image is generated on a background worker thread; update() uploads only finished frames
    // to the quad's diffuse texture and post_render() recycles the consumed buffer.
    // None of the worker code touches GL/scene state.
    //
    // CPU cost is kept low two ways: (1) the image is generated at a small fixed budget
    // (longest side <= a cap) and bilinear-upscaled by the GPU onto the quad, so cost is
    // independent of the window/frame size; (2) the generator uses a separable-plasma scheme
    // (per-frame 1D sine arrays + a precomputed palette) so each pixel costs a few table reads
    // instead of trigonometric calls.
    class plasma_scene
        : public scene_wrapper
    {
    public:
        explicit plasma_scene(triengine::visualization::offscreen_renderer_dx& renderer);
        ~plasma_scene() override;

        // Refit to the current frame size if it changed, then upload the latest produced
        // frame to the quad texture.
        void update(triengine::vec2_i32 frame_size) override;

        // Return the frame uploaded by the last update() to the producer's pool.
        void post_render() override;

    private:
        // The small generation resolution for a given frame size: the frame aspect scaled so
        // its longest side equals the budget cap. The GPU upscales it to the full quad.
        static triengine::vec2_i32 _calc_gen_size(triengine::vec2_i32 frame_size);

        // Stretch the quad to the given frame aspect so it exactly fills the orthographic
        // viewport (ortho half_w = half_h * aspect).
        void _fit_quad_to_aspect(triengine::vec2_i32 frame_size);

        // Recreate the (small) generation texture at `gen_size`, repoint the quad, and
        // retarget the producer.
        void _resize_gen_texture(triengine::vec2_i32 gen_size);

    private:
        // Generates the animated CPU images off-thread (defined in the .cc).
        class async_pattern_producer;
        std::unique_ptr<async_pattern_producer> _producer;

        triengine::texture_handle_t _screen_tex_handle{ triengine::kInvalidTextureHandle };

        // Size of the generated (small) texture. A produced frame is uploaded only if it
        // matches, so frames generated at a stale size (mid-resize) are dropped.
        triengine::vec2_i32 _gen_size{ 0, 0 };
        // Last frame size seen by update(), to detect changes (drives gen-size + quad aspect).
        triengine::vec2_i32 _frame_size{ 0, 0 };

        std::shared_ptr<triengine::geometry::mesh_object> _screen2d_quad;

        // The frame checked out by update() and held until post_render() returns it.
        std::shared_ptr<triengine::image_buffer> _inflight_frame;

    }; // class

} // namespace
