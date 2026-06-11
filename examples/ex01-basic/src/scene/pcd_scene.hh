#pragma once
#include "../scene_wrapper.hh"

#include <triengine/math/math3d.hh>
#include <triengine/geometry/pcd_object.hh>
#include <triengine/io/file_ply_loader.hh>
#include <triengine/utility/color_map.hh>

#include <vector>
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
                    XUTL_PANIC("Cannot start with empty or null original pointcloud");
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

        /// A named color map preset, used to populate the colorize GUI combo.
        struct named_color_map {
            const char* name;
            const utility::color_map& (*getter)();
        };

        const named_color_map kColorizeColorMaps[] = {
            { "Jet",           &utility::color_map_presets::jet },
            { "Classic",       &utility::color_map_presets::classic },
            { "Hue",           &utility::color_map_presets::hue },
            { "Grayscale",     &utility::color_map_presets::grayscale },
            { "Inv Grayscale", &utility::color_map_presets::inv_grayscale },
            { "Biomes",        &utility::color_map_presets::biomes },
            { "Cold",          &utility::color_map_presets::cold },
            { "Warm",          &utility::color_map_presets::warm },
        };

    } // namespace

    class pointcloud_scene
        : public scene_wrapper
    {
    private:
        /// Scalar source used when colorizing the point cloud.
        enum class colorize_source { axis, distance };

        /// All user-controllable colorize parameters.
        struct colorize_settings {
            bool enabled{ false };
            colorize_source source{ colorize_source::axis };
            geometry::colorize_axis axis{ geometry::colorize_axis::z };
            Eigen::Vector3f reference_point{ Eigen::Vector3f::Zero() };
            int color_map_index{ 0 };
            geometry::colorize_mapping mapping{ geometry::colorize_mapping::linear };
            bool auto_range{ true };
            float range_min{ 0.0f };
            float range_max{ 1.0f };
            int histogram_bins{ 4096 };
        };

        std::shared_ptr<geometry::pcd_object> _pcd;
        std::shared_ptr<geometry::mesh_object> _pcd_axis_frame;
        std::unique_ptr<pcd_noise_generator> _pcd_gen;
        bool _inplace_update{ true };
        colorize_settings _colorize;
        std::vector<color3_f32> _pcd_colors_original; // colors as loaded, before any colorize

    private:
        /// Builds a `colorize_options` from the current `_colorize` settings.
        geometry::colorize_options _build_colorize_options() const
        {
            geometry::colorize_options opts;
            opts.mapping = _colorize.mapping;
            opts.histogram_bins = _colorize.histogram_bins;
            if (!_colorize.auto_range) {
                opts.range = geometry::colorize_range{ _colorize.range_min, _colorize.range_max };
            }
            return opts;
        }

        /// Applies colorize to `_pcd` using the current settings. No-op when disabled.
        void _apply_colorize()
        {
            if (!_pcd || !_colorize.enabled) { return; }

            const utility::color_map& cmap = kColorizeColorMaps[_colorize.color_map_index].getter();
            const auto opts = _build_colorize_options();
            if (_colorize.source == colorize_source::axis) {
                _pcd->colorize_by_axis(_colorize.axis, cmap, opts);
            } else {
                _pcd->colorize_by_distance(_colorize.reference_point, cmap, opts);
            }
        }

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
            scn->get_render_config()->light_opts.dir_light.ambient_intensity = 0.1f;
            scn->get_render_config()->light_opts.dir_light.diffuse_intensity = 1.5f;
            scn->get_render_config()->light_opts.dir_light.specular_intensity = 1.5f;
            scn->get_render_config()->light_opts.point_light.position = vec3_f32{ 0.0f, 1.5f, -1.5f };

            Eigen::Matrix4f offset_Tr{ Eigen::Matrix4f::Identity() }; {
                Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
                R = Eigen::AngleAxisf(math::deg2rad(180.0f), Eigen::Vector3f::UnitZ())
                    * Eigen::AngleAxisf(math::deg2rad(0.0f), Eigen::Vector3f::UnitY())
                    * Eigen::AngleAxisf(math::deg2rad(0.0f), Eigen::Vector3f::UnitX());
                offset_Tr.block<3, 3>(0, 0) = R;
            }

            _pcd = std::make_shared<geometry::pcd_object>();
            if (io::load_pointcloud_from_ply(
                rsrc_dir_path / "pointcloud/sample.ply",
                *_pcd
            )) {
                _pcd->transform(offset_Tr, true);
                scn->add_geometry(_pcd);
            }

            // Keep a copy of the loaded colors so colorize can be toggled off.
            _pcd_colors_original = _pcd->colors;
            // Default the distance colorize reference to the point cloud center.
            _colorize.reference_point = _pcd->get_center();

            _pcd_axis_frame = geometry::mesh_object::create_coordinate_frame(0.5f);
            _pcd_axis_frame->transform(offset_Tr, true);
            scn->add_geometry(_pcd_axis_frame);

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

                // Colorize is applied last, after the noise pass
                this->_apply_colorize();
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

            ImGui::Separator();
            ImGui::TextUnformatted("Colorize");

            if (ImGui::Checkbox("Enable Colorize", &_colorize.enabled)) {
                if (_colorize.enabled) {
                    this->_apply_colorize();
                } else if (_pcd) {
                    // Restore the point cloud's original colors.
                    _pcd->colors = _pcd_colors_original;
                    _pcd->mark_dirty();
                }
            }

            if (_colorize.enabled)
            {
                bool colorize_changed{ false };

                int source_idx = static_cast<int>(_colorize.source);
                if (ImGui::Combo("Source", &source_idx, "Axis\0Distance\0")) {
                    _colorize.source = static_cast<colorize_source>(source_idx);
                    colorize_changed = true;
                }

                if (_colorize.source == colorize_source::axis) {
                    int axis_idx = static_cast<int>(_colorize.axis);
                    if (ImGui::Combo("Axis", &axis_idx, "X\0Y\0Z\0")) {
                        _colorize.axis = static_cast<geometry::colorize_axis>(axis_idx);
                        colorize_changed = true;
                    }
                } else {
                    if (ImGui::DragFloat3("Reference Point", _colorize.reference_point.data(), 0.01f)) {
                        colorize_changed = true;
                    }
                }

                const char* color_map_names[IM_ARRAYSIZE(kColorizeColorMaps)];
                for (int i = 0; i < IM_ARRAYSIZE(kColorizeColorMaps); ++i) {
                    color_map_names[i] = kColorizeColorMaps[i].name;
                }
                if (ImGui::Combo("Color Map", &_colorize.color_map_index,
                                 color_map_names, IM_ARRAYSIZE(kColorizeColorMaps))) {
                    colorize_changed = true;
                }

                // "Dynamic" maps colors via cumulative-histogram equalization.
                int mapping_idx = static_cast<int>(_colorize.mapping);
                if (ImGui::Combo("Mapping", &mapping_idx, "Linear\0Dynamic\0")) {
                    _colorize.mapping = static_cast<geometry::colorize_mapping>(mapping_idx);
                    colorize_changed = true;
                }

                if (_colorize.mapping == geometry::colorize_mapping::dynamic) {
                    if (ImGui::SliderInt("Histogram Bins", &_colorize.histogram_bins, 16, 16384)) {
                        colorize_changed = true;
                    }
                }

                if (ImGui::Checkbox("Auto Range", &_colorize.auto_range)) {
                    colorize_changed = true;
                }

                if (!_colorize.auto_range) {
                    // DragFloatRange2 keeps range_min <= range_max automatically.
                    if (ImGui::DragFloatRange2("Range",
                        &_colorize.range_min, &_colorize.range_max,
                        0.01f, 0.0f, 0.0f, "%.3f")) {
                        colorize_changed = true;
                    }
                }

                // Re-apply immediately so colorize changes are reflected without
                // waiting for the next noise update tick.
                if (colorize_changed) {
                    this->_apply_colorize();
                }
            }
        }

    }; // class

} // namespace