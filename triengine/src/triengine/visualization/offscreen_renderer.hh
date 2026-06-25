#pragma once
#include <triengine/common.h>
#include <triengine/image_buffer.hh>
#include <triengine/core/gl_context.hh>
#include <triengine/core/scene_renderer.hh>
#include <triengine/utility/noncopyable.hh>

#include <functional>
#include <unordered_map>
#include <array>

namespace triengine::visualization
{
    // Renders triengine scenes to an offscreen framebuffer and reads the result
    // back into an `image_buffer`, using a hidden GLFW window/context (no on-screen
    // output). The OpenGL context can be owned by any single thread, so the whole
    // render loop may run off the GLFW main thread.
    //
    // Threading contract:
    //   - Construct and destroy this object on the GLFW main thread: the constructor
    //     creates the GLFW window and the destructor destroys it, and GLFW
    //     requires window creation/destruction on the GLFW main thread.
    //   - Call `create()`, `add_scene()`/`render()`/other scene work, and `destroy()`
    //     all on ONE render thread, in that order. `create()` must precede any
    //     scene/render call, and `destroy()` must run on that render thread before
    //     this object is destroyed.
    // 
    // NOTE: the "GLFW main thread" is the thread that called `glfwInit()`,
    //       which is distinct from the render thread that owns the GL context.
    class offscreen_renderer
        : utility::noncopyable
    {
    public:
        // Creates the hidden GLFW window that carries the GL context.
        // Does not initialize GL and does not commit to an output size; call `create()` next.
        //
        // NOTE: MUST be called on the GLFW main thread.
        //       (GLFW requires window creation there)
        offscreen_renderer();

        // Destroys the hidden GLFW window.
        //
        // NOTE: Destructor MUST be called on the GLFW main thread.
        //       `destroy()` must run on the render thread before this object is destroyed.
        virtual ~offscreen_renderer();

        // Returns the owned GL context (e.g. to register window/input callbacks).
        //
        // NOTE: Safe from any thread (returns a stable pointer); GL operations
        // through the returned context follow `gl_context`'s own thread rules.
        const core::gl_context* get_gl_context() const noexcept { return &_glctx; }
        core::gl_context* get_gl_context() noexcept { return &_glctx; }

        // Initializes the GL context and sets the offscreen render target to `initial_frame_size`.
        //
        // NOTE: MUST be called on the render thread that will own the GL context
        // (the single thread used for all scene/render work). The context stays
        // current on this thread until `destroy()`.
        void create(vec2_i32 initial_frame_size);

        // Tears down the scene renderer and the GL context. 
        // Call before destroying this object.
        //
        // NOTE: MUST be called on the render thread that called `create()`.
        void destroy();

        // Scene management.
        // NOTE: MUST be called on the render thread. 
        // (these read or mutate the scene list that `render()` uses).
        std::shared_ptr<scene> add_scene();
        void remove_scene(scene_id_t scn_id);

        void switch_scene(scene_id_t scn_id);
        void switch_to_previous_scene();
        void switch_to_next_scene();

        std::shared_ptr<const scene> find_scene(scene_id_t scn_id) const;
        std::shared_ptr<scene> find_scene(scene_id_t scn_id);

        std::shared_ptr<const scene> get_current_scene() const;
        std::shared_ptr<scene> get_current_scene();

        // NOTE: MUST be called on the render thread.
        vec2_i32 get_frame_size() const noexcept;

        // Changes the render target size (re-applied on the next `render()`).
        // NOTE: MUST be called on the render thread.
        void resize_frame(vec2_i32 new_frame_size);

        // Renders the current scene and reads it back into `frame_image`.
        // NOTE: MUST be called on the render thread.
        void render(
            image_buffer& frame_image/* out */,
            image_format_type frame_image_format = image_format_type::bgra
        );

    private:
        void _begin_frame();
        void _end_frame();
        
    private:
        bool _is_created{ false };
        bool _flag_invalidate_fbo{ true };

        core::gl_context _glctx;
        vec2_i32 _curr_frame_size{};

        core::scene_renderer _scn_renderer;
        
        std::list<std::shared_ptr<scene>> _scn_list;
        std::list<std::shared_ptr<scene>>::iterator _curr_scn_it{ _scn_list.end() };
        std::unordered_map<
            scene_id_t, 
            std::list<std::shared_ptr<scene>>::iterator
        > _scn_id_map;

        core::frame_buffer _fb_main;

        // frame time calculation
        double _frame_time_delta{ 0.0 }, _last_frame_time{ 0.0 };

    }; // class

} // namespace