#include "renderer_process.hh"

#include <triengine_interop/surface/surface_producer.hh>

#include "task_dispatcher.hh"

#include <xutl/concurrency/spin_lock.hh>
#include <xutl/debug/logger.hh>

// Short namespace aliases for the triengine_interop surface protocol and API.
namespace ipc_proto = triengine_interop::surface::proto;
namespace ipc_surface = triengine_interop::surface;

// The per-session interface the surface producer drives. Created once and hosted for the
// single consumer: the GL renderer/scene is built on connect (on_session_init) and torn down on
// disconnect, so each connection starts fresh. Surface work marshals onto the main
// render thread (the offscreen renderer is not thread-safe); input runs on the IPC
// thread and uses the spin lock to guard the scene against the render thread.
class renderer_process::impl
    : public ipc_surface::surface_producer::session_interface
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
        if (!_renderer) { return false; }
        this->_update_renderer_scene();

        // Render with frame synchronization
        constexpr uint64_t mutex_key = 0;
        XUTL_ASSERT(_renderer->render(mutex_key));
        return true;
    }

    // --- session_interface ---

    void on_session_init(
        int32_t width, int32_t height,
        LUID& out_adapter_luid,
        HANDLE& out_surface_handle) override
    {
        _main_task_dispatcher->submit_task([this, width, height, &out_adapter_luid, &out_surface_handle]()
        {
            XUTL_INFO("Start initialization... (frame size: {}x{})", width, height);
            this->_create_renderer(width, height);
            this->_create_renderer_scene();

            DXGI_ADAPTER_DESC desc{};
            _renderer->get_dxgi_adapter()->GetDesc(&desc);
            out_adapter_luid = desc.AdapterLuid;
            out_surface_handle = _renderer->get_surface_handle().get();

            XUTL_DEBUG("initialize complete");
        }).get();
    }

    void on_frame_resize_event(
        int32_t width, int32_t height, 
        HANDLE& out_surface_handle) override
    {
        const triengine::vec2_i32 new_frame_size{ width, height };
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

    void on_mouse_button_event(
        [[maybe_unused]] int32_t x,
        [[maybe_unused]] int32_t y,
        ipc_proto::mouse_button_type button,
        ipc_proto::button_action_type action,
        [[maybe_unused]] ipc_proto::modifier_button_type mods) override
    {
        std::scoped_lock lk{ _ipc_lock };

        switch (button) {
        case ipc_proto::MOUSE_L:
            _flag_l_mouse_pressed = action != ipc_proto::ACTION_RELEASE;
            break;
        case ipc_proto::MOUSE_R:
            _flag_r_mouse_pressed = action != ipc_proto::ACTION_RELEASE;
            break;
        case ipc_proto::MOUSE_M:
            _flag_m_mouse_pressed = action != ipc_proto::ACTION_RELEASE;
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
        int32_t x,
        int32_t y,
        [[maybe_unused]] ipc_proto::modifier_button_type mods) override
    {
        std::scoped_lock lk{ _ipc_lock };
        if (!_renderer || !_scene) { return; }

        const triengine::vec2_f32 cursor_screen_pos{
            static_cast<float>(x),
            static_cast<float>(y)
        };

        if (_flag_mouse_dragging)
        {
            const triengine::vec2_f32 move_offset{
                cursor_screen_pos.x() - _begin_click_cursor_screen_pos.value_or(cursor_screen_pos).x(),
                _begin_click_cursor_screen_pos.value_or(cursor_screen_pos).y() - cursor_screen_pos.y() // reversed since y-coordinates go from bottom to top
            };

            triengine::abstract_camera* const scn_camera = _scene->get_camera();

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
        if (!_scene) { return; }
        _scene->get_camera()->process_mouse_zoom(yoffset);
    }

    void on_session_disconnect() override
    {
        // Tear down the GL state on the render thread so the next connection starts
        // fresh. Fire-and-forget: the task runs (FIFO) before any subsequent on_session_init.
        _main_task_dispatcher->submit_task([this]() {
            XUTL_DEBUG("Session disconnected, cleaning up GL renderer resources...");
            std::scoped_lock lk{ _ipc_lock };
            if (_renderer) {
                _renderer->destroy_renderer();
            }
            _skull_mesh.reset();
            _skull_mesh2.reset();
            _scene.reset();
            _renderer.reset();
            _flag_mouse_dragging = false;
            _flag_l_mouse_pressed = false;
            _flag_r_mouse_pressed = false;
            _flag_m_mouse_pressed = false;
            _begin_click_cursor_screen_pos.reset();
        });
    }

private:
    void _create_renderer(
        const int32_t frame_width,
        const int32_t frame_height)
    {
        XUTL_TRACE("initialize renderer... (frame size: {}x{})"
            , frame_width
            , frame_height
        );

        _renderer = std::make_unique<triengine::visualization::offscreen_renderer_dx>();
        _renderer->create_renderer(triengine::vec2_i32{
            frame_width,
            frame_height
        });

        XUTL_TRACE("Renderer created successfully.");
    }

    void _create_renderer_scene()
    {
        XUTL_TRACE("Create renderer scene...");

        const auto rsrc_dir_path = triengine::global_options::instance()->get_resource_directory();

        auto scn = _renderer->add_scene();
        scn->set_name("main");

        scn->get_render_config()->show_origin_xz_grid = true;
        scn->get_render_config()->light_opts.point_light.position = triengine::vec3_f32{ 0.0f, 1.5f, -1.5f };
        scn->get_render_config()->light_opts.point_light.ambient_intensity = 0.0f;
        scn->get_render_config()->light_opts.point_light.diffuse_intensity = 2.5f;
        scn->get_render_config()->light_opts.point_light.specular_intensity = 1.35f;

        scn->switch_camera_type(triengine::camera_type::arcball);
        scn->get_camera()->as<triengine::arcball_camera>()->get_options().damping_factor = 11.0f;

        auto mesh_axis_frame = triengine::geometry::mesh_object::create_coordinate_frame(0.5f);
        scn->add_geometry(mesh_axis_frame);

        _skull_mesh = std::make_shared<triengine::geometry::mesh_object>();
        if (triengine::io::load_mesh_from_obj(
            rsrc_dir_path / "objects/skull/12140_Skull_v3_L2.obj",
            false,
            *scn,
            *_skull_mesh
        ))
        {
            _skull_mesh->set_model(
                triengine::math::scale(_skull_mesh->get_model(), triengine::vec3_f32(0.0125f, 0.0125f, 0.0125f))
            );

            _skull_mesh->apply_model_in_place();

            Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
            R = Eigen::AngleAxisf(triengine::math::deg2rad(0.0f), Eigen::Vector3f::UnitZ())
                * Eigen::AngleAxisf(triengine::math::deg2rad(180.0f), Eigen::Vector3f::UnitY())
                * Eigen::AngleAxisf(triengine::math::deg2rad(-90.0f), Eigen::Vector3f::UnitX());

            _skull_mesh->rotate(R, true);
            _skull_mesh->translate(triengine::vec3_f32(0.0f, 0.5f, 0.0f), true);
            scn->add_geometry(_skull_mesh);
        }

        _skull_mesh2 = std::make_shared<triengine::geometry::mesh_object>();
        if (triengine::io::load_mesh_from_obj(
            rsrc_dir_path / "objects/skull/12140_Skull_v3_L2.obj",
            false,
            *scn,
            *_skull_mesh2
        ))
        {
            _skull_mesh2->set_model(
                triengine::math::scale(_skull_mesh2->get_model(), triengine::vec3_f32(0.0125f, 0.0125f, 0.0125f))
            );
            _skull_mesh2->get_texture_shading_material()->alpha = 0.5f;
            _skull_mesh2->apply_model_in_place();

            Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
            R = Eigen::AngleAxisf(triengine::math::deg2rad(0.0f), Eigen::Vector3f::UnitZ())
                * Eigen::AngleAxisf(triengine::math::deg2rad(180.0f), Eigen::Vector3f::UnitY())
                * Eigen::AngleAxisf(triengine::math::deg2rad(-90.0f), Eigen::Vector3f::UnitX());
            _skull_mesh2->rotate(R, true);
            _skull_mesh2->translate(triengine::vec3_f32(0.0f, 0.6f, 0.0f), true);

            scn->add_geometry(_skull_mesh2);
        }

        _scene = scn;
        XUTL_TRACE("Renderer scene created successfully.");
    }

    void _update_renderer_scene()
    {
        constexpr float rotSpeed = triengine::math::pi<float>() / 8.0f;
        const float dT = static_cast<float>(::glfwGetTime());

        Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
        R = Eigen::AngleAxisf(triengine::math::deg2rad(-90.0f), Eigen::Vector3f::UnitX()) *
            Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitZ());

        if (_skull_mesh) {
            _skull_mesh->rotate(R);
        }
    }

private:
    std::shared_ptr<task_dispatcher> _main_task_dispatcher;

    _XUTL concurrency::spin_lock _ipc_lock;

    // GL Renderer
    std::unique_ptr<triengine::visualization::offscreen_renderer_dx> _renderer;
    std::shared_ptr<triengine::scene> _scene;
    std::shared_ptr<triengine::geometry::mesh_object> _skull_mesh;
    std::shared_ptr<triengine::geometry::mesh_object> _skull_mesh2;

    bool _flag_mouse_dragging{ false };
    bool _flag_l_mouse_pressed{ false };
    bool _flag_r_mouse_pressed{ false };
    bool _flag_m_mouse_pressed{ false };
    std::optional<triengine::vec2_f32> _begin_click_cursor_screen_pos;

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
        static int frameCount = 0;
        ++frameCount;
        static auto lastTime = std::chrono::steady_clock::now();
        const auto currentTime = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime) >= 1s)
        {
            XUTL_DEBUG("Render FPS: {}", frameCount);
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
