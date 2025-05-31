#pragma once
#include "basic_demo_app.hh"

#include <triengine/math/math3d.hh>
#include <triengine/io/file_obj_loader.hh>
#include <triengine/io/file_ply_loader.hh>
#include <triengine/io/file_bvh_loader.hh>

#include <cxlib/utils/debug_panic.hh>
#include <cxlib/utils/debug_assert.hh>
#include <cxlib/utils/scrolling_buffer.hh>

#include <magic_enum/magic_enum.hpp>

#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>

namespace gui
{
    namespace {

        /**
         * @brief Manages adding patterned (e.g., wave) or random noise to an pointcloud in a background thread.
         *
         * This class takes an original pointcloud and parameters for noise generation.
         * It runs a worker thread that continuously generates a new pointcloud with noise
         * applied based on the configured pattern (like an expanding wave) and time.
         * The main thread can safely query if new data is ready and retrieve it without blocking.
         */
        class pcd_noise_generator {
        public:
            struct generate_options_t {
                Eigen::Vector3f wave_origin{ Eigen::Vector3f::Zero() }; // Origin point for the wave pattern
                float wave_amplitude{ 0.0f }; // Maximum displacement amplitude of the wave
                float wave_length{ 0.25f }; // Wave length
                float wave_speed{ 0.05f }; // Wave speed
                float noise_intensity{ 0.015f }; // Noise intensity

                generate_options_t() = default;
            };

        private:
            generate_options_t _opts;
            mutable std::mutex _opts_mtx;

            std::shared_ptr<triengine::geometry::pcd_object> _pcd_original; // The unmodified base point cloud
            mutable std::shared_ptr<triengine::geometry::pcd_object> _pcd_noisy; // The latest generated noisy point cloud (shared)
            
            std::thread _worker_thread;
            std::atomic_bool _is_running{ false };
            mutable std::atomic_bool _is_data_ready{ false };
            mutable std::mutex _data_mtx;
            
        public:
            pcd_noise_generator(
                const triengine::geometry::pcd_object& original_pcd,
                generate_options_t opts = generate_options_t{})
                : _pcd_original{ original_pcd.clone() }
                , _opts{ opts }
            {
                if (!_pcd_original || _pcd_original->points.empty()) {
                    CXLIB_PANIC("Cannot start with empty or null original pointcloud");
                    return;
                }

                _pcd_noisy = _pcd_original->clone();

                _is_running = true;
                _is_data_ready = false;
                _worker_thread = std::thread{ &pcd_noise_generator::worker_thread_proc, this };
            }
            
            ~pcd_noise_generator()
            {
                if (_is_running) {
                    _is_running = false;
                }

                if (_worker_thread.joinable()) {
                    _worker_thread.join();
                }
            }

            generate_options_t get_options() const {
                std::scoped_lock lk{ _opts_mtx };
                return _opts;
            }

            void set_options(const generate_options_t& opts) {
                std::scoped_lock lk{ _opts_mtx };
                _opts = opts;
            }

            // Checks if a new noisy point cloud is ready for retrieval
            bool is_data_ready() const {
                return _is_data_ready;
            }
            
            // Safely retrieves the latest noisy point cloud if ready.
            // Resets the ready flag after retrieval. Returns nullptr if not ready.
            std::shared_ptr<triengine::geometry::pcd_object> get_noisy_pointcloud()
            {
                std::scoped_lock lk{ _data_mtx };
                if (_is_data_ready) {
                    _is_data_ready = false;
                    // Return a copy to minimize lock time
                    return _pcd_noisy->clone();
                } else {
                    return nullptr;
                }
            }
            
