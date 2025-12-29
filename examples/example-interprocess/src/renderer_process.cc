#include "renderer_process.hh"

#include <cxlib/utils/spin_lock.hh>
#include <cxlib/utils/logger.hh>

class renderer_process::impl
{
public:
    impl(
        std::shared_ptr<ipc_session> session)
        : _ipc_session{ session }
        , _main_task_dispatcher{ std::make_shared<task_dispatcher>() }
    {
        CXLIB_TRACE("{}() ENTER", __func__);
        this->_initialize();
        CXLIB_TRACE("{}() LEAVE", __func__);
    }

    ~impl()
    {
        CXLIB_TRACE("{}() ENTER", __func__);
        _renderer->destroy_renderer();
        CXLIB_TRACE("{}() LEAVE", __func__);
    }

    // NOTE: This method MUST be called in the main thread 
    void poll()
    {
        _main_task_dispatcher->dispatch_pending_tasks();

        if (_fl_initialized)
        {
            std::unique_lock lk{ _ipc_lock };
            if (!_renderer) { return; }
            this->_update_scene();

            // Render with frame synchronization
            constexpr uint64_t mutex_key = 0;
            CXLIB_ASSERT(_renderer->render(mutex_key));
            lk.unlock();
        }
    }

private:
    void _initialize()
    {
        //
        // IPC 세션 초기화
        //

        _ipc_session->set_request_callback(
            [this](
                [[maybe_unused]] const uint32_t req_pck_id, 
                const std::string_view req_pck_data, 
                std::vector<uint8_t>& rep_pck_data)
            {
                packet_view pck{ req_pck_data.data(), req_pck_data.size() };

                switch (pck.type()) {
                case ipc_proto::packet_type::init_request:
                {
                    auto body = pck.body<ipc_proto::packets::init_request_t>();
                    CXLIB_TRACE("----- recv init request: {}x{}"
                        , body->frame_width
                        , body->frame_height
                    );

                    // init 작업은 ipc 스레드에서 직접 수행하지 않고, 메인 렌더 스레드에 위임
                    auto init_future = _main_task_dispatcher->submit_task(
                        [this, init_req = *pck.body<ipc_proto::packets::init_request_t>()]() -> bool
                        {
                            CXLIB_INFO("Start initialization...");

                            this->_create_renderer(
                                init_req.frame_width,
                                init_req.frame_height
                            );

                            this->_create_renderer_scene();

                            CXLIB_DEBUG("initialize complete");
                            return true;
                        });

                    CXLIB_TRACE("IPC thread waiting for init completion...");
                    try {
                        const bool init_res = init_future.get();
                        if (!init_res) {
                            CXLIB_ERROR("Failed to initialize renderer.");
                            return;
                        }
                    } catch (const std::exception& e) {
                        CXLIB_ERROR("Initialization task failed: {}", e.what());
                        return;
                    }

                    DXGI_ADAPTER_DESC target_adapter_desc{};
                    _renderer->get_dxgi_adapter()->GetDesc(&target_adapter_desc);
                    auto surface_handle = _renderer->get_surface_handle();

                    CXLIB_TRACE("Sending init response...");
                    packet_builder<ipc_proto::packets::init_response_t> rep_pck{ ipc_proto::packet_type::init_response };
                    rep_pck.body()->renderer_process_id = ::GetCurrentProcessId();
                    rep_pck.body()->target_adapter_luid = target_adapter_desc.AdapterLuid;
                    rep_pck.body()->surface_handle = surface_handle.get();
                    CXLIB_TRACE("target adapter: {:x}-{:x}, surface handle: {:p}"
                        , target_adapter_desc.AdapterLuid.HighPart
                        , target_adapter_desc.AdapterLuid.LowPart
                        , surface_handle.get()
                    );
                    rep_pck_data.assign(rep_pck.data(), rep_pck.data() + rep_pck.size());

                    CXLIB_TRACE("Initialization complete!");
                    _fl_initialized = true;
                    break;
                }
                case ipc_proto::packet_type::frame_resize_request:
                {
                    auto body = pck.body<ipc_proto::packets::frame_resize_request_t>();
                    const triengine::vec2_i32 new_frame_size{ body->width, body->height };

                    // resize 작업은 ipc 스레드에서 직접 수행하지 않고, 메인 렌더 스레드에 위임
                    auto resize_future = _main_task_dispatcher->submit_task(
                        [this, new_frame_size]() -> triengine::shared_win32_handle
                        {
                            return this->_handle_frame_resize_request(new_frame_size);
                        });

                    CXLIB_TRACE("IPC thread waiting for resize completion...");
                    const auto new_surface_handle = resize_future.get();
                    if (!new_surface_handle) {
                        CXLIB_ERROR("Failed to resize frame");
                        return;
                    }

                    CXLIB_TRACE("Sending resize response...");
                    packet_builder<ipc_proto::packets::frame_resize_response_t> rep_pck{ ipc_proto::packet_type::frame_resize_response };
                    rep_pck.body()->surface_handle = new_surface_handle.get();
                    rep_pck_data.assign(rep_pck.data(), rep_pck.data() + rep_pck.size());
                    break;
                }
                default:
                    CXLIB_WARN("Got unknown request packet");
                    break;
                } // switch
            });
        
        _ipc_session->set_notify_callback(
            [this](
                [[maybe_unused]] const uint32_t id, 
                const std::string_view data)
            {
                packet_view pck{ data.data(), data.size() };

                switch (pck.type()) {
                case ipc_proto::packet_type::mouse_button_event:
                {
                    auto body = pck.body<ipc_proto::packets::mouse_button_event_t>();
                    //CXLIB_TRACE("mouse button {} action: {}"
                    //    , static_cast<int>(body->button)
                    //    , static_cast<int>(body->action)
                    //);

                    std::scoped_lock lk{ _ipc_lock };
                    [[maybe_unused]] triengine::abstract_camera* const scn_camera = _scene->get_camera();

                    switch (body->button) {
                    case ipc_proto::MOUSE_L:
                        _flag_l_mouse_pressed = body->action != ipc_proto::ACTION_RELEASE;
                        //CXLIB_TRACE("l mouse pressed : {}", _flag_m_mouse_pressed);
                        break;
                    case ipc_proto::MOUSE_R:
                        _flag_r_mouse_pressed = body->action != ipc_proto::ACTION_RELEASE;
                        //CXLIB_TRACE("r mouse pressed : {}", _flag_m_mouse_pressed);
                        break;
                    case ipc_proto::MOUSE_M:
                        _flag_m_mouse_pressed = body->action != ipc_proto::ACTION_RELEASE;
                        //CXLIB_TRACE("m mouse pressed : {}", _flag_m_mouse_pressed);
                        break;
                    default:
                        break;
                    }

                    _flag_mouse_dragging = 
                        _flag_l_mouse_pressed || 
                        _flag_r_mouse_pressed ||
                        _flag_m_mouse_pressed;

                    //CXLIB_TRACE("mouse dragging : {}", _flag_mouse_dragging);
                    break;
                }
                case ipc_proto::packet_type::mouse_move_event:
                {
                    auto body = pck.body<ipc_proto::packets::mouse_move_event_t>();
                    //CXLIB_TRACE("mouse move: {}x{}", body->x, body->y);

                    std::scoped_lock lk{ _ipc_lock };

                    const triengine::vec2_f32 cursor_screen_pos{
                        static_cast<float>(body->x),
                        static_cast<float>(body->y)
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
                    break;
                }
                case ipc_proto::packet_type::mouse_scroll_event:
                {
                    auto body = pck.body<ipc_proto::packets::mouse_scroll_event_t>();
                    //CXLIB_TRACE("mouse scroll: {}"
                    //    , body->yoffset
                    //);
                    std::scoped_lock lk{ _ipc_lock };
                    triengine::abstract_camera* const scn_camera = _scene->get_camera();
                
                    const float zoom_offset = body->yoffset;
                    scn_camera->process_mouse_zoom(static_cast<float>(zoom_offset));
                    break;
                }
                default:
                    CXLIB_WARN("Got unknown notify packet");
                    break;
                } // switch
            });

        // 세션 패킷 수신 시작 (초기화 요청 수신 대기)
        CXLIB_INFO("Start waiting for init request...");
        _ipc_session->start();
    }

    void _create_renderer(
        const int32_t frame_width,
        const int32_t frame_height)
    {
        CXLIB_TRACE("initialize renderer... (frame size: {}x{})"
            , frame_width
            , frame_height
        );

        _renderer = std::make_unique<triengine::visualization::offscreen_renderer_dx>();
        _renderer->create_renderer(triengine::vec2_i32{ 
            frame_width, 
            frame_height 
        });

        CXLIB_TRACE("Renderer created successfully.");
    }

    void _create_renderer_scene()
    {
        CXLIB_TRACE("Create renderer scene...");

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
        //mesh_axis_frame->paint_uniform_color(_get_next_color());
        //mesh_axis_frame->translate(Eigen::Vector3f{ 1.8f, 0.0f, -1.5f });
        scn->add_geometry(mesh_axis_frame);

        _skull_mesh = std::make_shared<triengine::geometry::mesh_object>();
        if (triengine::io::load_mesh_from_obj(
            rsrc_dir_path / "objects/skull/12140_Skull_v3_L2.obj",
            false,
            *scn,
            *_skull_mesh
        ))
        {
            //_skull_mesh->compute_vertex_normals();
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
            //_skull_mesh2->compute_vertex_normals();
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
        CXLIB_TRACE("Renderer scene created successfully.");
    }

    triengine::shared_win32_handle _handle_frame_resize_request(
        const triengine::vec2_i32 new_window_size)
    {
        CXLIB_TRACE("frame resize start... (target size: {}x{})", new_window_size.x(), new_window_size.y());

        auto new_surface_handle = _renderer->resize_frame(new_window_size);

        CXLIB_TRACE("frame resize complete! (update surface handle: 0x{:X})"
            , new_surface_handle.get()
        );

        return new_surface_handle;
    }

    void _update_scene()
    {
        constexpr float rotSpeed = triengine::math::pi<float>() / 8.0f;
        const float dT = static_cast<float>(::glfwGetTime());

        Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
        //R = Eigen::AngleAxisf(rotSpeed * dT * 0.1f, Eigen::Vector3f::UnitZ()) *
        //    Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitY()) *
        //    Eigen::AngleAxisf(rotSpeed * dT * 0.5f, Eigen::Vector3f::UnitX());

        R = Eigen::AngleAxisf(triengine::math::deg2rad(-90.0f), Eigen::Vector3f::UnitX()) *
            Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitZ());

        if (_skull_mesh) {
            _skull_mesh->rotate(R);
        }
    }

private:
    bool _fl_initialized{ false };
    std::shared_ptr<task_dispatcher> _main_task_dispatcher;

    std::shared_ptr<ipc_session> _ipc_session;
    _CXLIB utils::spin_lock _ipc_lock;

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

void renderer_process::impl_deleter::operator()(impl* p) const
{
    delete p;
}

renderer_process::renderer_process()
    : _main_task_dispatcher{ std::make_shared<task_dispatcher>() }
{
    CXLIB_INFO("GL renderer process spawned (PID: {})"
        , ::GetCurrentProcessId()
    );

    CXLIB_DEBUG("Creating IPC server...");
    _ipc_srv = std::make_shared<ipc_server>();

    _ipc_srv->set_session_connect_callback(
        [this](std::shared_ptr<ipc_session> session) {
            _main_task_dispatcher->submit_task([this, session]() {
                CXLIB_DEBUG("Session connected, initializing GL renderer...");

                // NOTE: `impl` object MUST be manipulated on the main render thread.
                _impl = impl_unique_ptr{ new impl{ session } };
            });
        });

    _ipc_srv->set_session_disconnect_callback(
        [this]([[maybe_unused]] std::shared_ptr<ipc_session> session) {
            _main_task_dispatcher->submit_task([this]() {
                CXLIB_DEBUG("Session disconnected, cleaning up GL renderer resources...");

                // NOTE: `impl` object MUST be manipulated on the main render thread.
                _impl.reset();
            });
        });

    _ipc_srv->start(Config::RENDERER_SERVER_NAME, 1);
    CXLIB_DEBUG("Created!");

    _process_inst_handle.reset(
        ::CreateEventA(nullptr, TRUE, TRUE, Config::RENDERER_PROCESS_INST_NAME),
        ::CloseHandle
    );

    _run_flag = true;
}

renderer_process::~renderer_process()
{
    CXLIB_DEBUG("Cleaning up GL renderer resources...");
}

void renderer_process::run()
{
    CXLIB_INFO("Initialization complete. entering render loop...");

    while (_run_flag)
    {
        _main_task_dispatcher->dispatch_pending_tasks();

        if (!_impl) {
            CXLIB_TRACE("waiting for connection...");
            std::this_thread::sleep_for(1000ms);
            continue;
        }

        _impl->poll();

        // log average of frame time and fps every 1 second
        static int frameCount = 0;
        ++frameCount;
        static auto lastTime = std::chrono::steady_clock::now();
        const auto currentTime = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime) >= 1s)
        {
            CXLIB_DEBUG("Render FPS: {}", frameCount);
            lastTime = currentTime;
            frameCount = 0;
        }

    } // while

    CXLIB_INFO("Render loop complete");
}

void renderer_process::stop()
{
    _run_flag = false;
}