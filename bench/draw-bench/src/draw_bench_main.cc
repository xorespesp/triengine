#include <triengine/global_options.hh>
#include <triengine/core/gpu_frame_profiler.hh>
#include <triengine/utility/gl_utils.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/visualization/visualizer.hh>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
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
#if defined(TRIENGINE_DEBUG_MODE)
    constexpr size_t kTopGlCallSites = 8;
#endif

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

    // Median of a stage over the records with a frame ID in `[first_id, last_id]`; `NaN` when there is none.
    template <class _Profiler>
    double stage_p50(const _Profiler& profiler, const size_t stage, const uint64_t first_id, const uint64_t last_id,
        std::vector<double>& samples)
    {
        samples.clear();
        for (size_t i = profiler.record_count(); i-- > 0;) {
            const auto record = profiler.get_record(i);
            if (record.frame_id() < first_id) {
                break;
            }
            const double ms = record.stage_ms(stage);
            if (record.frame_id() <= last_id && !std::isnan(ms)) {
                samples.push_back(ms);
            }
        }
        return samples.empty() ? std::numeric_limits<double>::quiet_NaN() : quantile(samples, 0.50);
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

            auto& profiler = _vis->get_cpu_frame_profiler();
            for (size_t id = 0; id < profiler.stage_count(); ++id) {
                const std::string& name = profiler.get_stage(id).name;
                if (name == triengine::visualization::visualizer::profiling_stages::kSceneRender.name) {
                    _scene_render_stage = id;
                } else if (name == triengine::visualization::visualizer::profiling_stages::kSwap.name) {
                    _swap_stage = id;
                }
            }
            profiler.request_enabled(true);
            _vis->get_pass_cpu_profiler().request_enabled(true);
            _vis->get_pass_gpu_profiler().request_enabled(true);

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
            const auto& profiler = _vis->get_cpu_frame_profiler();
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

            _frame_ms_samples.clear();
            _scene_render_ms_samples.clear();
            _swap_ms_samples.clear();
            const size_t first_index = profiler.find_first_record_after(_last_report_time);
            if (first_index == profiler.record_count()) {
                return;
            }
            const uint64_t first_frame_id = profiler.get_record(first_index).frame_id();
            const uint64_t last_frame_id = profiler.get_record(profiler.record_count() - 1).frame_id();
            for (size_t i = first_index; i < profiler.record_count(); ++i) {
                const auto record = profiler.get_record(i);
                _frame_ms_samples.push_back(record.frame_ms());
                // `NaN` marks a stage that was not measured in that frame.
                if (_scene_render_stage && !std::isnan(record.stage_ms(*_scene_render_stage))) {
                    _scene_render_ms_samples.push_back(record.stage_ms(*_scene_render_stage));
                }
                if (_swap_stage && !std::isnan(record.stage_ms(*_swap_stage))) {
                    _swap_ms_samples.push_back(record.stage_ms(*_swap_stage));
                }
            }
            _last_report_time = newest_time;

            if (_frame_ms_samples.empty()) {
                return;
            }
            const size_t frames = _frame_ms_samples.size();
            const double frame_p50 = quantile(_frame_ms_samples, 0.50);
            const double frame_p95 = quantile(_frame_ms_samples, 0.95);
            const double render_p50 = _scene_render_ms_samples.empty() ? 0.0 : quantile(_scene_render_ms_samples, 0.50);
            const double swap_p50 = _swap_ms_samples.empty() ? 0.0 : quantile(_swap_ms_samples, 0.50);
            std::printf("frames=%4zu  frame p50=%7.3f ms p95=%7.3f ms  scene_render p50=%7.3f ms  swap p50=%7.3f ms\n",
                frames, frame_p50, frame_p95, render_p50, swap_p50);

            this->_report_passes(first_frame_id, last_frame_id);
#if defined(TRIENGINE_DEBUG_MODE)
            this->_report_gl_calls();
#endif
        }

