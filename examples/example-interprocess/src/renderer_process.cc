#include "renderer_process.hh"

#include <cxlib/utils/logger.hh>

namespace
{
    // IPC 공유 및 KeyedMutex 동기화를 위한 별도의 동기화된 공유 텍스처 생성
    // NOTE: https://stackoverflow.com/a/70164789
    ComPtr<ID3D11Texture2D> create_dx11_shared_texture(
        const uint32_t width_pixels,
        const uint32_t height_pixels,
        ComPtr<ID3D11Device2> device)
    {
        ComPtr<ID3D11Texture2D> dx11_shared_texture;
        D3D11_TEXTURE2D_DESC dx11_shared_texture_desc = { 0, };
        dx11_shared_texture_desc.Width = width_pixels;
        dx11_shared_texture_desc.Height = height_pixels;
        dx11_shared_texture_desc.MipLevels = 1;
        dx11_shared_texture_desc.ArraySize = 1;
        dx11_shared_texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        dx11_shared_texture_desc.SampleDesc.Count = 1;
        dx11_shared_texture_desc.SampleDesc.Quality = 0;
        dx11_shared_texture_desc.Usage = D3D11_USAGE_DEFAULT;
        dx11_shared_texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        dx11_shared_texture_desc.CPUAccessFlags = 0;
        dx11_shared_texture_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX; // <- 핵심: 동기화된 공유를 위해서는 NtHandle + KeyedMutex 플래그 설정 필요
        ASSERT_HR(device->CreateTexture2D(&dx11_shared_texture_desc, NULL, &dx11_shared_texture));

        return dx11_shared_texture;
    }

} // namespace

class renderer_process::impl
{
private:
    static constexpr int32_t
        kInitialTextureWidth{ 640 },
        kInitialTextureHeight{ 640 };

public:
    impl(std::shared_ptr<ipc_session> session)
        : _ipc_session{ session }
    {
        CXLIB_TRACE("{}() ENTER", __func__);
        this->_init_d3d_device();
        this->_init_renderer();
        this->_init_ipc_session();
        CXLIB_TRACE("{}() LEAVE", __func__);
    }

    ~impl()
    {
        CXLIB_TRACE("{}() ENTER", __func__);
        _renderer->destroy_renderer();
        CXLIB_TRACE("{}() LEAVE", __func__);
    }