        private:
            void worker_thread_proc() const
            {
                using namespace std::chrono_literals;
                
                std::random_device rd;
                std::mt19937 gen{ rd() };
                
                float curr_time{ 0.0f };       // Internal time counter for animation
                constexpr float time_step = 0.1f; // Time increment per loop
                
                while (_is_running)
                {
                    curr_time += time_step;

                    generate_options_t curr_opts; {
                        std::scoped_lock lk{ _opts_mtx };
                        curr_opts = _opts;
                    }

                    const float 
                        wave_frequency_k{ 2.0f * triengine::math::pi<float>() / curr_opts.wave_length }, // Spatial frequency (k = 2*pi / wavelength)
                        wave_speed_omega{ wave_frequency_k * curr_opts.wave_speed }; // Temporal frequency (omega = k * speed)

                    // Create a working copy from the original point cloud
                    auto pcd_temp = _pcd_original->clone();
                    
                    // Apply wave noise pattern to each point
                    for (size_t i = 0; i < pcd_temp->points.size(); ++i)
                    {
                        Eigen::Vector3f& point = pcd_temp->points[i];
                        const Eigen::Vector3f& original_point = _pcd_original->points[i];
                        
                        const float 
                            distance = (original_point - curr_opts.wave_origin).norm(),
                            offset = curr_opts.wave_amplitude * std::sin(wave_frequency_k * distance - wave_speed_omega * curr_time);
                        
                        Eigen::Vector3f direction;
                        if (distance > 1e-9f) { // Avoid division by zero or near-zero
                            direction = (original_point - curr_opts.wave_origin).normalized();
                        } else {
                            direction = Eigen::Vector3f::Zero();
                        }
                        
                        // Apply offset from the original position
                        point = original_point + direction * offset;
                        
                        // Add random noise
                        std::normal_distribution<float> dist{ 0.0f/* mean value */, curr_opts.noise_intensity/* stddev value */ };
                        point.x() += dist(gen);
                        point.y() += dist(gen);
                        point.z() += dist(gen);
                    }
                    
                    // Update shared data and set ready flag under lock
                    {
                        std::scoped_lock lk{ _data_mtx };
                        _pcd_noisy = pcd_temp;
                        _is_data_ready = true;
                    }
                    
                    // Wait briefly to control update rate
                    std::this_thread::sleep_for(10ms);
                } // while
            }
            
        }; // class

    } // namespace

    class scene_control_window
        : public triengine::gui::iwindow
    {
    private:
        triengine::visualization::visualizer* const _vis;

        std::list<std::shared_ptr<scene_wrapper>> _scn_list;
        std::list<std::shared_ptr<scene_wrapper>>::iterator _curr_scn_it{ _scn_list.end() };
        std::unordered_map<
            triengine::scene_id_t,
            std::list<std::shared_ptr<scene_wrapper>>::iterator
        > _scn_id_map;
        
    public:
        scene_control_window(triengine::visualization::visualizer* vis)
            : _vis{ vis }
        {}

        virtual ~scene_control_window() = default;

        const char* get_window_name() const override {
            return "Scene Controller";
        }

        ImVec2 get_initial_window_size() const override {
            return ImVec2{ 430.0f, 470.0f };
        }

        void render(
            [[maybe_unused]] const triengine::gui::window_render_context& render_ctx) override
        {
            if (_curr_scn_it == _scn_list.end()) {
                return;
            }

            if (_vis->get_current_scene()->get_id() != (*_curr_scn_it)->get_scene_id()) {
                return;
            }

            ImGui::Text("Current Scene: %s", (*_curr_scn_it)->get_scene()->get_name().c_str());
            if (ImGui::Button("<")) { this->switch_to_prev_scene(); }
            ImGui::SameLine();
            if (ImGui::Button(">")) { this->switch_to_next_scene(); }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            (*_curr_scn_it)->render_gui(render_ctx);
        }

        std::shared_ptr<const scene_wrapper> get_current_scene() const
        {
            return (_curr_scn_it != _scn_list.end())
                ? *_curr_scn_it
                : nullptr;
        }
        
        std::shared_ptr<scene_wrapper> get_current_scene()
        {
            return (_curr_scn_it != _scn_list.end())
                ? *_curr_scn_it
                : nullptr;
        }

        void add_scene(const std::shared_ptr<scene_wrapper>& new_scn)
        {
            const bool is_first{ _scn_list.empty() };

            _scn_list.push_back(new_scn);
            _scn_id_map[new_scn->get_scene_id()] = std::prev(_scn_list.end());

            if (is_first) {
                _curr_scn_it = std::prev(_scn_list.end());
                _vis->switch_scene((*_curr_scn_it)->get_scene_id());
            }
        }

        void switch_to_prev_scene()
        {
            if (_curr_scn_it != _scn_list.end()) {
                _curr_scn_it = std::prev((_curr_scn_it != _scn_list.begin())
                    ? _curr_scn_it
                    : _scn_list.end()
                );
                _vis->switch_scene((*_curr_scn_it)->get_scene_id());
            }
        }

        void switch_to_next_scene()
        {
            if (_curr_scn_it != _scn_list.end()) {
                const auto next_it = std::next(_curr_scn_it);
                _curr_scn_it = (next_it != _scn_list.end())
                    ? next_it
                    : _scn_list.begin();
                _vis->switch_scene((*_curr_scn_it)->get_scene_id());
            }
        }

        void update_animation()
        {
            if (_curr_scn_it != _scn_list.end()) {
                (*_curr_scn_it)->update_animation();
            }
        }

    }; // class

    basic_demo_app::basic_demo_app()
    {}

    basic_demo_app::~basic_demo_app()
    {}

