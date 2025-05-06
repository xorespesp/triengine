#pragma once
#include "basic_demo_app.hh"

#include <triengine/math/math3d.hh>
#include <triengine/io/file_obj_loader.hh>
#include <triengine/io/file_ply_loader.hh>

#include <utils/logger.hh>
#include <utils/bit_cast.hh>
#include <utils/scrolling_buffer.hh>

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
                    TRIENGINE_PANIC("Cannot start with empty or null original pointcloud");
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

    basic_demo_app::basic_demo_app(
        const std::filesystem::path& triengine_resource_dir)
        : _triengine_resource_dir{ triengine_resource_dir }
    {
        if (!std::filesystem::is_directory(_triengine_resource_dir)) {
            TRIENGINE_PANIC("Invalid resource directory path");
        }
    }

    basic_demo_app::~basic_demo_app()
    {}

    void basic_demo_app::create()
    {
        LOG_TRACE("%s() ENTER", __func__);

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
                    TRIENGINE_TRACE("key: %d", key);

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
    }

    void basic_demo_app::destroy()
    {
        LOG_TRACE("%s() ENTER", __func__);

        _log_window.reset();
        _render_stats_window.reset();
        
        _vis->destroy_window();
        _vis.reset();

        LOG_TRACE("%s() LEAVE", __func__);
    }

    void basic_demo_app::run()
    {
        LOG_TRACE("%s() ENTER", __func__);

        LOG_INFO("polling start..");
        while (_vis->update_window())
        {
            // animate
            if (_flag_animation) {
                _scene_ctrl_window->update_animation();
            }

            _vis->render();
        } // while

        LOG_TRACE("%s() LEAVE", __func__);
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
                triengine::visualization::visualizer& vis,
                const std::filesystem::path& rsrc_dir_path)
                : scene_wrapper(vis.add_scene())
            {
                auto scn = this->get_scene();
                scn->set_name("main");

                scn->get_render_config()->show_origin_xz_grid = true;
                scn->get_render_config()->light_opts.point_light.position = triengine::vec3_f32{ 0.0f, -1.5f, -1.5f };
                scn->get_camera()->set_mirror_mode(false);
        
                auto mesh_axis_frame = triangle_mesh_object::create_coordinate_frame(0.5f);
                //mesh_axis_frame->paint_uniform_color(_get_next_color());
                //mesh_axis_frame->translate(Eigen::Vector3f{ 1.8f, 0.0f, -1.5f });
                scn->add_geometry(mesh_axis_frame);

                _skull_mesh = std::make_shared<triangle_mesh_object>();
                if (triengine::io::load_triangle_mesh_from_obj(
                        rsrc_dir_path / "objects/skull/12140_Skull_v3_L2.obj",
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
            
            void render_gui(const triengine::gui::window_render_context& render_ctx) override
            {
                ImGui::Text("main scene gui!");
            }
        };

        _scene_ctrl_window->add_scene(
            std::make_shared<main_scene>(*_vis, _triengine_resource_dir)
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
                triengine::visualization::visualizer& vis,
                const std::filesystem::path& rsrc_dir_path)
                : scene_wrapper(vis.add_scene())
            {
                auto scn = this->get_scene();
                scn->set_name("engine");

                scn->get_render_config()->show_origin_xz_grid = false;
                scn->get_render_config()->bg_color = triengine::color4_f32{ 0.8f, 0.8f, 0.8f, 1.0f };
                scn->get_render_config()->light_opts.point_light.enabled = false;
                scn->get_render_config()->light_opts.dir_light.ambientIntensity = 0.25f;
                scn->get_render_config()->light_opts.dir_light.diffuseIntensity = 0.5f;
                scn->get_render_config()->light_opts.dir_light.specularIntensity = 0.35f;
        
                scn->get_camera()->set_position(triengine::vec3_f32{ 0.0f, -2.2326f, -11.1618f });
                scn->get_camera()->set_mirror_mode(false);
        
                _engine_mesh = std::make_shared<triangle_mesh_object>();

                if (triengine::io::load_triangle_mesh_from_obj(
                        rsrc_dir_path / "objects/car_engine/car_engine.obj",
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
            
            void render_gui(const triengine::gui::window_render_context& render_ctx) override
            {
                ImGui::Text("engine scene gui!");
            }

        }; // class

        _scene_ctrl_window->add_scene(
            std::make_shared<engine_scene>(*_vis, _triengine_resource_dir)
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
                triengine::visualization::visualizer& vis,
                const std::filesystem::path& rsrc_dir_path)
                : scene_wrapper(vis.add_scene())
            {
                auto scn = this->get_scene();
                scn->set_name("pointcloud");

                scn->get_render_config()->pcd_point_size = 5.0f;
                scn->get_render_config()->show_origin_xz_grid = false;
                scn->get_render_config()->light_opts.dir_light.diffuseIntensity = 1.0f;
                scn->get_render_config()->light_opts.dir_light.specularIntensity = 1.0f;
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
            
            void render_gui(const triengine::gui::window_render_context& render_ctx) override
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
            std::make_shared<pointcloud_scene>(*_vis, _triengine_resource_dir)
        );
    }

} // namespace