#if defined(TRIENGINE_DEBUG_MODE)
        // GL call counting needs `TRIENGINE_ENABLE_DEBUG_MODE`, which the `-ship` presets turn off.
        // Call counts are deterministic, so the last frame stands for the whole report.
        void _report_gl_calls()
        {
            const auto& gl_calls = _vis->get_last_frame_gl_calls();

            std::printf("  gl calls/frame: total %llu  glProgramUniform* %llu  glDraw* %llu  glBindVertexArray %llu\n",
                static_cast<unsigned long long>(gl_calls.total_calls()),
                static_cast<unsigned long long>(gl_calls.calls_with_prefix("glProgramUniform")),
                static_cast<unsigned long long>(gl_calls.calls_with_prefix("glDraw")),
                static_cast<unsigned long long>(gl_calls.calls_with_prefix("glBindVertexArray")));

            // The busiest call sites, once, after the warm-up report.
            if (++_gl_call_reports != 2) {
                return;
            }
            std::vector<triengine::utility::gl_call_site_id_t> sites(gl_calls.site_count());
            for (triengine::utility::gl_call_site_id_t id = 0; id < sites.size(); ++id) {
                sites[id] = id;
            }
            const size_t shown = std::min<size_t>(kTopGlCallSites, sites.size());
            std::partial_sort(sites.begin(), sites.begin() + static_cast<ptrdiff_t>(shown), sites.end(),
                [&](const auto a, const auto b) { return gl_calls.site_calls(a) > gl_calls.site_calls(b); });

            std::printf("  top gl call sites:\n");
            for (size_t i = 0; i < shown; ++i) {
                const auto site = triengine::utility::get_gl_call_site(sites[i]);
                const std::string_view file = site.location.filename();
                std::printf("    %8llu  %.*s  %.*s:%d\n",
                    static_cast<unsigned long long>(gl_calls.site_calls(sites[i])),
                    static_cast<int>(site.function_name.size()), site.function_name.data(),
                    static_cast<int>(file.size()), file.data(), site.location.line);
            }
        }
#endif

        // Prints the per-pass CPU and GPU medians of the same frames. GPU results arrive a few frames
        // late, so the newest frames of the range may have CPU values only.
        void _report_passes(const uint64_t first_frame_id, const uint64_t last_frame_id)
        {
            const auto& cpu_profiler = _vis->get_pass_cpu_profiler();
            const auto& gpu_profiler = _vis->get_pass_gpu_profiler();

            std::printf("  pass p50 cpu/gpu ms:");
            for (size_t stage = 0; stage < cpu_profiler.stage_count(); ++stage) {
                const double cpu_ms = stage_p50(cpu_profiler, stage, first_frame_id, last_frame_id, _pass_ms_samples);
                const double gpu_ms = stage_p50(gpu_profiler, stage, first_frame_id, last_frame_id, _pass_ms_samples);
                if (std::isnan(cpu_ms) && std::isnan(gpu_ms)) {
                    continue; // pass skipped in these frames
                }
                std::printf("  %s %.3f/%.3f", cpu_profiler.get_stage(stage).name.c_str(), cpu_ms, gpu_ms);
            }
            if (gpu_profiler.dropped_frame_count() > 0) {
                std::printf("  (gpu dropped %zu)", gpu_profiler.dropped_frame_count());
            }
            std::printf("\n");
        }

    private:
        bench_options _opts;
        std::unique_ptr<triengine::visualization::visualizer> _vis;

        std::optional<size_t> _scene_render_stage;
        std::optional<size_t> _swap_stage;
        size_t _report_epoch{};
        double _last_report_time{};

        // Samples of the frames in one report; cleared and refilled each time, kept to reuse their capacity.
        std::vector<double> _frame_ms_samples;
        std::vector<double> _scene_render_ms_samples;
        std::vector<double> _swap_ms_samples;
        std::vector<double> _pass_ms_samples;
#if defined(TRIENGINE_DEBUG_MODE)
        size_t _gl_call_reports{};
#endif
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