    void basic_demo_app::create()
    {
        CXLIB_TRACE("{}() ENTER", __func__);

        _vis = std::make_unique<triengine::visualization::visualizer>();
        _vis->create_window("Triengine Demo"
            " (Build: " __DATE__ ", " __TIME__
#if defined (_DEBUG)
            " DBG"
#else  // ^^^ _DEBUG ^^^ / vvv !_DEBUG vvv
            " REL"
#endif // ^^^ !_DEBUG ^^^
            ")"
        );

        _vis->set_key_callback(
            [this](
                [[maybe_unused]] triengine::visualization::visualizer& vis,
                [[maybe_unused]] const int key,
                [[maybe_unused]] const int scancode,
                [[maybe_unused]] const int action,
                [[maybe_unused]] const int mods,
                [[maybe_unused]] bool& handled)
            {
                if (action != GLFW_RELEASE)
                {
                    CXLIB_TRACE("key: {}", key);

                    switch (key) {
                    case GLFW_KEY_ESCAPE:
                        break;
                    case GLFW_KEY_F12:
                        _vis->enable_main_menu(!_vis->is_main_menu_enabled());
                        break;
                    case GLFW_KEY_SPACE:
                        _flag_animation = !_flag_animation;
                        break;
                    case GLFW_KEY_A:
                        _vis->get_current_scene()->get_render_config()->light_opts.point_light.position.x() -= 0.05f; // left
                        break;
                    case GLFW_KEY_D:
                        _vis->get_current_scene()->get_render_config()->light_opts.point_light.position.x() += 0.05f; // right
                        break;
                    case GLFW_KEY_W:
                        _vis->get_current_scene()->get_render_config()->light_opts.point_light.position.z() += 0.05f; // forward
                        break;
                    case GLFW_KEY_S:
                        _vis->get_current_scene()->get_render_config()->light_opts.point_light.position.z() -= 0.05f; // backward
                        break;
                    case GLFW_KEY_UP:
                        _vis->get_current_scene()->get_render_config()->light_opts.point_light.position.y() -= 0.05f; // up
                        break;
                    case GLFW_KEY_DOWN:
                        _vis->get_current_scene()->get_render_config()->light_opts.point_light.position.y() += 0.05f; // down
                        break;
                    case GLFW_KEY_LEFT:
                        _scene_ctrl_window->switch_to_prev_scene();
                        break;
                    case GLFW_KEY_RIGHT:
                        _scene_ctrl_window->switch_to_next_scene();
                        break;
                    }
                }
            });

        _log_window = std::make_shared<triengine::gui::log_window>();
        _log_window->set_visible(false);
        _vis->add_gui_window(_log_window);

        _render_stats_window = std::make_shared<triengine::gui::render_stats_window>();
        _render_stats_window->set_visible(false);
        _vis->add_gui_window(_render_stats_window);

        _scene_ctrl_window = std::make_shared<scene_control_window>(_vis.get());
        _scene_ctrl_window->set_visible(true);
        _vis->add_gui_window(_scene_ctrl_window);

        this->_add_main_scene();
        this->_add_engine_scene();
        this->_add_pointcloud_scene();
        this->_add_bvh_scene();
    }

    void basic_demo_app::destroy()
    {
        CXLIB_TRACE("{}() ENTER", __func__);

        _log_window.reset();
        _render_stats_window.reset();
        
        _vis->destroy_window();
        _vis.reset();

        CXLIB_TRACE("{}() LEAVE", __func__);
    }

    void basic_demo_app::run()
    {
        CXLIB_TRACE("{}() ENTER", __func__);

        CXLIB_DEBUG("polling start..");
        while (_vis->update_window())
        {
            // animate
            if (_flag_animation) {
                _scene_ctrl_window->update_animation();
            }

            _vis->render();
        } // while

        CXLIB_TRACE("{}() LEAVE", __func__);
    }

