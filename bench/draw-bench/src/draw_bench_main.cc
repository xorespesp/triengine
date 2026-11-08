#include <triengine/global_options.hh>
#include <triengine/core/gpu_frame_profiler.hh>
#include <triengine/utility/gl_utils.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/visualization/visualizer.hh>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>
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
    constexpr bool kDebugMode = true;
    constexpr size_t kTopGlCallSites = 8;
#else
    constexpr bool kDebugMode = false;
#endif

    // Sweep timing per object count. The warm-up discards the first frames, which upload the new boxes.
    constexpr std::chrono::seconds kSweepWarmup{ 2 };
    constexpr std::chrono::seconds kSweepMeasure{ 5 };
    // Frames rendered after a measurement at most, waiting for its last GPU results.
    constexpr size_t kMaxGpuResultWaitFrames = 16;

    using bench_clock = std::chrono::steady_clock;
    using json = nlohmann::ordered_json;

    // Microsecond resolution, as on the console. `NaN` (no samples) stays `NaN`, which the JSON file shows as `null`.
    double round_ms(const double ms)
    {
        return std::round(ms * 1000.0) / 1000.0;
    }

    struct bench_options {
        size_t object_count{ 10000 };
        std::vector<size_t> sweep_object_counts; // empty: report every second for `object_count` until closed
        std::string json_path; // sweep results, written when the bench exits
        bool postfx{ false }; // bloom and SMAA; off keeps the GPU cost small next to the submission cost
    };

    void print_usage()
    {
        std::fprintf(stderr,
            "usage: draw-bench [--n <object count> | --sweep <count,count,...> [--json <path>]] [--postfx on|off]\n"
            "  --n       number of boxes, reported every second until closed (default 10000)\n"
            "  --sweep   measure each number of boxes in turn, then exit\n"
            "  --json    write the sweep results to a JSON file\n"
            "  --postfx  bloom and SMAA (default off)\n");
    }

    std::optional<size_t> parse_object_count(const char* text)
    {
        char* end{};
        const unsigned long long n = std::strtoull(text, &end, 10);
        if (end == text || *end != '\0' || n == 0) {
            return std::nullopt;
        }
        return static_cast<size_t>(n);
    }

    // "100,1000,10000" -> { 100, 1000, 10000 }
    std::optional<std::vector<size_t>> parse_object_counts(const std::string& text)
    {
        std::vector<size_t> counts;
        size_t begin = 0;
        while (begin <= text.size()) {
            const size_t end = std::min(text.find(',', begin), text.size());
            const auto n = parse_object_count(text.substr(begin, end - begin).c_str());
            if (!n) {
                return std::nullopt;
            }
            counts.push_back(*n);
            begin = end + 1;
        }
        return counts;
    }

    std::optional<bench_options> parse_args(const int argc, char** argv)
    {
        bench_options opts;
        for (int i = 1; i < argc; ++i) {
            const bool has_value = i + 1 < argc;
            if (std::strcmp(argv[i], "--n") == 0 && has_value) {
                const auto n = parse_object_count(argv[++i]);
                if (!n) {
                    return std::nullopt;
                }
                opts.object_count = *n;
            }
            else if (std::strcmp(argv[i], "--sweep") == 0 && has_value) {
                auto counts = parse_object_counts(argv[++i]);
                if (!counts) {
                    return std::nullopt;
                }
                opts.sweep_object_counts = std::move(*counts);
            }
            else if (std::strcmp(argv[i], "--json") == 0 && has_value) {
                opts.json_path = argv[++i];
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
        if (!opts.json_path.empty() && opts.sweep_object_counts.empty()) {
            return std::nullopt; // only a sweep has one result per object count to write
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

    struct stage_stats {
        double p50_ms{ std::numeric_limits<double>::quiet_NaN() };
        double max_ms{ std::numeric_limits<double>::quiet_NaN() };
    };

    // Median and maximum of a stage over the records with a frame ID in `[first_id, last_id]`; `NaN` when there is none.
    template <class _Profiler>
    stage_stats get_stage_stats(const _Profiler& profiler, const size_t stage, const uint64_t first_id, const uint64_t last_id,
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
        if (samples.empty()) {
            return {};
        }
        return stage_stats{ quantile(samples, 0.50), *std::max_element(samples.begin(), samples.end()) };
    }

    struct pass_summary {
        std::string name;
        stage_stats cpu;
        stage_stats gpu;
    };

    // Timings of the frames with an ID in a closed range.
    struct frames_summary {
        size_t frames{};
        double frame_p50_ms{};
        double frame_p95_ms{};
        double scene_render_p50_ms{};
        double swap_p50_ms{};
        std::vector<pass_summary> passes; // passes skipped in all of the frames are left out
        size_t gpu_dropped_frames{};
    };

#if defined(TRIENGINE_DEBUG_MODE)
    // `GLCall()` calls of one frame.
    struct gl_call_counts {
        uint64_t total{};
        uint64_t uniform{}; // glProgramUniform*
        uint64_t draw{}; // glDraw*
        uint64_t bind_vertex_array{};
    };
#endif

    struct sweep_result {
        size_t object_count{};
        frames_summary frames;
#if defined(TRIENGINE_DEBUG_MODE)
        gl_call_counts gl_calls;
#endif
    };

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

            _scene = _vis->add_scene();
            auto& render_config = *_scene->get_render_config();
            render_config.enable_anti_aliasing = _opts.postfx;
            render_config.light_opts.bloom.enabled = _opts.postfx;

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

            std::printf("postfx=%s\n", _opts.postfx ? "on" : "off");
            if (this->_is_sweep()) {
                this->_start_sweep_step();
            } else {
                populate_scene(*_scene, _opts.object_count);
                std::printf("objects=%zu\n", _opts.object_count);
            }
        }

        void destroy()
        {
            _scene.reset();
            _vis->destroy_window();
            _vis.reset();
        }

        void run()
        {
            while (_vis->update_window()) {
                _vis->render();
                if (this->_is_sweep()) {
                    this->_step_sweep();
                } else {
                    this->_report();
                }
            }
            if (!_opts.json_path.empty()) {
                this->_write_json(); // also when the window was closed before the sweep finished
            }
        }

    private:
        bool _is_sweep() const noexcept { return !_opts.sweep_object_counts.empty(); }

        // Rebuilds the scene with the next object count and starts its warm-up.
        void _start_sweep_step()
        {
            const size_t object_count = _opts.sweep_object_counts[_sweep_index];
            _scene->clear_geometries();
            populate_scene(*_scene, object_count);

            _sweep_phase = sweep_phase::warmup;
            _sweep_phase_started = bench_clock::now();
        }

        // Called after every frame: warm-up, measurement, then waiting for the GPU results of the measured frames.
        void _step_sweep()
        {
            const auto& profiler = _vis->get_cpu_frame_profiler();
            if (profiler.record_count() == 0) {
                return;
            }
            const uint64_t newest_frame_id = profiler.get_record(profiler.record_count() - 1).frame_id();
            const auto now = bench_clock::now();

            switch (_sweep_phase) {
            case sweep_phase::warmup:
                if (now - _sweep_phase_started >= kSweepWarmup) {
                    _measure_first_frame_id = newest_frame_id + 1;
                    _sweep_phase = sweep_phase::measure;
                    _sweep_phase_started = now;
                }
                break;

            case sweep_phase::measure:
                if (now - _sweep_phase_started >= kSweepMeasure) {
                    _measure_last_frame_id = newest_frame_id;
                    _sweep_phase = sweep_phase::wait_gpu_results;
                    _gpu_result_wait_frames = 0;
                }
                break;

            case sweep_phase::wait_gpu_results: {
                const auto& gpu_profiler = _vis->get_pass_gpu_profiler();
                const bool arrived = gpu_profiler.record_count() > 0
                    && gpu_profiler.get_record(gpu_profiler.record_count() - 1).frame_id() >= _measure_last_frame_id;
                if (!arrived && ++_gpu_result_wait_frames < kMaxGpuResultWaitFrames) {
                    break;
                }

                sweep_result result;
                result.object_count = _opts.sweep_object_counts[_sweep_index];
                result.frames = this->_summarize(_measure_first_frame_id, _measure_last_frame_id);
                std::printf("objects=%zu\n", result.object_count);
                this->_print_summary(result.frames);
#if defined(TRIENGINE_DEBUG_MODE)
                result.gl_calls = this->_count_gl_calls();
                this->_report_gl_calls(false/* top call sites */);
#endif
                _sweep_results.push_back(std::move(result));
                if (++_sweep_index == _opts.sweep_object_counts.size()) {
                    _vis->close_window();
                } else {
                    this->_start_sweep_step();
                }
                break;
            }
            }
        }

        // Prints the frames recorded since the last report, once a second.
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

            const size_t first_index = profiler.find_first_record_after(_last_report_time);
            if (first_index == profiler.record_count()) {
                return;
            }
            const uint64_t first_frame_id = profiler.get_record(first_index).frame_id();
            const uint64_t last_frame_id = profiler.get_record(profiler.record_count() - 1).frame_id();
            _last_report_time = newest_time;

            this->_print_summary(this->_summarize(first_frame_id, last_frame_id));
#if defined(TRIENGINE_DEBUG_MODE)
            // The busiest call sites, once, after the warm-up report.
            this->_report_gl_calls(++_gl_call_reports == 2);
#endif
        }

        // Frame, scene render and swap times of the frames in `[first_frame_id, last_frame_id]`, and the per-pass
        // CPU and GPU medians of the same frames. GPU results arrive a few frames late, so the newest frames of
        // the range may have CPU values only.
        frames_summary _summarize(const uint64_t first_frame_id, const uint64_t last_frame_id)
        {
            const auto& profiler = _vis->get_cpu_frame_profiler();

            _frame_ms_samples.clear();
            _scene_render_ms_samples.clear();
            _swap_ms_samples.clear();
            for (size_t i = profiler.record_count(); i-- > 0;) {
                const auto record = profiler.get_record(i);
                if (record.frame_id() < first_frame_id) {
                    break;
                }
                if (record.frame_id() > last_frame_id) {
                    continue;
                }
                _frame_ms_samples.push_back(record.frame_ms());
                // `NaN` marks a stage that was not measured in that frame.
                if (_scene_render_stage && !std::isnan(record.stage_ms(*_scene_render_stage))) {
                    _scene_render_ms_samples.push_back(record.stage_ms(*_scene_render_stage));
                }
                if (_swap_stage && !std::isnan(record.stage_ms(*_swap_stage))) {
                    _swap_ms_samples.push_back(record.stage_ms(*_swap_stage));
                }
            }

            frames_summary summary;
            summary.frames = _frame_ms_samples.size();
            if (summary.frames == 0) {
                return summary;
            }
            summary.frame_p50_ms = quantile(_frame_ms_samples, 0.50);
            summary.frame_p95_ms = quantile(_frame_ms_samples, 0.95);
            summary.scene_render_p50_ms = _scene_render_ms_samples.empty() ? 0.0 : quantile(_scene_render_ms_samples, 0.50);
            summary.swap_p50_ms = _swap_ms_samples.empty() ? 0.0 : quantile(_swap_ms_samples, 0.50);

            const auto& cpu_profiler = _vis->get_pass_cpu_profiler();
            const auto& gpu_profiler = _vis->get_pass_gpu_profiler();
            for (size_t stage = 0; stage < cpu_profiler.stage_count(); ++stage) {
                const stage_stats cpu = get_stage_stats(cpu_profiler, stage, first_frame_id, last_frame_id, _pass_ms_samples);
                const stage_stats gpu = get_stage_stats(gpu_profiler, stage, first_frame_id, last_frame_id, _pass_ms_samples);
                if (std::isnan(cpu.p50_ms) && std::isnan(gpu.p50_ms)) {
                    continue; // pass skipped in these frames
                }
                summary.passes.push_back(pass_summary{ cpu_profiler.get_stage(stage).name, cpu, gpu });
            }
            summary.gpu_dropped_frames = gpu_profiler.dropped_frame_count();
            return summary;
        }

        static void _print_summary(const frames_summary& summary)
        {
            if (summary.frames == 0) {
                return;
            }
            std::printf("frames=%4zu  frame p50=%7.3f ms p95=%7.3f ms  scene_render p50=%7.3f ms  swap p50=%7.3f ms\n",
                summary.frames, summary.frame_p50_ms, summary.frame_p95_ms, summary.scene_render_p50_ms, summary.swap_p50_ms);

            std::printf("  pass p50 cpu/gpu ms:");
            for (const auto& pass : summary.passes) {
                std::printf("  %s %.3f/%.3f", pass.name.c_str(), pass.cpu.p50_ms, pass.gpu.p50_ms);
            }
            if (summary.gpu_dropped_frames > 0) {
                std::printf("  (gpu dropped %zu)", summary.gpu_dropped_frames);
            }
            std::printf("\n");
        }

#if defined(TRIENGINE_DEBUG_MODE)
        // GL call counting needs `TRIENGINE_ENABLE_DEBUG_MODE`, which the `-ship` presets turn off.
        // Call counts are deterministic, so the last frame stands for the whole report.
        gl_call_counts _count_gl_calls() const
        {
            const auto& gl_calls = _vis->get_last_frame_gl_calls();
            gl_call_counts counts;
            counts.total = gl_calls.total_calls();
            counts.uniform = gl_calls.calls_with_prefix("glProgramUniform");
            counts.draw = gl_calls.calls_with_prefix("glDraw");
            counts.bind_vertex_array = gl_calls.calls_with_prefix("glBindVertexArray");
            return counts;
        }

        void _report_gl_calls(const bool with_top_sites)
        {
            const gl_call_counts counts = this->_count_gl_calls();
            std::printf("  gl calls/frame: total %llu  glProgramUniform* %llu  glDraw* %llu  glBindVertexArray %llu\n",
                static_cast<unsigned long long>(counts.total),
                static_cast<unsigned long long>(counts.uniform),
                static_cast<unsigned long long>(counts.draw),
                static_cast<unsigned long long>(counts.bind_vertex_array));

            if (!with_top_sites) {
                return;
            }
            const auto& gl_calls = _vis->get_last_frame_gl_calls();
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

        // { "meta": { build and settings }, "runs": [ one object per measured object count ] }
        void _write_json() const
        {
            json meta;
            meta["debug_mode"] = kDebugMode;
            meta["postfx"] = _opts.postfx;
            meta["warmup_s"] = kSweepWarmup.count();
            meta["measure_s"] = kSweepMeasure.count();
            if (const auto* device = _vis->get_graphics_device_info()) {
                meta["device"] = device->device;
                meta["driver_version"] = device->driver_version;
            }
            meta["object_counts"] = _opts.sweep_object_counts;
            meta["complete"] = _sweep_results.size() == _opts.sweep_object_counts.size();

            json runs = json::array();
            for (const sweep_result& result : _sweep_results) {
                runs.push_back(_to_json(result));
            }

            std::ofstream file{ _opts.json_path };
            if (!file) {
                std::fprintf(stderr, "error: cannot write %s\n", _opts.json_path.c_str());
                return;
            }
            file << json{ { "meta", std::move(meta) }, { "runs", std::move(runs) } }.dump(2) << '\n';
            std::printf("wrote %s\n", _opts.json_path.c_str());
        }

        static json _to_json(const sweep_result& result)
        {
            const frames_summary& frames = result.frames;

            json passes = json::object();
            for (const pass_summary& pass : frames.passes) {
                passes[pass.name] = {
                    { "cpu_p50_ms", round_ms(pass.cpu.p50_ms) },
                    { "cpu_max_ms", round_ms(pass.cpu.max_ms) },
                    { "gpu_p50_ms", round_ms(pass.gpu.p50_ms) },
                    { "gpu_max_ms", round_ms(pass.gpu.max_ms) },
                };
            }

            json run = {
                { "objects", result.object_count },
                { "frames", frames.frames },
                { "frame_p50_ms", round_ms(frames.frame_p50_ms) },
                { "frame_p95_ms", round_ms(frames.frame_p95_ms) },
                { "scene_render_p50_ms", round_ms(frames.scene_render_p50_ms) },
                { "swap_p50_ms", round_ms(frames.swap_p50_ms) },
                { "gpu_dropped_frames", frames.gpu_dropped_frames },
                { "passes", std::move(passes) },
            };
#if defined(TRIENGINE_DEBUG_MODE)
            run["gl_calls"] = {
                { "total", result.gl_calls.total },
                { "uniform", result.gl_calls.uniform },
                { "draw", result.gl_calls.draw },
                { "bind_vertex_array", result.gl_calls.bind_vertex_array },
            };
#endif
            return run;
        }

    private:
        enum class sweep_phase { warmup, measure, wait_gpu_results };

        bench_options _opts;
        std::unique_ptr<triengine::visualization::visualizer> _vis;
        std::shared_ptr<triengine::scene> _scene;

        std::optional<size_t> _scene_render_stage;
        std::optional<size_t> _swap_stage;

        // Per-second reports
        size_t _report_epoch{};
        double _last_report_time{};
#if defined(TRIENGINE_DEBUG_MODE)
        size_t _gl_call_reports{};
#endif

        // Sweep: `_sweep_index` is the object count being measured.
        size_t _sweep_index{};
        sweep_phase _sweep_phase{ sweep_phase::warmup };
        bench_clock::time_point _sweep_phase_started{};
        uint64_t _measure_first_frame_id{};
        uint64_t _measure_last_frame_id{};
        size_t _gpu_result_wait_frames{};
        std::vector<sweep_result> _sweep_results;

        // Samples of the frames in one summary; cleared and refilled each time, kept to reuse their capacity.
        std::vector<double> _frame_ms_samples;
        std::vector<double> _scene_render_ms_samples;
        std::vector<double> _swap_ms_samples;
        std::vector<double> _pass_ms_samples;
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
