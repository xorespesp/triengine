#include "renderer_process.hh"

#include <triengine_interop/surface/surface_producer.hh>

#include "scene_manager.hh"
#include "scene/skull_scene.hh"
#include "scene/plasma_scene.hh"
#include "task_dispatcher.hh"

#include <xutl/concurrency/spin_lock.hh>
#include <xutl/debug/logger.hh>

namespace surface_proto = triengine_interop::surface::proto;

// The per-session interface the surface producer drives. Created once and hosted for the
// single consumer: the GL renderer/scene is built on connect (on_session_init) and torn down on
// disconnect, so each connection starts fresh. Surface work marshals onto the main
// render thread (the offscreen renderer is not thread-safe); input runs on the IPC
// thread and uses the spin lock to guard the scene against the render thread.
class renderer_process::impl
    : public triengine_interop::surface::surface_producer::session_interface
{
public:
    impl()
        : _main_task_dispatcher{ std::make_shared<task_dispatcher>() }
    {
        XUTL_TRACE("{}() ENTER", __func__);
        XUTL_TRACE("{}() LEAVE", __func__);
    }

    ~impl() override
    {
        XUTL_TRACE("{}() ENTER", __func__);
        // Destroy the scenes (stops the plasma worker) before the renderer they reference.
        _scene_mgr.reset();
        if (_renderer) {
            _renderer->destroy_renderer();
        }
        XUTL_TRACE("{}() LEAVE", __func__);
    }

    // Pump marshaled tasks and render the current frame. 
    // Returns true if a frame was rendered. 
    // NOTE: MUST be called on the main render thread.
    bool poll()
    {
        _main_task_dispatcher->dispatch_pending_tasks();

        std::unique_lock lk{ _ipc_lock };
        if (!_renderer || !_scene_mgr) { return false; }

        this->_apply_pending_scene_switch();
        this->_process_scene_camera_keyboard_input();

        scene_wrapper* const curr_scn = _scene_mgr->current();

        // Per-frame update before render (skull rotation, plasma texture upload, ...).
        if (curr_scn) { curr_scn->update(_renderer->get_frame_size()); }

        // Render with frame synchronization
        XUTL_ASSERT(_renderer->render(surface_proto::SHARED_SURFACE_MUTEX_KEY));

        // After render: let the scene release anything the render consumed (e.g. recycle the
        // plasma frame buffer whose upload was just applied).
        if (curr_scn) { curr_scn->post_render(); }
        return true;
    }

    // --- session_interface ---

    void on_session_init(
        SIZE initial_frame_size,
        uint32_t requested_max_fps,
        LUID& out_adapter_luid,
        HANDLE& out_surface_handle) override
    {
        const int32_t width = static_cast<int32_t>(initial_frame_size.cx);
        const int32_t height = static_cast<int32_t>(initial_frame_size.cy);
        _main_task_dispatcher->submit_task([this, width, height, requested_max_fps, &out_adapter_luid, &out_surface_handle]()
        {
            XUTL_INFO("Start initialization... (frame size: {}x{}, max fps: {})", width, height, requested_max_fps);
            this->_create_renderer(width, height, requested_max_fps);
            this->_create_renderer_scenes();

            DXGI_ADAPTER_DESC desc{};
            _renderer->get_dxgi_adapter()->GetDesc(&desc);
            out_adapter_luid = desc.AdapterLuid;
            out_surface_handle = _renderer->get_surface_handle().get();

            XUTL_DEBUG("initialize complete");
        }).get();
    }

    void on_frame_resize_event(
        SIZE new_size,
        HANDLE& out_surface_handle) override
    {
        const triengine::vec2_i32 new_frame_size{
            static_cast<int32_t>(new_size.cx),
            static_cast<int32_t>(new_size.cy)
        };
        out_surface_handle = _main_task_dispatcher->submit_task([this, new_frame_size]() -> triengine::shared_win32_handle
        {
            XUTL_TRACE("frame resize start... (target size: {}x{})", new_frame_size.x(), new_frame_size.y());

            auto new_surface_handle = _renderer->resize_frame(new_frame_size);

            XUTL_TRACE("frame resize complete! (update surface handle: 0x{:X})"
                , new_surface_handle.get()
            );

            return new_surface_handle;
        }).get().get();
    }

    void on_change_max_fps(uint32_t max_fps) override
    {
        // change_max_fps() must run on the render thread, so marshal it like init/resize.
        _main_task_dispatcher->submit_task([this, max_fps]()
        {
            XUTL_TRACE("change max fps... (max fps: {})", max_fps);
            if (_renderer) {
                _renderer->change_max_fps(max_fps);
            }
        });
    }

    void on_mouse_button_event(
        [[maybe_unused]] POINT pos,
        surface_proto::mouse_button_type button,
        surface_proto::button_action_type action,
        [[maybe_unused]] surface_proto::modifier_button_type mods) override
    {
        std::scoped_lock lk{ _ipc_lock };

        switch (button) {
        case surface_proto::MOUSE_L:
            _flag_l_mouse_pressed = action != surface_proto::ACTION_RELEASE;
            break;
        case surface_proto::MOUSE_R:
            _flag_r_mouse_pressed = action != surface_proto::ACTION_RELEASE;
            break;
        case surface_proto::MOUSE_M:
            _flag_m_mouse_pressed = action != surface_proto::ACTION_RELEASE;
            break;
        default:
            break;
        }

        _flag_mouse_dragging =
            _flag_l_mouse_pressed ||
            _flag_r_mouse_pressed ||
            _flag_m_mouse_pressed;
    }

    void on_mouse_move_event(
        POINT pos,
        [[maybe_unused]] surface_proto::modifier_button_type mods) override
    {
        std::scoped_lock lk{ _ipc_lock };
        triengine::scene* const scn = this->_current_scene();
        if (!_renderer || !scn) { return; }

        const triengine::vec2_f32 cursor_screen_pos{
            static_cast<float>(pos.x),
            static_cast<float>(pos.y)
        };

        if (_flag_mouse_dragging)
        {
            const triengine::vec2_f32 move_offset{
                cursor_screen_pos.x() - _begin_click_cursor_screen_pos.value_or(cursor_screen_pos).x(),
                _begin_click_cursor_screen_pos.value_or(cursor_screen_pos).y() - cursor_screen_pos.y() // reversed since y-coordinates go from bottom to top
            };

            triengine::abstract_camera* const scn_camera = scn->get_camera();

            if (_flag_l_mouse_pressed)
            {
                scn_camera->process_mouse_rotation(move_offset);
            }
            else if (_flag_m_mouse_pressed)
            {
                const triengine::vec2_i32 screen_size = _renderer->get_frame_size().cast<int32_t>();
                const triengine::view_port& screen_viewport = scn_camera->get_viewport();

                const triengine::vec2_f32 start_pos = triengine::win32_screen_pos_2_gl_viewport_pos(
                    _begin_click_cursor_screen_pos.value_or(cursor_screen_pos),
                    screen_size,
                    screen_viewport
                );

                const triengine::vec2_f32 end_pos = triengine::win32_screen_pos_2_gl_viewport_pos(
                    cursor_screen_pos,
                    screen_size,
                    screen_viewport
                );

                scn_camera->process_mouse_translation(
                    start_pos,
                    end_pos
                );
            }

            _begin_click_cursor_screen_pos = cursor_screen_pos;
        }
        else //if (!_flag_mouse_dragging)
        {
            if (_begin_click_cursor_screen_pos) {
                _begin_click_cursor_screen_pos.reset();
            }
        }
    }

    void on_mouse_scroll_event(float yoffset) override
    {
        std::scoped_lock lk{ _ipc_lock };
        triengine::scene* const scn = this->_current_scene();
        if (!scn) { return; }
        scn->get_camera()->process_mouse_zoom(yoffset);
    }

    void on_key_event(
        surface_proto::key_button_type key,
        surface_proto::button_action_type action,
        [[maybe_unused]] surface_proto::modifier_button_type mods) override
    {
        std::scoped_lock lk{ _ipc_lock };
        if (!_renderer || !_scene_mgr) { return; }

        XUTL_TRACE("key event -> key: {}, action: {}"
            , static_cast<int>(key)
            , static_cast<int>(action)
        );

        // Left/Right switch scenes on a discrete press. The actual switch mutates the scene
        // manager (and the renderer's current scene), so it is marshaled to the render thread
        // (applied in poll()) instead of mutating shared state from this IPC thread.
        if (action == surface_proto::ACTION_PRESS) {
            switch (key) {
            case surface_proto::KEY_LEFT: _pending_scene_switch = -1; return;
            case surface_proto::KEY_RIGHT: _pending_scene_switch = +1; return;
            default: break;
            }
        }

        // Track W/A/S/D as held state instead of acting on each event.
        const bool pressed = action != surface_proto::ACTION_RELEASE;
        switch (key) {
        case surface_proto::KEY_W: _flag_key_w_pressed = pressed; break;
        case surface_proto::KEY_A: _flag_key_a_pressed = pressed; break;
        case surface_proto::KEY_S: _flag_key_s_pressed = pressed; break;
        case surface_proto::KEY_D: _flag_key_d_pressed = pressed; break;
        default: break;
        }
    }

    void on_session_disconnect() override
    {
        // Tear down the GL state on the render thread so the next connection starts
        // fresh. Fire-and-forget: the task runs (FIFO) before any subsequent on_session_init.
        _main_task_dispatcher->submit_task([this]() {
            XUTL_DEBUG("Session disconnected, cleaning up GL renderer resources...");
            std::scoped_lock lk{ _ipc_lock };
            // Destroy the scenes (stops the plasma worker) before tearing down the renderer
            // they live in, so the next connection starts fresh.
            _scene_mgr.reset();
            if (_renderer) {
                _renderer->destroy_renderer();
            }
            _renderer.reset();
            _pending_scene_switch = 0;
            _flag_mouse_dragging = false;
            _flag_l_mouse_pressed = false;
            _flag_r_mouse_pressed = false;
            _flag_m_mouse_pressed = false;
            _begin_click_cursor_screen_pos.reset();
            _flag_key_w_pressed = false;
            _flag_key_a_pressed = false;
            _flag_key_s_pressed = false;
            _flag_key_d_pressed = false;
            _last_key_update_time.reset();
        });
    }

private:
    void _create_renderer(
        const int32_t frame_width,
        const int32_t frame_height,
        const uint32_t max_fps)
    {
        XUTL_TRACE("initialize renderer... (frame size: {}x{}, max_fps: {})"
            , frame_width
            , frame_height
            , max_fps
        );

        _renderer = std::make_unique<triengine::visualization::offscreen_renderer_dx>();
        _renderer->create_renderer(
            triengine::vec2_i32{ frame_width, frame_height },
            max_fps
        );

        XUTL_TRACE("Renderer created successfully.");
    }

    void _create_renderer_scenes()
    {
        XUTL_TRACE("Create renderer scenes...");

        // Register the demo's scenes. The first one added becomes active, so skull starts
        // active. Adding a new scene type here is the only step needed to extend the demo.
        _scene_mgr = std::make_unique<scene_manager>(*_renderer);
        _scene_mgr->add(std::make_unique<scene::skull_scene>(*_renderer));
        _scene_mgr->add(std::make_unique<scene::plasma_scene>(*_renderer));

        XUTL_TRACE("Renderer scenes created successfully.");
    }

    // Apply a scene switch requested from the IPC thread (left/right arrow). Runs on the
    // render thread under _ipc_lock, so it can safely mutate the scene manager.
    void _apply_pending_scene_switch()
    {
        if (_pending_scene_switch == 0 || !_scene_mgr) { return; }

        if (_pending_scene_switch > 0) {
            _scene_mgr->switch_to_next();
        } else {
            _scene_mgr->switch_to_prev();
        }
        _pending_scene_switch = 0;

        if (const scene_wrapper* const curr = _scene_mgr->current()) {
            XUTL_DEBUG("Switched to scene: {}", curr->get_scene()->get_name());
        }
    }

    // Apply continuous camera translation for the held W/A/S/D keys. Driven once
    // per rendered frame so movement is smooth and frame-rate independent.
    // NOTE: MUST be called on the main render thread with _ipc_lock held.
    void _process_scene_camera_keyboard_input()
    {
        triengine::scene* const scn = this->_current_scene();
        if (!scn) { return; }

        const double now = ::glfwGetTime();
        const float frame_delta = _last_key_update_time
            ? static_cast<float>(now - *_last_key_update_time)
            : 0.0f;
        _last_key_update_time = now;

        triengine::abstract_camera* const scn_camera = scn->get_camera();
        if (_flag_key_w_pressed) { scn_camera->process_keyboard_translation(triengine::camera_movement_type::forward, frame_delta); }
        if (_flag_key_s_pressed) { scn_camera->process_keyboard_translation(triengine::camera_movement_type::backward, frame_delta); }
        if (_flag_key_a_pressed) { scn_camera->process_keyboard_translation(triengine::camera_movement_type::left, frame_delta); }
        if (_flag_key_d_pressed) { scn_camera->process_keyboard_translation(triengine::camera_movement_type::right, frame_delta); }
    }

    // The active scene (mirrors the manager's current scene), or nullptr if none. Input is
    // routed to its camera. MUST be called with _ipc_lock held.
    triengine::scene* _current_scene() noexcept
    {
        if (!_scene_mgr) { return nullptr; }
        scene_wrapper* const curr = _scene_mgr->current();
        return curr ? curr->get_scene().get() : nullptr;
    }

private:
    std::shared_ptr<task_dispatcher> _main_task_dispatcher;

    _XUTL concurrency::spin_lock _ipc_lock;

    // GL Renderer
    std::unique_ptr<triengine::visualization::offscreen_renderer_dx> _renderer;

    // Owns the demo's scenes and tracks the active one. Created on session init, reset on
    // disconnect (before the renderer it references is destroyed).
    std::unique_ptr<scene_manager> _scene_mgr;

    // Pending scene switch requested from the IPC thread: -1 previous, +1 next, 0 none.
    // Applied on the render thread in poll().
    int _pending_scene_switch{ 0 };

    bool _flag_mouse_dragging{ false };
    bool _flag_l_mouse_pressed{ false };
    bool _flag_r_mouse_pressed{ false };
    bool _flag_m_mouse_pressed{ false };
    std::optional<triengine::vec2_f32> _begin_click_cursor_screen_pos;

    // Held state for the W/A/S/D camera movement keys, sampled per frame.
    bool _flag_key_w_pressed{ false };
    bool _flag_key_a_pressed{ false };
    bool _flag_key_s_pressed{ false };
    bool _flag_key_d_pressed{ false };
    std::optional<double> _last_key_update_time;

}; // class