    void basic_demo_app::_add_main_scene()
    {
        using namespace triengine::geometry;

        class main_scene
            : public scene_wrapper
        {
            std::shared_ptr<triangle_mesh_object> _skull_mesh;
            std::shared_ptr<triangle_mesh_object> _skull_mesh2;

        public:
            main_scene(
                triengine::visualization::visualizer& vis)
                : scene_wrapper(vis.add_scene())
            {
                const auto rsrc_dir_path = triengine::global_options::instance()->get_resource_directory();
                auto scn = this->get_scene();
                scn->set_name("main");

                scn->get_render_config()->show_origin_xz_grid = true;
                scn->get_render_config()->light_opts.point_light.position = triengine::vec3_f32{ 0.0f, -1.5f, -1.5f };
                scn->get_render_config()->light_opts.point_light.ambientIntensity = 0.0f;
                scn->get_render_config()->light_opts.point_light.diffuseIntensity = 2.5f;
                scn->get_render_config()->light_opts.point_light.specularIntensity = 1.35f;

                scn->get_camera()->set_mirror_mode(false);
        
                auto mesh_axis_frame = triangle_mesh_object::create_coordinate_frame(0.5f);
                //mesh_axis_frame->paint_uniform_color(_get_next_color());
                //mesh_axis_frame->translate(Eigen::Vector3f{ 1.8f, 0.0f, -1.5f });
                scn->add_geometry(mesh_axis_frame);

                _skull_mesh = std::make_shared<triangle_mesh_object>();
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
                        * Eigen::AngleAxisf(triengine::math::deg2rad(90.0f), Eigen::Vector3f::UnitX());
                    
                    _skull_mesh->rotate(R, true);
                    _skull_mesh->translate(triengine::vec3_f32(0.0f, -0.5f, 0.0f), true);
                    scn->add_geometry(_skull_mesh);
                }

                _skull_mesh2 = std::make_shared<triangle_mesh_object>();
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
                        * Eigen::AngleAxisf(triengine::math::deg2rad(90.0f), Eigen::Vector3f::UnitX());
                    _skull_mesh2->rotate(R, true);
                    _skull_mesh2->translate(triengine::vec3_f32(0.0f, -0.6f, 0.0f), true);
        
                    scn->add_geometry(_skull_mesh2);
                }
            }

            void update_animation() override
            {
                constexpr float rotSpeed = triengine::math::pi<float>() / 8.0f;
                const float dT = static_cast<float>(::glfwGetTime());
    
                Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
                //R = Eigen::AngleAxisf(rotSpeed * dT * 0.1f, Eigen::Vector3f::UnitZ()) *
                //    Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitY()) *
                //    Eigen::AngleAxisf(rotSpeed * dT * 0.5f, Eigen::Vector3f::UnitX());
    
                R = Eigen::AngleAxisf(triengine::math::deg2rad(90.0f), Eigen::Vector3f::UnitX()) *
                    Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitZ());
    
