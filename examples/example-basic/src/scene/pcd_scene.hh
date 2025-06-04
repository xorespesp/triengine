#pragma once
#include "../scene_wrapper.hh"

#include <triengine/math/math3d.hh>
#include <triengine/geometry/pcd_object.hh>
#include <triengine/io/file_ply_loader.hh>

#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include <chrono>

namespace demo::scene
{
    using namespace triengine;

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

            std::shared_ptr<geometry::pcd_object> _pcd_original; // The unmodified base point cloud
            mutable std::shared_ptr<geometry::pcd_object> _pcd_noisy; // The latest generated noisy point cloud (shared)

            std::thread _worker_thread;
            std::atomic_bool _is_running{ false };
            mutable std::atomic_bool _is_data_ready{ false };
            mutable std::mutex _data_mtx;

        public:
            pcd_noise_generator(
                const geometry::pcd_object& original_pcd,
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
            std::shared_ptr<geometry::pcd_object> get_noisy_pointcloud()
            {
                std::scoped_lock lk{ _data_mtx };
                if (_is_data_ready) {
                    _is_data_ready = false;
                    // Return a copy to minimize lock time
                    return _pcd_noisy->clone();
                }
                else {
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
                        wave_frequency_k{ 2.0f * math::pi<float>() / curr_opts.wave_length }, // Spatial frequency (k = 2*pi / wavelength)
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
                        }
                        else {
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

    class pointcloud_scene
        : public scene_wrapper
    {
        std::shared_ptr<geometry::pcd_object> _pcd;
        std::unique_ptr<pcd_noise_generator> _pcd_gen;
        bool _inplace_update{ true };

    public:
        pointcloud_scene(
            visualization::visualizer& vis)
            : scene_wrapper(vis.add_scene())
        {
            const auto rsrc_dir_path = global_options::instance()->get_resource_directory();
            auto scn = this->get_scene();
            scn->set_name("pointcloud");

            scn->get_render_config()->pcd_point_size = 5.0f;
            scn->get_render_config()->show_origin_xz_grid = false;
            scn->get_render_config()->light_opts.dir_light.ambientIntensity = 0.1f;
            scn->get_render_config()->light_opts.dir_light.diffuseIntensity = 1.5f;
            scn->get_render_config()->light_opts.dir_light.specularIntensity = 2.5f;
            scn->get_render_config()->light_opts.point_light.position = vec3_f32{ 0.0f, -1.5f, -1.5f };

            auto mesh_axis_frame = geometry::triangle_mesh_object::create_coordinate_frame(0.5f);
            scn->add_geometry(mesh_axis_frame);

            _pcd = std::make_shared<geometry::pcd_object>();
            if (io::load_pointcloud_from_ply(
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
            [[maybe_unused]] const gui::window_render_context& render_ctx) override
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

} // namespace