renderer_process::renderer_process()
{
    XUTL_INFO("GL renderer process spawned (PID: {})"
        , ::GetCurrentProcessId()
    );

    XUTL_DEBUG("Creating IPC server...");

    // impl is the producer's session interface; its ctor touches no GL, so create it up
    // front and host it for the single consumer.
    _imp = std::make_shared<impl>();
    _producer.start(Config::RENDERER_SERVER_NAME, _imp);

    XUTL_DEBUG("Created!");

    _process_inst_handle.reset(
        ::CreateEventA(nullptr, TRUE, TRUE, Config::RENDERER_PROCESS_INST_NAME),
        ::CloseHandle
    );

    _run_flag = true;
}

renderer_process::~renderer_process()
{
    XUTL_DEBUG("Cleaning up GL renderer resources...");
}

void renderer_process::run()
{
    XUTL_INFO("Initialization complete. entering render loop...");

    while (_run_flag)
    {
        // Pumps marshaled init/resize/teardown tasks and renders when a consumer is
        // connected; idles otherwise.
        if (!_imp->poll())
        {
            std::this_thread::sleep_for(16ms);
            continue;
        }

        // log average of frame time and fps every 1 second
        thread_local int frameCount = 0;
        ++frameCount;
        thread_local auto lastTime = std::chrono::steady_clock::now();
        const auto currentTime = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime) >= 1s) {
            XUTL_DEBUG("Renderer FPS: {}", frameCount);
            lastTime = currentTime;
            frameCount = 0;
        }

    } // while

    XUTL_INFO("Render loop complete");
}

void renderer_process::stop()
{
    // Stop the surface producer before signaling the render loop to exit. This is called
    // from a different thread than run(), so the render loop keeps pumping while the
    // producer joins its IPC threads: any IPC thread blocked in on_session_init /
    // on_frame_resize_event (which marshal onto the render thread and wait) can complete its
    // task and unblock, letting the join finish. Clearing _run_flag first would stop the
    // pump while an IPC thread is still waiting on it, deadlocking the join.
    _producer.stop();
    _run_flag = false;
}