                if (_skull_mesh) {
                    _skull_mesh->rotate(R);
                }
            }
            
            void render_gui([[maybe_unused]] const triengine::gui::window_render_context& render_ctx) override
            {
                ImGui::Text("main scene gui!");
            }
        };

        _scene_ctrl_window->add_scene(
            std::make_shared<main_scene>(*_vis)
        );
    }

    void basic_demo_app::_add_engine_scene()
    {
        using namespace triengine::geometry;

        class engine_scene
            : public scene_wrapper
        {
            std::shared_ptr<triangle_mesh_object> _engine_mesh;

        public:
            engine_scene(
                triengine::visualization::visualizer& vis)
                : scene_wrapper(vis.add_scene())
            {
                const auto rsrc_dir_path = triengine::global_options::instance()->get_resource_directory();
                auto scn = this->get_scene();
                scn->set_name("engine");

                scn->get_render_config()->show_origin_xz_grid = false;
                scn->get_render_config()->bg_color = triengine::color4_f32{ 1.0f, 1.0f, 1.0f, 1.0f };
                scn->get_render_config()->light_opts.point_light.enabled = false;
                scn->get_render_config()->light_opts.dir_light.ambientIntensity = 0.1f;
                scn->get_render_config()->light_opts.dir_light.diffuseIntensity = 0.45f;
                scn->get_render_config()->light_opts.dir_light.specularIntensity = 3.0f;
        
                scn->get_camera()->set_position(triengine::vec3_f32{ 0.0f, -2.2326f, -11.1618f });
                scn->get_camera()->set_mirror_mode(false);
        
                _engine_mesh = std::make_shared<triangle_mesh_object>();

                if (triengine::io::load_triangle_mesh_from_obj(
                        rsrc_dir_path / "objects/car_engine/car_engine.obj",
                        false,
                        *_engine_mesh
                    ))
                {
                    //_engine_mesh->compute_vertex_normals();
                    _engine_mesh->set_model(
                        triengine::math::scale(_engine_mesh->get_model(), triengine::vec3_f32{ 0.0125f, 0.0125f, 0.0125f })
                    );
        
                    _engine_mesh->apply_model_in_place();
                    _engine_mesh->translate(triengine::vec3_f32(0.0f, -0.5f, 0.0f), true);
        
                    scn->add_geometry(_engine_mesh);
                }
            }

            void update_animation() override
            {
                constexpr float rotSpeed = triengine::math::pi<float>() / 8.0f;
                const float dT = static_cast<float>(::glfwGetTime());
    
                Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
                //R = Eigen::AngleAxisf(rotSpeed * dT * 0.1f, Eigen::Vector3f::UnitZ()) *
                //    Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitY()) *
                //    Eigen::AngleAxisf(rotSpeed * dT * 0.5f, Eigen::Vector3f::UnitX());
    
                R = Eigen::AngleAxisf(triengine::math::deg2rad(90.0f), Eigen::Vector3f::UnitX()) *
                    Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitZ());
    
                if (_engine_mesh) {
                    _engine_mesh->rotate(R);
                }
            }
            
            void render_gui(
                [[maybe_unused]] const triengine::gui::window_render_context& render_ctx) override
            {
                ImGui::Text("engine scene gui!");
            }

        }; // class

        _scene_ctrl_window->add_scene(
            std::make_shared<engine_scene>(*_vis)
        );
    }

    void basic_demo_app::_add_pointcloud_scene()
    {
        using namespace triengine::geometry;

        class pointcloud_scene
            : public scene_wrapper
        {
            std::shared_ptr<pcd_object> _pcd;
            std::unique_ptr<pcd_noise_generator> _pcd_gen;
            bool _inplace_update{ true };

        public:
            pointcloud_scene(
                triengine::visualization::visualizer& vis)
                : scene_wrapper(vis.add_scene())
            {
                const auto rsrc_dir_path = triengine::global_options::instance()->get_resource_directory();
                auto scn = this->get_scene();
                scn->set_name("pointcloud");

                scn->get_render_config()->pcd_point_size = 5.0f;
                scn->get_render_config()->show_origin_xz_grid = false;
                scn->get_render_config()->light_opts.dir_light.ambientIntensity = 0.1f;
                scn->get_render_config()->light_opts.dir_light.diffuseIntensity = 1.5f;
                scn->get_render_config()->light_opts.dir_light.specularIntensity = 2.5f;
                scn->get_render_config()->light_opts.point_light.position = triengine::vec3_f32{ 0.0f, -1.5f, -1.5f };
        
                auto mesh_axis_frame = triangle_mesh_object::create_coordinate_frame(0.5f);
                scn->add_geometry(mesh_axis_frame);
                
                _pcd = std::make_shared<pcd_object>();
                if (triengine::io::load_pointcloud_from_ply(
                    rsrc_dir_path / "pointcloud/sample.ply",
                        *_pcd
                    ))
                {
                    scn->add_geometry(_pcd);
                }

                pcd_noise_generator::generate_options_t gen_opts{};
                gen_opts.wave_origin = _pcd->get_center();
                _pcd_gen = std::make_unique<pcd_noise_generator>(*_pcd, gen_opts);
            }

            void update_animation() override
            {
                if (auto new_pcd = _pcd_gen->get_noisy_pointcloud(); 
                    new_pcd)
                {
                    if (_inplace_update)
                    {
                        _pcd->points = new_pcd->points;
                        if (new_pcd->has_colors()) { _pcd->colors = new_pcd->colors; }
                        if (new_pcd->has_normals()) { _pcd->normals = new_pcd->normals; }
                    }
                    else
                    {
                        this->get_scene()->remove_geometry(_pcd);
                        _pcd = new_pcd;
                        this->get_scene()->add_geometry(_pcd);
                    }

                    _pcd->mark_dirty();
                }
            }
            
            void render_gui(
                [[maybe_unused]] const triengine::gui::window_render_context& render_ctx) override
            {
                {
                    auto gen_opts = _pcd_gen->get_options();
                    bool opts_updated{ false };

                    if (ImGui::DragFloat3("Wave Origin", gen_opts.wave_origin.data(), 0.01f)) {
                        opts_updated = true;
                    }

                    if (ImGui::DragFloat("Wave Amplitude", &gen_opts.wave_amplitude, 0.001f, 0.0f, 0.1f, "%.3f")) {
                        opts_updated = true;
                    }

                    if (ImGui::DragFloat("Wave Length", &gen_opts.wave_length, 0.001f, 0.05f, 2.0f, "%.3f")) {
                        opts_updated = true;
                    }

                    if (ImGui::DragFloat("Wave Speed", &gen_opts.wave_speed, 0.001f, 0.0f, 5.0f, "%.3f")) {
                        opts_updated = true;
                    }

                    if (ImGui::DragFloat("Noise Intensity", &gen_opts.noise_intensity, 0.001f, 0.0f, 0.5f, "%.3f")) {
                        opts_updated = true;
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("Reset to Defaults")) {
                        gen_opts = pcd_noise_generator::generate_options_t{};
                        opts_updated = true;
                    }

                    if (opts_updated) {
                        _pcd_gen->set_options(gen_opts);
                    }
                }

                ImGui::Separator();

                ImGui::Checkbox("Inplace update", &_inplace_update);
            }

        }; // class
        
        _scene_ctrl_window->add_scene(
            std::make_shared<pointcloud_scene>(*_vis)
        );
    }

    void basic_demo_app::_add_bvh_scene()
    {
        using namespace triengine::geometry;
        using namespace triengine::io;

        class bvh_scene
            : public scene_wrapper
        {
            enum class playback_state_type
            {
                paused,
                playing,
            };

            struct ui_state_t
            {
                std::unique_ptr<triengine::io::bvh_file_t> bvh_data;
                std::unordered_map<bvh_joint_id_t/* parent */, std::vector<bvh_joint_id_t>/* childs */> hierarchy_map_cache;

                int current_frame_index{ 0 };
                float playback_speed{ 1.0f }; // speed factor
                playback_state_type playback_state{ playback_state_type::paused };
                bool fl_update_animation{ true };

                std::optional<bvh_joint_id_t> selected_joint_id;
                
                std::optional<triengine::mat4_f32> offset_transform;

                ui_state_t() = default;
            };

            ui_state_t _state;
            std::shared_ptr<triangle_mesh_object> _origin_axis;
            std::shared_ptr<skeleton_object> _skeleton;

        public:
            bvh_scene(
                triengine::visualization::visualizer& vis)
                : scene_wrapper(vis.add_scene())
            {
                const auto rsrc_dir_path = triengine::global_options::instance()->get_resource_directory();
                auto scn = this->get_scene();
                scn->set_name("bvh playback");

                scn->get_render_config()->show_origin_xz_grid = true;
                scn->get_render_config()->light_opts.point_light.position = triengine::vec3_f32{ 0.0f, -1.5f, -1.5f };
                scn->get_render_config()->light_opts.point_light.ambientIntensity = 0.0f;
                scn->get_render_config()->light_opts.point_light.diffuseIntensity = 2.8f;
                scn->get_render_config()->light_opts.point_light.specularIntensity = 1.5f;
                scn->get_render_config()->light_opts.dir_light.ambientIntensity = 0.2f;
                scn->get_render_config()->light_opts.dir_light.diffuseIntensity = 0.2f;
                scn->get_render_config()->light_opts.dir_light.specularIntensity = 0.5f;
                scn->get_render_config()->light_opts.bloom.strength = 0.05f;
                scn->get_render_config()->light_opts.hdr.exposure = 0.3f;
                scn->get_render_config()->inf_plane_opts.max_view_distance = 35.0f;

                {

                    Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
                    R = Eigen::AngleAxisf(triengine::math::deg2rad(180.0f), Eigen::Vector3f::UnitZ())
                        * Eigen::AngleAxisf(triengine::math::deg2rad(0.0f), Eigen::Vector3f::UnitY())
                        * Eigen::AngleAxisf(triengine::math::deg2rad(0.0f), Eigen::Vector3f::UnitX());

                    Eigen::Matrix4f Tr{ Eigen::Matrix4f::Identity() };
                    Tr.block<3, 3>(0, 0) = R;

                    _state.offset_transform = Tr;
                }

                _origin_axis = triangle_mesh_object::create_coordinate_frame(0.5f);
                scn->add_geometry(_origin_axis);

                {
                    _state.bvh_data = std::make_unique<triengine::io::bvh_file_t>();
                    if (!triengine::io::load_skeleton_from_bvh(
                        rsrc_dir_path / "bvh/xsens-sample-walk.bvh",
                        *_state.bvh_data
                    )) {
                        CXLIB_PANIC("failed to load skeletons from bvh file");
                    }
                    
                    // Build(rebuild) hierarchy cache
                    _state.hierarchy_map_cache.clear();
                    for (const auto [child_jid, parent_jid] : _state.bvh_data->joints_parent_map) {
                        if (child_jid == parent_jid) { continue; }
                        _state.hierarchy_map_cache[parent_jid].push_back(child_jid);
                    }

                    _state.selected_joint_id.reset();
                    _state.current_frame_index = 0;
                }
            }

            void update_animation() override
            {
                if (_state.fl_update_animation)
                {
                    if (_skeleton) {
                        this->get_scene()->remove_geometry(_skeleton);
                    }
                    _skeleton = this->_create_skeleton_object_from_bvh(*_state.bvh_data, _state.bvh_data->frames[_state.current_frame_index]);
                    if (_state.offset_transform) {
                        _skeleton->transform(_state.offset_transform.value(), true);
                        _origin_axis->transform(_state.offset_transform.value(), false);
                    }
                    this->get_scene()->add_geometry(_skeleton);

                    _state.fl_update_animation = false;
                }
            }

            void render_gui(
                [[maybe_unused]] const triengine::gui::window_render_context& render_ctx) override
            {
                if (!_state.bvh_data) {
                    ImGui::Text("No bvh file loaded.");
                    return;
                }

                const bvh_file_t& bvh_data = *_state.bvh_data;

                //
                // Playback handling
                // 

                if (_state.playback_state == playback_state_type::playing)
                {
                    using clock_type = std::chrono::high_resolution_clock;
                    using duration_type = std::chrono::microseconds;

                    thread_local clock_type::time_point tp_next_update{};

                    const float default_fps = 1.0 / bvh_data.frame_time;
                    const float scaled_fps = std::max(1.0f, default_fps * _state.playback_speed);

                    const auto update_step = std::chrono::milliseconds{ static_cast<int64_t>(1000.0f / scaled_fps) };

                    if (const auto tp_delta = clock_type::now() - tp_next_update;
                        tp_delta >= update_step)
                    {
                        tp_next_update += (tp_delta + update_step);

                        // move to next frame
                        _state.current_frame_index = (_state.current_frame_index + 1) % bvh_data.frames.size();
                        _state.fl_update_animation = true;
                    }
                }

                //
                // UI rendering
                // 

                ImGui::Text("Frame Time: %.4f seconds (%.1f FPS)"
                    , bvh_data.frame_time
                    , (bvh_data.frame_time > 0.0) ? 1.0 / bvh_data.frame_time : 0.0
                );

                ImGui::Separator();

                if (bvh_data.frames.empty()) {
                    ImGui::Text("No motion frames found.");
                    _state.current_frame_index = 0;
                    return;
                }

                if (ImGui::Button("|<")) {
                    _state.current_frame_index = 0;
                    _state.fl_update_animation = true;
                }

                ImGui::SameLine();

                if (_state.playback_state == playback_state_type::playing) {
                    if (ImGui::Button("||")) { 
                        _state.playback_state = playback_state_type::paused;
                    }
                } else {
                    if (ImGui::Button("> ")) {
                        _state.playback_state = playback_state_type::playing;
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button(">|")) {
                    _state.current_frame_index = _state.bvh_data->frames.size() - 1;
                    _state.fl_update_animation = true;
                }

                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
                if (ImGui::SliderInt("##FrameSlider"
                    , &_state.current_frame_index
                    , 0
                    , static_cast<int>(bvh_data.frames.size()) - 1
                    , "Frame: %d"))
                {
                    _state.playback_state = playback_state_type::paused;
                    _state.fl_update_animation = true;
                }
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::Text("/ %zu frames", bvh_data.frames.size());

                if (ImGui::DragFloat("Playback Speed", &_state.playback_speed, 0.01f, 0.01f, 4.0f, "%.2fx")) {
                    _state.fl_update_animation = true;
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("File Inspector", ImGuiTreeNodeFlags_None))
                {
                    // Left panel
                    const float left_panel_width = std::max(200.0f * render_ctx.dpi_scale, ImGui::GetContentRegionAvail().x * 0.45f);
                    ImGui::BeginChild("JointHierarchyPanel", ImVec2(left_panel_width, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
                    {
                        ImGui::Text("Joint Hierarchy");
                        ImGui::Separator();

                        const float indent_spacing = 8.0f * render_ctx.dpi_scale;
                        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, indent_spacing);
                        this->_render_hierarchy_tree(_state.bvh_data->root_joint_id);
                        ImGui::PopStyleVar();
                    }
                    ImGui::EndChild();

                    ImGui::SameLine();

                    // Right panel
                    ImGui::BeginChild("JointDetailsPanel", ImVec2(0, 0), true);
                    {
                        ImGui::Text("Selected Joint Details");
                        ImGui::Separator();

                        if (!_state.selected_joint_id) {
                            ImGui::Text("Select a joint from the hierarchy tree.");
                            ImGui::EndChild();
                            return;
                        }

                        CXLIB_ASSERT(_state.selected_joint_id.value() < bvh_data.joints.size());

                        const bvh_joint_id_t sel_bvh_jid = _state.selected_joint_id.value();
                        const bvh_joint_info_t& sel_bvh_jinfo = bvh_data.joints[sel_bvh_jid];
                        const bvh_joint_data_t& sel_bvh_jdata = bvh_data.frames[_state.current_frame_index].skeleton.at(sel_bvh_jid);

                        ImGui::Text("Joint Name: %s (#%zu)", sel_bvh_jinfo.name.c_str(), sel_bvh_jid);
                        ImGui::Text("Joint Length: %.6f", sel_bvh_jinfo.length);

                        ImGui::Spacing();
                        ImGui::Text("BVH Euler Angles (%s, deg):", sel_bvh_jinfo.bvh_euler_axis_order.c_str()); {
                            ImGui::Indent();
                            const auto& angles = sel_bvh_jdata.bvh_euler_angels;
                            ImGui::Text("[%.6f, %.6f, %.6f]", angles.x(), angles.y(), angles.z());
                            ImGui::Unindent();
                        }

                        ImGui::Spacing();
                        ImGui::Text("World Position:"); {
                            ImGui::Indent();
                            const auto& pos = sel_bvh_jdata.world_position;
                            ImGui::Text("[%.6f, %.6f, %.6f]", pos.x(), pos.y(), pos.z());
                            ImGui::Unindent();
                        }

                        ImGui::Spacing();
                        ImGui::Text("World Rotation:"); {
                            ImGui::Indent();
                            const auto& R = sel_bvh_jdata.world_rotation;
                            ImGui::Text("[%.6f, %.6f, %.6f]", R(0, 0), R(0, 1), R(0, 2));
                            ImGui::Text("[%.6f, %.6f, %.6f]", R(1, 0), R(1, 1), R(1, 2));
                            ImGui::Text("[%.6f, %.6f, %.6f]", R(2, 0), R(2, 1), R(2, 2));
                            ImGui::Unindent();
                        }
                    }
                    ImGui::EndChild();
                } // collapsing header
            }

        private:
            void _render_hierarchy_tree(
                const bvh_joint_id_t bvh_jid)
            {
                CXLIB_ASSERT(bvh_jid < _state.bvh_data->joints.size());

                const bool has_children = _state.hierarchy_map_cache.count(bvh_jid) && _state.hierarchy_map_cache.at(bvh_jid).size() > 0;
                const auto& bvh_jinfo = _state.bvh_data->joints[bvh_jid];

                char node_label[256];
                snprintf(node_label, sizeof(node_label), "%s (#%zu)", bvh_jinfo.name.c_str(), bvh_jid);

                ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (_state.selected_joint_id == bvh_jid) { tree_node_flags |= ImGuiTreeNodeFlags_Selected; }
                if (!has_children) { tree_node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; }

                bool open_tree_node = ImGui::TreeNodeEx((void*)(intptr_t)bvh_jid, tree_node_flags, "%s", node_label);
                if (!has_children) {
                    open_tree_node = false; // If there are no children (Leaf), TreeNodeEx only displays the contents and does not open.
                }

                if (ImGui::IsItemClicked()) {
                    _state.selected_joint_id = bvh_jid; // update selected jid
                }

                if (open_tree_node && has_children) {
                    for (const bvh_joint_id_t child_bvh_jid : _state.hierarchy_map_cache.at(bvh_jid)) {
                        this->_render_hierarchy_tree(child_bvh_jid);
                    }
                    ImGui::TreePop();
                }
            }

            std::shared_ptr<skeleton_object> _create_skeleton_object_from_bvh(
                const bvh_file_t& bvh_file,
                const bvh_motion_frame_t& bvh_frame) const
            {
                constexpr double kScaleCM2M = 0.01;

                auto new_skeleton = skeleton_object::create();

                // Assign the correct color based on the body id
                const auto
                    hi_conf_color = triengine::color3_f32{ 0.8f, 0.8f, 0.8f },
                    lo_conf_color = triengine::color3_f32{ 0.6f, 0.6f, 0.6f };

                // Visualize joints
                for (const auto& [bvh_jid, bvh_jdata] : bvh_frame.skeleton)
                {
                    new_skeleton->add_joint(
                        (bvh_jdata.world_position * kScaleCM2M).cast<float>(),
                        bvh_jdata.world_rotation.cast<float>(),
                        hi_conf_color
                    );
                }

                // Visualize bones
                for (const auto [child_jid, parent_jid] : bvh_file.joints_parent_map)
                {
                    if (child_jid == parent_jid) { continue; }

                    const auto
                        & child_jdata = bvh_frame.skeleton.at(child_jid), 
                        & parent_jdata = bvh_frame.skeleton.at(parent_jid);

                    new_skeleton->add_bone(
                        (child_jdata.world_position * kScaleCM2M).cast<float>(),
                        (parent_jdata.world_position * kScaleCM2M).cast<float>(),
                        hi_conf_color
                    );
                }

                return new_skeleton;
            }

        }; // class

        _scene_ctrl_window->add_scene(
            std::make_shared<bvh_scene>(*_vis)
        );
    }

} // namespace