    bool poll()
    {
        std::unique_lock lk{ _ipc_lock };
        this->_update_scene();
        ID3D11Texture2D* const dx11_gl_interop_texture = _renderer->render();
        lk.unlock();

        // OpenGL 렌더링 결과를 KeyedMutex를 이용하여 적절히 동기화된 공유 텍스처로 복사 수행 (서버 프로세스로 공유)
        {
            // Key '0'을 사용하여 KeyedMutex 잠금 시도..
            HRESULT hr = _dxgi_keyed_mutex->AcquireSync(0/* Key */, 0/* Wait Timeout */);
            if (SUCCEEDED(hr))
            {
                _dx11_device_context2->CopyResource(
                    _dx11_synced_shared_texture.Get(),
                    dx11_gl_interop_texture
                );

                _dxgi_keyed_mutex->ReleaseSync(0/* Key */); // 복사를 완료했다면 KeyedMutex 잠금 해제
            }
            else
            {
                //CXLIB_DEBUG("Failed to acquire keyed mutex lock.");
            }
        }

        return true;
    }

private:
    // 1. D3D11 장치 초기화, 공유 텍스처 생성
    void _init_d3d_device()
    {
        // IMPORTANT NOTE: 반드시 CreateDXGIFactory2 함수를 사용해서 DXGI 1.2 버전 이상의 DXGI 팩토리(`IDXGIFactory`)를 생성해줘야 함.
        // (안 그러면 `D3D11 ERROR: ID3D11Device::CreateTexture2D: D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX is only available for devices created off of Dxgi1.1 factories or later.` 오류 발생)
        ComPtr<IDXGIFactory2> dxgiFactory2;
        ASSERT_HR(::CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory2)));

        ComPtr<IDXGIAdapter> dxgiAdapter0;
        ASSERT_HR(dxgiFactory2->EnumAdapters(0, &dxgiAdapter0)); // 기본 어댑터 사용
        _target_dxgi_adapter = dxgiAdapter0;

        DXGI_ADAPTER_DESC dxgiAdapterDesc;
        dxgiAdapter0->GetDesc(&dxgiAdapterDesc);
        _target_dxgi_adapter_luid = dxgiAdapterDesc.AdapterLuid;

        ComPtr<ID3D11Device> dx11Device0;
        ComPtr<ID3D11DeviceContext> dx11DeviceContext0;

        UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        ASSERT_HR(::D3D11CreateDevice(
            dxgiAdapter0.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            NULL,
            createDeviceFlags,
            NULL,
            0,
            D3D11_SDK_VERSION,
            &dx11Device0,
            NULL,
            &dx11DeviceContext0
        ));

        // ID3D11Device -> ID3D11Device2 (상위 버전 오브젝트)로 변환
        ASSERT_HR(dx11Device0.As(&_dx11_device2));

        // ID3D11DeviceContext -> ID3D11DeviceContext2 (상위 버전 오브젝트)로 변환
        ASSERT_HR(dx11DeviceContext0.As(&_dx11_device_context2));

        // IPC 공유 및 KeyedMutex 동기화를 위한 별도의 동기화된 공유 텍스처 생성
        _dx11_synced_shared_texture = create_dx11_shared_texture(kInitialTextureWidth, kInitialTextureHeight, _dx11_device2);

        // 동기화된 공유 텍스처에서 KeyedMutex 인터페이스 획득
        ASSERT_HR(_dx11_synced_shared_texture.As(&_dxgi_keyed_mutex));

        // 동기화된 공유 텍스처의 핸들을 공유
        ComPtr<IDXGIResource1> dxgiResource1;
        ASSERT_HR(_dx11_synced_shared_texture.As(&dxgiResource1));

        // CreateSharedHandle 함수로 Native Kernel Handle(NT Handle)을 생성 
        // (이후 서버측에서 OpenSharedResource1/OpenSharedResourceByName 함수를 통해 접근 예정)
        HANDLE newSharedHandle{ nullptr };
        ASSERT_HR(dxgiResource1->CreateSharedHandle(
            nullptr,
            DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
            nullptr, // NOTE: Name 지정 시 OpenSharedResourceByName 함수로 접근해야 함
            &newSharedHandle
        ));
        _shared_texture_handle.reset(newSharedHandle);
        CXLIB_DEBUG("Created resource handle: {}", _shared_texture_handle.get());
    }

    // 2. OpenGL 렌더러 및 Scene 초기화
    void _init_renderer()
    {
        const auto rsrc_dir_path = triengine::global_options::instance()->get_resource_directory();

        _renderer = std::make_unique<triengine::visualization::offscreen_renderer_dx>();
        _renderer->create_renderer(
            _dx11_device2,
            _dx11_device_context2,
            kInitialTextureWidth,
            kInitialTextureHeight
        );

        auto scn = _renderer->add_scene();
        scn->set_name("main");

        scn->get_render_config()->show_origin_xz_grid = true;
        scn->get_render_config()->light_opts.point_light.position = triengine::vec3_f32{ 0.0f, 1.5f, -1.5f };
        scn->get_render_config()->light_opts.point_light.ambientIntensity = 0.0f;
        scn->get_render_config()->light_opts.point_light.diffuseIntensity = 2.5f;
        scn->get_render_config()->light_opts.point_light.specularIntensity = 1.35f;

        scn->switch_camera_type(triengine::camera_type::arcball);
        scn->get_camera()->as<triengine::arcball_camera>()->get_options().damping_factor = 9.0f;

        auto mesh_axis_frame = triengine::geometry::triangle_mesh_object::create_coordinate_frame(0.5f);
        //mesh_axis_frame->paint_uniform_color(_get_next_color());
        //mesh_axis_frame->translate(Eigen::Vector3f{ 1.8f, 0.0f, -1.5f });
        scn->add_geometry(mesh_axis_frame);

        _skull_mesh = std::make_shared<triengine::geometry::triangle_mesh_object>();
        if (triengine::io::load_triangle_mesh_from_obj(
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

        _skull_mesh2 = std::make_shared<triengine::geometry::triangle_mesh_object>();
        if (triengine::io::load_triangle_mesh_from_obj(
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

    // 3. IPC 세션 초기화 및 이벤트 핸들러 설정
    void _init_ipc_session()
    {
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
                    case ipc_proto::MOUSEBTN_L:
                        _flag_l_mouse_pressed = body->action != ipc_proto::ACTION_RELEASE;
                        //CXLIB_TRACE("l mouse pressed : {}", _flag_m_mouse_pressed);
                        break;
                    case ipc_proto::MOUSEBTN_R:
                        _flag_r_mouse_pressed = body->action != ipc_proto::ACTION_RELEASE;
                        //CXLIB_TRACE("r mouse pressed : {}", _flag_m_mouse_pressed);
                        break;
                    case ipc_proto::MOUSEBTN_M:
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

        _ipc_session->set_request_callback(
            [this](
                [[maybe_unused]] const uint32_t req_pck_id, 
                const std::string_view req_pck_data, 
                std::vector<uint8_t>& rep_pck_data)
            {
                packet_view pck{ req_pck_data.data(), req_pck_data.size() };

                switch (pck.type()) {
                case ipc_proto::packet_type::frame_resize_request:
                {
                    //std::scoped_lock lk{ _ipc_lock };

                    auto body = pck.body<ipc_proto::packets::frame_resize_request_t>();
                    const triengine::vec2_i32 new_frame_size{ body->width, body->height };

                    if (new_frame_size.x() > 0 && 
                        new_frame_size.y() > 0 && 
                        _renderer->get_frame_size() != new_frame_size)
                    {
                        CXLIB_TRACE("----- frame resize start: {}x{}", new_frame_size.x(), new_frame_size.y());
                        this->_handle_frame_resize_request(new_frame_size.x(), new_frame_size.y());
                        CXLIB_TRACE("----- frame resize complete");
                    }

                    packet_builder<ipc_proto::packets::frame_resize_response_t> rep_pck{ ipc_proto::packet_type::frame_resize_response };
                    rep_pck.body()->shared_texture_handle = _shared_texture_handle.get();
                    rep_pck_data.assign(rep_pck.data(), rep_pck.data() + rep_pck.size());
                    break;
                }
                default:
                    CXLIB_WARN("Got unknown request packet");
                    break;
                } // switch
            });

        // 클라이언트 프로세스(뷰어 프로세스)로 데이터 전송
        packet_builder<ipc_proto::packets::shared_render_context_t> pck{ ipc_proto::packet_type::shared_render_context };
        pck.body()->renderer_process_id = ::GetCurrentProcessId();
        pck.body()->target_adapter_luid = _target_dxgi_adapter_luid;
        pck.body()->shared_texture_handle = _shared_texture_handle.get();
        pck.body()->shared_texture_width = kInitialTextureWidth;
        pck.body()->shared_texture_height = kInitialTextureHeight;
        CXLIB_ASSERT(std::errc{} == _ipc_session->send_notify(pck.data(), pck.size()));

        CXLIB_DEBUG("Sent data to viewer process. (adapter: {:x}-{:x}, resource handle: {})"
            , _target_dxgi_adapter_luid.HighPart
            , _target_dxgi_adapter_luid.LowPart
            , _shared_texture_handle.get()
        );
    }

    void _handle_frame_resize_request(
        const int32_t frame_width,
        const int32_t frame_height)
    {
        CXLIB_TRACE("frame resize start... (target size: {}x{})", frame_width, frame_height);

        _renderer->resize_frame(frame_width, frame_height);

        _dx11_synced_shared_texture.Reset();
        _dxgi_keyed_mutex.Reset();

        // IPC 공유 및 KeyedMutex 동기화를 위한 별도의 동기화된 공유 텍스처 재생성
        _dx11_synced_shared_texture = create_dx11_shared_texture(frame_width, frame_height, _dx11_device2);

        // 동기화된 공유 텍스처에서 KeyedMutex 인터페이스 획득
        ASSERT_HR(_dx11_synced_shared_texture.As(&_dxgi_keyed_mutex));

        // 동기화된 공유 텍스처의 핸들을 공유
        ComPtr<IDXGIResource1> dxgiResource1;
        ASSERT_HR(_dx11_synced_shared_texture.As(&dxgiResource1));

        // CreateSharedHandle 함수로 Native Kernel Handle(NT Handle)을 생성 
        // (이후 서버측에서 OpenSharedResource1/OpenSharedResourceByName 함수를 통해 접근 예정)
        HANDLE newSharedHandle{ nullptr };
        ASSERT_HR(dxgiResource1->CreateSharedHandle(
            nullptr,
            DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
            nullptr, // NOTE: Name 지정 시 OpenSharedResourceByName 함수로 접근해야 함
            &newSharedHandle
        ));
        _shared_texture_handle.reset(newSharedHandle);
        CXLIB_DEBUG("update resource handle: {}", _shared_texture_handle.get());

        CXLIB_TRACE("frame resize complete!");
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

    // D3D Resources
    ComPtr<IDXGIAdapter> _target_dxgi_adapter;
    ComPtr<ID3D11Device2> _dx11_device2;
    ComPtr<ID3D11DeviceContext2> _dx11_device_context2;
    ComPtr<ID3D11Texture2D> _dx11_synced_shared_texture; // KeyedMutex로 동기화되고 공유될 텍스처
    ComPtr<IDXGIKeyedMutex> _dxgi_keyed_mutex; // 동기화 공유 텍스처로부터 획득한 KeyedMutex

    LUID _target_dxgi_adapter_luid{};
    utils::unique_handle _shared_texture_handle;

    // GL Renderer
    std::unique_ptr<triengine::visualization::offscreen_renderer_dx> _renderer;
    std::shared_ptr<triengine::scene> _scene;
    std::shared_ptr<triengine::geometry::triangle_mesh_object> _skull_mesh;
    std::shared_ptr<triengine::geometry::triangle_mesh_object> _skull_mesh2;
    
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
{
    CXLIB_INFO("GL renderer process spawned (PID: {})"
        , ::GetCurrentProcessId()
    );

    CXLIB_DEBUG("Creating IPC server...");
    _ipc_srv = std::make_shared<ipc_server>();

    _ipc_srv->set_session_connected_callback(
        [this](std::shared_ptr<ipc_session> session) {
            this->_post_task([this, session]() {
                CXLIB_DEBUG("Session connected, initializing GL renderer...");

                // NOTE: `impl` object MUST be manipulated on the main render thread.
                _impl = impl_unique_ptr{ new impl{ session } };
            });
        });

    _ipc_srv->set_session_disconnected_callback(
        [this]([[maybe_unused]] std::shared_ptr<ipc_session> session) {
            this->_post_task([this]() {
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
        this->_process_pending_tasks();

        if (!_impl) {
            CXLIB_TRACE("waiting for connection...");
            std::this_thread::sleep_for(1000ms);
            continue;
        }

        if (!_impl->poll()) {
            CXLIB_ERROR("poll failed");
            break;
        }

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

void renderer_process::_post_task(std::function<void()> task)
{
    std::scoped_lock lk{ _task_q_lock };
    _task_q.emplace_back(std::move(task));
}

void renderer_process::_process_pending_tasks()
{
    std::scoped_lock lk{ _task_q_lock };
    while (!_task_q.empty()) {
        auto task = std::move(_task_q.front());
        task(); // Execute the task
        _task_q.pop_front();
    }
}
