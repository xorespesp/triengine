#include <triengine/global_options.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/visualization/visualizer.hh>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    // Draw-call bound scene: many small lit boxes, each with its own transform and material,
    // seen from a fixed camera. Every run with the same options renders the same frames.
    //   - ESC : close the window

    constexpr uint32_t kSceneSeed = 20261108;
    constexpr float kBoxSize = 0.2f;
    constexpr float kGridSpacing = 0.5f;
    constexpr double kReportIntervalSeconds = 1.0;

    struct bench_options {
        size_t object_count{ 10000 };
        bool postfx{ false }; // bloom and SMAA; off keeps the GPU cost small next to the submission cost
    };

    void print_usage()
    {
        std::fprintf(stderr,
            "usage: draw-bench [--n <object count>] [--postfx on|off]\n"
            "  --n       number of boxes (default 10000)\n"
            "  --postfx  bloom and SMAA (default off)\n");
    }

    std::optional<bench_options> parse_args(const int argc, char** argv)
    {
        bench_options opts;
        for (int i = 1; i < argc; ++i) {
            const bool has_value = i + 1 < argc;
            if (std::strcmp(argv[i], "--n") == 0 && has_value) {
                char* end{};
                const unsigned long long n = std::strtoull(argv[++i], &end, 10);
                if (*end != '\0' || n == 0) {
                    return std::nullopt;
                }
                opts.object_count = static_cast<size_t>(n);
            }
            else if (std::strcmp(argv[i], "--postfx") == 0 && has_value) {
                const std::string value{ argv[++i] };
                if (value != "on" && value != "off") {
                    return std::nullopt;
                }
                opts.postfx = value == "on";
            }
            else {
                return std::nullopt;
            }
        }
        return opts;
    }

    // Lays the boxes out on a cube-shaped grid centered at the origin.
    void populate_scene(triengine::scene& scn, const size_t object_count)
    {
        const size_t side = static_cast<size_t>(std::ceil(std::cbrt(static_cast<double>(object_count))));
        const float half_extent = 0.5f * kGridSpacing * static_cast<float>(side - 1);

        std::mt19937 rng{ kSceneSeed };
        std::uniform_real_distribution<float> unit{ 0.0f, 1.0f };
        std::uniform_real_distribution<float> signed_unit{ -1.0f, 1.0f };

        for (size_t i = 0; i < object_count; ++i) {
            const size_t x = i % side;
            const size_t y = (i / side) % side;
            const size_t z = i / (side * side);

            auto box = triengine::geometry::mesh_object::create_box(kBoxSize, kBoxSize, kBoxSize);
            box->paint_uniform_color(triengine::color3_f32{ unit(rng), unit(rng), unit(rng) });

            // Distinct per-object values, so no draw can reuse the previous one's state.
            auto* const material = box->get_vertex_shading_material();
            material->ambient_intensity = 0.6f + 0.4f * unit(rng);
            material->diffuse_intensity = 0.6f + 0.4f * unit(rng);
            material->specular_intensity = 0.1f + 0.4f * unit(rng);
            material->shininess = static_cast<uint16_t>(16 + static_cast<int>(unit(rng) * 112.0f));

            const triengine::vec3_f32 axis = triengine::vec3_f32{ signed_unit(rng), signed_unit(rng), signed_unit(rng) }.normalized();
            box->rotate(triengine::quat_f32{ Eigen::AngleAxisf{ unit(rng) * 3.14159265f, axis } });
            box->translate(triengine::vec3_f32{
                kGridSpacing * static_cast<float>(x) - half_extent,
                kGridSpacing * static_cast<float>(y) - half_extent,
                kGridSpacing * static_cast<float>(z) - half_extent });

            scn.add_geometry(box);
        }

        scn.switch_camera_type(triengine::camera_type::arcball);
        auto& cam = *scn.get_camera()->as<triengine::arcball_camera>();
        cam.set_pivot_point(triengine::vec3_f32{ 0.0f, 0.0f, 0.0f });
        cam.set_yaw(30.0f);
        cam.set_pitch(25.0f);
        cam.set_zoom_distance(1.0f + 4.6f * half_extent); // fits the grid's bounding sphere in the 45 deg vertical fov
    }

    // `q` in [0, 1]; reorders `values`.
    double quantile(std::vector<double>& values, const double q)
    {
        const size_t k = static_cast<size_t>(q * static_cast<double>(values.size() - 1) + 0.5);
        std::nth_element(values.begin(), values.begin() + static_cast<ptrdiff_t>(k), values.end());
        return values[k];
    }

    class draw_bench_app
    {
    public:
        explicit draw_bench_app(const bench_options& opts)
            : _opts{ opts }
        {}

        void create()
        {
            _vis = std::make_unique<triengine::visualization::visualizer>();
            _vis->create_window("Triengine Draw Bench", true, 1280, 720,
                false/* fullscreen */, false/* enable_vsync */, 0/* max_fps: uncapped */);
            _vis->enable_camera_interaction(false);

            _vis->set_key_callback(
                [this](
                    const int32_t key,
                    [[maybe_unused]] const int32_t scancode,
                    const int32_t action,
                    [[maybe_unused]] const int32_t mods,
                    [[maybe_unused]] bool& handled)
                {
                    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
                        _vis->close_window();
                    }
                });

            auto scn = _vis->add_scene();
            auto& render_config = *scn->get_render_config();
            render_config.enable_anti_aliasing = _opts.postfx;
            render_config.light_opts.bloom.enabled = _opts.postfx;
            populate_scene(*scn, _opts.object_count);

            auto& profiler = _vis->get_frame_profiler();
            for (size_t id = 0; id < profiler.stage_count(); ++id) {
                const std::string& name = profiler.get_stage(id).name;
                if (name == triengine::visualization::visualizer::profiling_stages::kSceneRender.name) {
                    _scene_render_stage = id;
                } else if (name == triengine::visualization::visualizer::profiling_stages::kSwap.name) {
                    _swap_stage = id;
                }
            }
            profiler.request_enabled(true);

            std::printf("objects=%zu postfx=%s\n", _opts.object_count, _opts.postfx ? "on" : "off");
        }

        void destroy()
        {
            _vis->destroy_window();
            _vis.reset();
        }

        void run()
        {
            while (_vis->update_window()) {
                _vis->render();
                _report();
            }
        }

    private:
        // Prints the median and p95 of the frames recorded since the last report.
        void _report()
        {
            const auto& profiler = _vis->get_frame_profiler();
            if (profiler.history_epoch() != _report_epoch) {
                _report_epoch = profiler.history_epoch();
                _last_report_time = 0.0;
            }
            if (profiler.record_count() == 0) {
                return;
            }

            const double newest_time = profiler.get_record(profiler.record_count() - 1).time_seconds();
            if (newest_time - _last_report_time < kReportIntervalSeconds) {
                return;
            }

            _frame_ms.clear();
            _scene_render_ms.clear();
            _swap_ms.clear();
            for (size_t i = profiler.find_first_record_after(_last_report_time); i < profiler.record_count(); ++i) {
                const auto record = profiler.get_record(i);
                _frame_ms.push_back(record.frame_ms());
                // `NaN` marks a stage that was not measured in that frame.
                if (_scene_render_stage && !std::isnan(record.stage_ms(*_scene_render_stage))) {
                    _scene_render_ms.push_back(record.stage_ms(*_scene_render_stage));
                }
                if (_swap_stage && !std::isnan(record.stage_ms(*_swap_stage))) {
                    _swap_ms.push_back(record.stage_ms(*_swap_stage));
                }
            }
            _last_report_time = newest_time;

            if (_frame_ms.empty()) {
                return;
            }
            const size_t frames = _frame_ms.size();
            const double frame_p50 = quantile(_frame_ms, 0.50);
            const double frame_p95 = quantile(_frame_ms, 0.95);
            const double render_p50 = _scene_render_ms.empty() ? 0.0 : quantile(_scene_render_ms, 0.50);
            const double swap_p50 = _swap_ms.empty() ? 0.0 : quantile(_swap_ms, 0.50);
            std::printf("frames=%4zu  frame p50=%7.3f ms p95=%7.3f ms  scene_render p50=%7.3f ms  swap p50=%7.3f ms\n",
                frames, frame_p50, frame_p95, render_p50, swap_p50);
        }

    private:
        bench_options _opts;
        std::unique_ptr<triengine::visualization::visualizer> _vis;

        std::optional<size_t> _scene_render_stage;
        std::optional<size_t> _swap_stage;
        size_t _report_epoch{};
        double _last_report_time{};

        // Reused per report
        std::vector<double> _frame_ms;
        std::vector<double> _scene_render_ms;
        std::vector<double> _swap_ms;
    };

} // namespace

int main(int argc, char** argv)
{
    const auto opts = parse_args(argc, argv);
    if (!opts) {
        print_usage();
        return 1;
    }

    try
    {
        auto& logger = triengine::global_options::instance()->get_logger();
        logger.set_log_level(triengine::utility::log_level::warn);
        const auto log_subscription = logger.subscribe(
            [](triengine::utility::log_level, const std::string& msg)
            {
                std::fprintf(stderr, "[triengine] %s\n", msg.c_str());
            });

        draw_bench_app app{ *opts };
        app.create();
        app.run();
        app.destroy();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
