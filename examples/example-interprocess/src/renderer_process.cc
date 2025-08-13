#include "renderer_process.hh"

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

        std::unique_lock lk{ _ipc_lock };
        if (!_renderer) { return; }
        this->_update_scene();
        ID3D11Texture2D* const dx11_gl_interop_texture = _renderer->render();
        lk.unlock();

        // OpenGL 렌더링 결과(GL interop texture)를 공유 텍스처로 복사(서버 프로세스로 공유)
        // 이 때, KeyedMutex를 이용하여 렌더링 타이밍을 적절히 맞춘다.
        if (const HRESULT sync_hr = _dxgi_keyed_mutex->AcquireSync( // Key '0'을 사용하여 KeyedMutex 잠금 시도
                0/* Key */,
                0/* Wait Timeout */
            ); SUCCEEDED(sync_hr))
        {
            // 잠금 획득에 성공했다면, GL interop 텍스처를 공유 텍스처로 복사
            _dx11_device_context2->CopyResource(
                _dx11_shared_texture.Get(),
                dx11_gl_interop_texture
            );

            // 작업을 완료했으면 KeyedMutex 잠금 해제
            _dxgi_keyed_mutex->ReleaseSync(0/* Key */);
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
                    CXLIB_TRACE("----- recv init request: {}x{}, format: {}"
                        , body->frame_width
                        , body->frame_height
                        , static_cast<int>(body->frame_format)
                    );

                    // init 작업은 ipc 스레드에서 직접 수행하지 않고, 메인 렌더 스레드에 위임
                    auto init_future = _main_task_dispatcher->submit_task(
                        [this, init_req = *pck.body<ipc_proto::packets::init_request_t>()]() -> bool
                        {
                            CXLIB_INFO("Start initialization...");

                            this->_create_renderer(
                                init_req.frame_width,
                                init_req.frame_height,
                                init_req.frame_format
                            );

                            this->_create_renderer_scene();

                            CXLIB_DEBUG("init complete. send response start (adapter: {:x}-{:x}, resource handle: {})"
                                , _target_dxgi_adapter_desc.AdapterLuid.HighPart
                                , _target_dxgi_adapter_desc.AdapterLuid.LowPart
                                , _dx11_shared_texture_handle.get()
                            );

                            return true;
                        });

                    CXLIB_TRACE("IPC thread waiting for init completion...");
                    try {
                        const bool init_res = init_future.get();
                        if (!init_res) {
                            CXLIB_ERROR("Failed to initialize renderer.");
                            return;
                        }
                    }
                    catch (const std::exception& e) {
                        CXLIB_ERROR("Initialization task failed: {}", e.what());
                        return;
                    }

                    CXLIB_TRACE("Sending init response...");
                    packet_builder<ipc_proto::packets::init_response_t> rep_pck{ ipc_proto::packet_type::init_response };
                    rep_pck.body()->renderer_process_id = ::GetCurrentProcessId();
                    rep_pck.body()->target_adapter_luid = _target_dxgi_adapter_desc.AdapterLuid;
                    rep_pck.body()->shared_texture_handle = _dx11_shared_texture_handle.get();
                    rep_pck_data.assign(rep_pck.data(), rep_pck.data() + rep_pck.size());
                    break;
                }
                case ipc_proto::packet_type::frame_resize_request:
                {
                    auto body = pck.body<ipc_proto::packets::frame_resize_request_t>();
                    const triengine::vec2_i32 new_frame_size{ body->width, body->height };

                    // resize 작업은 ipc 스레드에서 직접 수행하지 않고, 메인 렌더 스레드에 위임
                    auto resize_future = _main_task_dispatcher->submit_task(
                        [this, new_frame_size]() -> bool
                        {
                            if (new_frame_size.x() > 0 &&
                                new_frame_size.y() > 0 &&
                                _renderer->get_frame_size() != new_frame_size)
                            {
                                CXLIB_TRACE("----- frame resize start: {}x{}", new_frame_size.x(), new_frame_size.y());
                                this->_handle_frame_resize_request(new_frame_size.x(), new_frame_size.y());
                                CXLIB_TRACE("----- frame resize complete");
                            }

                            return true;
                        });

                    CXLIB_TRACE("IPC thread waiting for resize completion...");
                    const bool resize_res = resize_future.get();
                    if (!resize_res) {
                        CXLIB_ERROR("Failed to resize frame");
                        return;
                    }

                    CXLIB_TRACE("Sending resize response...");
                    packet_builder<ipc_proto::packets::frame_resize_response_t> rep_pck{ ipc_proto::packet_type::frame_resize_response };
                    rep_pck.body()->shared_texture_handle = _dx11_shared_texture_handle.get();
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
        const int32_t frame_height,
        const DXGI_FORMAT frame_format)
    {
        CXLIB_TRACE("initialize renderer... (frame size: {}x{}, format: {})"
            , frame_width
            , frame_height
            , static_cast<int>(frame_format)
        );

        _renderer = std::make_unique<triengine::visualization::offscreen_renderer_dx>();
        _renderer->create_renderer(
            frame_width,
            frame_height,
            frame_format
        );

        _dx11_device2 = _renderer->get_dx11_device();
        _dx11_device_context2 = _renderer->get_dx11_device_context();
        _target_dxgi_adapter = _renderer->get_target_dxgi_adapter();
        _target_dxgi_adapter->GetDesc(&_target_dxgi_adapter_desc);

        // 타 프로세스로 공유할 공유 텍스처 생성 (SHARED_HANDLE + SHARED_KEYEDMUTEX)
        // (이후, KeyedMutex를 통해 렌더 타이밍 동기화 수행)
        // 
        // NOTE: You can't modify the texture surface owned by the DXGI swapchain to add the share flags like you tried to do above.
        //       Your best bet is going to require copying the backbuffer to a sharable texture.
        //       (Ref: https://stackoverflow.com/a/70164789)
        D3D11_TEXTURE2D_DESC dx11_shared_texture_desc{};
        dx11_shared_texture_desc.Width = static_cast<UINT>(frame_width);
        dx11_shared_texture_desc.Height = static_cast<UINT>(frame_height);
        dx11_shared_texture_desc.MipLevels = 1;
        dx11_shared_texture_desc.ArraySize = 1;
        dx11_shared_texture_desc.Format = frame_format;
        dx11_shared_texture_desc.SampleDesc.Count = 1;
        dx11_shared_texture_desc.SampleDesc.Quality = 0;
        dx11_shared_texture_desc.Usage = D3D11_USAGE_DEFAULT;
        dx11_shared_texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        dx11_shared_texture_desc.CPUAccessFlags = 0;
        dx11_shared_texture_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX; // <- 핵심: NtHandle + KeyedMutex 플래그 설정 필요
        ASSERT_HR(_dx11_device2->CreateTexture2D(
            &dx11_shared_texture_desc,
            nullptr,
            &_dx11_shared_texture
        ));

        // 생성한 공유 텍스처로부터 KeyedMutex 인터페이스 획득
        ASSERT_HR(_dx11_shared_texture.As(&_dxgi_keyed_mutex));

        // 공유 텍스처의 핸들(Shared NT Handle) 획득
        // IDXGIResource1 인터페이스의 CreateSharedHandle 메서드로 핸들을 생성한다.
        // (이후 Viewer 측에서 OpenSharedResource1/OpenSharedResourceByName 함수를 통해 접근)
        {
            HANDLE shared_texture_handle{ nullptr };
            ComPtr<IDXGIResource1> dxgiResource1;
            ASSERT_HR(_dx11_shared_texture.As(&dxgiResource1));
            ASSERT_HR(dxgiResource1->CreateSharedHandle(
                nullptr,
                DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                nullptr, // NOTE: Name 지정 시 OpenSharedResourceByName 함수로 접근해야 함
                &shared_texture_handle
            ));
            _dx11_shared_texture_handle.reset(shared_texture_handle);
        }
        CXLIB_DEBUG("Created resource handle: {}", _dx11_shared_texture_handle.get());
    }

    void _create_renderer_scene()
    {
        CXLIB_TRACE("initialize renderer scene...");

        const auto rsrc_dir_path = triengine::global_options::instance()->get_resource_directory();

        auto scn = _renderer->add_scene();
        scn->set_name("main");

        scn->get_render_config()->show_origin_xz_grid = true;
        scn->get_render_config()->light_opts.point_light.position = triengine::vec3_f32{ 0.0f, 1.5f, -1.5f };
        scn->get_render_config()->light_opts.point_light.ambientIntensity = 0.0f;
        scn->get_render_config()->light_opts.point_light.diffuseIntensity = 2.5f;
        scn->get_render_config()->light_opts.point_light.specularIntensity = 1.35f;

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
    }

    void _handle_frame_resize_request(
        const int32_t frame_width,
        const int32_t frame_height)
    {
        CXLIB_TRACE("frame resize start... (target size: {}x{})", frame_width, frame_height);

        // 렌더러의 프레임 사이즈 업데이트  
        _renderer->resize_frame(frame_width, frame_height);

        // 공유 텍스처, 공유 텍스처 핸들, KeyedMutex 재생성
        ComPtr<ID3D11Texture2D> new_dx11_shared_texture;
        ComPtr<IDXGIKeyedMutex> new_dxgi_keyed_mutex;
        utils::unique_handle new_dx11_shared_texture_handle;
        
        D3D11_TEXTURE2D_DESC dx11_shared_texture_desc{};
        _dx11_shared_texture->GetDesc(&dx11_shared_texture_desc); // 기존 공유 텍스처의 속성 복사
        dx11_shared_texture_desc.Width = static_cast<UINT>(frame_width);
        dx11_shared_texture_desc.Height = static_cast<UINT>(frame_height);
        ASSERT_HR(_dx11_device2->CreateTexture2D(
            &dx11_shared_texture_desc,
            nullptr,
            &new_dx11_shared_texture
        ));

        // 생성한 공유 텍스처로부터 KeyedMutex 인터페이스 획득
        ASSERT_HR(new_dx11_shared_texture.As(&new_dxgi_keyed_mutex));

        // 공유 텍스처의 핸들(Shared NT Handle) 획득
        // IDXGIResource1 인터페이스의 CreateSharedHandle 메서드로 핸들을 생성한다.
        // (이후 Viewer 측에서 OpenSharedResource1/OpenSharedResourceByName 함수를 통해 접근)
        {
            HANDLE shared_texture_handle{ nullptr };
            ComPtr<IDXGIResource1> dxgiResource1;
            ASSERT_HR(new_dx11_shared_texture.As(&dxgiResource1));
            ASSERT_HR(dxgiResource1->CreateSharedHandle(
                nullptr,
                DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                nullptr, // NOTE: Name 지정 시 OpenSharedResourceByName 함수로 접근해야 함
                &shared_texture_handle
            ));
            new_dx11_shared_texture_handle.reset(shared_texture_handle);
        }

        // 모든 리사이즈가 성공적으로 완료됐을 경우, 최종적으로 상태 업데이트 (스왑)
        _dx11_shared_texture.Swap(new_dx11_shared_texture);
        _dxgi_keyed_mutex.Swap(new_dxgi_keyed_mutex);
        _dx11_shared_texture_handle.swap(new_dx11_shared_texture_handle);

        CXLIB_TRACE("frame resize complete! (update resource handle: {})"
            , _dx11_shared_texture_handle.get()
        );
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
    std::shared_ptr<ipc_session> _ipc_session;
    _CXLIB utils::spin_lock _ipc_lock;
    std::shared_ptr<task_dispatcher> _main_task_dispatcher;

    // D3D Resources
    ComPtr<IDXGIAdapter> _target_dxgi_adapter;
    DXGI_ADAPTER_DESC _target_dxgi_adapter_desc{};

    ComPtr<ID3D11Device2> _dx11_device2;
    ComPtr<ID3D11DeviceContext2> _dx11_device_context2;

    ComPtr<ID3D11Texture2D> _dx11_shared_texture; // 타 프로세스로 공유할 "공유 텍스처" (KeyedMutex로 렌더 타이밍 동기화 수행)
    ComPtr<IDXGIKeyedMutex> _dxgi_keyed_mutex; // 공유 텍스처로부터 획득한 KeyedMutex
    utils::unique_handle _dx11_shared_texture_handle;

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