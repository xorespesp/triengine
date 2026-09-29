#pragma once
#include <triengine/common.h>
#if !defined(_TRIENGINE_PLATFORM_WIN32)
#  error Unsupported platform
#endif // ^^^ _TRIENGINE_PLATFORM_WIN32 ^^^

#include <windows.h>
#include <dxgi1_2.h> // DXGI 1.2 API header
#include <d3d11_2.h> // DX11.2 API header
#include <wrl/client.h> // Microsoft::WRL::ComPtr

#include <triengine/scene.hh>
#include <triengine/core/graphics_device_info.hh>
#include <triengine/utility/noncopyable.hh>

#include <memory>

namespace triengine
{
    using shared_win32_handle = std::shared_ptr<std::remove_pointer_t<HANDLE>>;

} // namespace

namespace triengine::visualization
{
    // Renders triengine scenes to a Direct3D11 shared surface via GL/DX interop
    // (EXT_memory_object), for cross-process consumption. Like `offscreen_renderer`,
    // the GL context can be owned by any single thread.
    //
    // Threading contract:
    //   - Construct and destroy this object on the GLFW main thread: the constructor
    //     creates the GLFW window and the destructor destroys it (GLFW requires
    //     window create/destroy on the GLFW main thread).
    //   - Call `create()`, `render()`/`resize_frame()`/scene work, and `destroy()`
    //     all on ONE render thread, in that order. `create()` must precede any
    //     scene/render/resize use, and `destroy()` must run on that render thread
    //     before this object is destroyed.
    //   - Unless documented otherwise, methods are thread-unsafe and must be called
    //     on the render thread.
    // 
    // NOTE: the "GLFW main thread" is the thread that called `glfwInit()`,
    //       which is distinct from the render thread that owns the GL context.
    class offscreen_renderer_dx
        : utility::noncopyable
    {
    public:
        // Creates the hidden GLFW window that carries the GL context. 
        // Does not initialize GL/DX and does not commit to an output size or frame-rate cap;
        // call `create()` next.
        //
        // NOTE: MUST be called on the GLFW main thread.
        //       (GLFW requires window creation there)
        offscreen_renderer_dx();

        // Destroys the hidden GLFW window.
        //
        // NOTE: Destructor MUST be called on the GLFW main thread.
        //       `destroy()` must run on the render thread before this object is destroyed.
        virtual ~offscreen_renderer_dx();

        // `nullptr` until `create()` has initialized the context.
        const core::graphics_device_info* get_graphics_device_info() const noexcept;

        // GL/DX interop accessors (D3D11 device/context, target surface, frame size).
        // Valid only after `create()`.
        // NOTE: MUST be called on the render thread (these resources are created and
        // replaced by `create()`/`resize_frame()`/`destroy()` on that thread).
        Microsoft::WRL::ComPtr<IDXGIAdapter> get_dxgi_adapter() const noexcept;
        Microsoft::WRL::ComPtr<ID3D11Device2> get_dx11_device() const noexcept;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext2> get_dx11_device_context() const noexcept;
        vec2_i32 get_frame_size() const noexcept;
        shared_win32_handle get_surface_handle() const;

        // Initializes the GL context, sets the render target to `initial_frame_size`.
        // Call once before any scene/render/resize use.
        //
        // NOTE: MUST be called on the render thread that will own the GL context
        // (the single thread used for all scene/render/resize work). 
        // The context stays current on this thread until `destroy()`.
        //
        // `max_fps` is an upper bound on the producer's frame rate, NOT a vsync (0 == uncapped).
        // It is a free-running software cap: a high-resolution timer throttles how fast
        // frames are produced and is unrelated to any display refresh or to the consumer's
        // consumption rate (offscreen rendering has no display vblank to sync against).
        //
        // Note this is fundamentally different from a single-pipeline vsync, where the
        // awaited frame is the one actually displayed. Here the produced frames travel
        // through a separate, unsynchronized consumer (shared-surface IPC), so the producer
        // and consumer run as two independent loops. Capping at the consumer's display rate
        // therefore still leaves up to one cap-interval of staleness before the consumer
        // samples a frame. To keep input latency low, set `max_fps` ABOVE the consumer's
        // display rate (over-produce); lower it to save CPU at the cost of latency.
        // Changeable at runtime via `change_max_fps()`.
        void create(vec2_i32 initial_frame_size, uint32_t max_fps = 0);

        // Tears down the GL/DX interop pipeline and the GL context. 
        // Call before destroying this object.
        //
        // NOTE: MUST be called on the render thread that called `create()`.
        void destroy();

        // Current frame-rate cap (0 == uncapped).
        uint32_t get_max_fps() const noexcept;

        // Updates the frame-rate cap at runtime (0 disables it).
        //
        // NOTE: MUST be called on the render thread.
        // (the same single thread that runs `create()`/`render()`)
        // This issues no GL calls, but it mutates frame-pacing state that 
        // `render()` reads, so calling it from another thread races with
        // the render loop.
        void change_max_fps(uint32_t max_fps);

        // Scene management.
        // NOTE: MUST be called on the render thread.
        // (these read or mutate the scene list that `render()` uses)
        std::shared_ptr<scene> add_scene();
        void remove_scene(scene_id_t scn_id);

        void switch_scene(scene_id_t scn_id);
        void switch_to_previous_scene();
        void switch_to_next_scene();

        std::shared_ptr<const scene> find_scene(scene_id_t scn_id) const;
        std::shared_ptr<scene> find_scene(scene_id_t scn_id);

        std::shared_ptr<const scene> get_current_scene() const;
        std::shared_ptr<scene> get_current_scene();

        // Renders the current scene into the shared D3D11 surface, guarding it with
        // the interop keyed mutex acquired/released under `mutex_key`.
        // NOTE: MUST be called on the render thread.
        bool render(uint64_t mutex_key);

        // Resizes the render target and rebuilds the shared surface; 
        // returns the new surface handle (the previous handle is invalidated).
        // NOTE: MUST be called on the render thread.
        shared_win32_handle resize_frame(vec2_i32 new_frame_size);

    private:
        struct impl;
        std::unique_ptr<impl> _imp;

    }; // class

} // namespace