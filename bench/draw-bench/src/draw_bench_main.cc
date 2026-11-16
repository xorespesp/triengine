#include <triengine/global_options.hh>
#include <triengine/core/gpu_frame_profiler.hh>
#include <triengine/utility/gl_utils.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/math/constants.hh>
#include <triengine/visualization/visualizer.hh>

#include <GLFW/glfw3.h>
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
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    // Draw-call bound scene: many small lit boxes, each with its own transform and material,
    // seen from a fixed camera. Every run with the same options renders the same frames.
    // `--animate` spins some of the boxes in place, so their per-object data changes every frame.
    //   - ESC : close the window

    constexpr uint32_t kSceneSeed = 20261108;
    constexpr float kBoxSize = 0.2f;
    constexpr float kGridSpacing = 0.5f;
    constexpr double kAnimatedRadiansPerFrame = 0.01;
    constexpr int32_t kWindowWidth = 1280;
    constexpr int32_t kWindowHeight = 720;
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

    // Which boxes `--animate` changes, by their order in the object list. The renderer writes the per-object records
    // in that order too, so this decides the range of records that changes each frame.
    enum class animated_layout {
        contiguous, // the first boxes
        strided, // evenly spaced boxes
    };

    struct animated_options {
        size_t percent{}; // of the boxes, from 1 to 100, rounded up to a whole box
        animated_layout layout{ animated_layout::strided };
    };

    struct bench_options {
        size_t object_count{ 10000 };
        std::vector<size_t> sweep_object_counts; // empty: report every second for `object_count` until closed
        std::string json_path; // sweep results, written when the bench exits
        std::optional<size_t> monitor_index; // `nullopt`: the primary monitor
        bool postfx{ false }; // bloom and SMAA; off keeps the GPU cost small next to the submission cost
        std::optional<animated_options> animated; // `nullopt`: every box stays still
    };

    void print_usage()
    {
        std::fprintf(stderr,
            "usage: draw-bench [--n <object count> | --sweep <count,count,...> [--json <path>]]\n"
            "                  [--animate <percent> [--animate-layout contiguous|strided]] [--postfx on|off] [--monitor <index>]\n"
            "  --n               number of boxes, reported every second until closed (default 10000)\n"
            "  --sweep           measure each number of boxes in turn, then exit\n"
            "  --json            write the sweep results to a JSON file\n"
            "  --animate         spin this percent of the boxes every frame, from 1 to 100 (default: none)\n"
            "  --animate-layout  which boxes spin, in the order they were added: the first ones (contiguous)\n"
            "                    or evenly spaced ones (strided, default)\n"
            "  --postfx          bloom and SMAA (default off)\n"
            "  --monitor         open the window centered on this monitor, from 0 (default: the primary monitor)\n");
    }

    const char* to_string(const animated_layout layout)
    {
        return layout == animated_layout::contiguous ? "contiguous" : "strided";
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

    std::optional<size_t> parse_index(const char* text)
    {
        char* end{};
        const unsigned long long index = std::strtoull(text, &end, 10);
        if (end == text || *end != '\0') {
            return std::nullopt;
        }
        return static_cast<size_t>(index);
    }

    std::optional<size_t> parse_percent(const char* text)
    {
        const auto percent = parse_index(text);
        if (!percent || *percent == 0 || *percent > 100) {
            return std::nullopt;
        }
        return percent;
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
        std::optional<animated_layout> layout; // `--animate-layout`, which needs `--animate`
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
            else if (std::strcmp(argv[i], "--monitor") == 0 && has_value) {
                opts.monitor_index = parse_index(argv[++i]);
                if (!opts.monitor_index) {
                    return std::nullopt;
                }
            }
            else if (std::strcmp(argv[i], "--postfx") == 0 && has_value) {
                const std::string value{ argv[++i] };
                if (value != "on" && value != "off") {
                    return std::nullopt;
                }
                opts.postfx = value == "on";
            }
            else if (std::strcmp(argv[i], "--animate") == 0 && has_value) {
                const auto percent = parse_percent(argv[++i]);
                if (!percent) {
                    return std::nullopt;
                }
                opts.animated = animated_options{ *percent };
            }
            else if (std::strcmp(argv[i], "--animate-layout") == 0 && has_value) {
                const std::string value{ argv[++i] };
                if (value == "contiguous") {
                    layout = animated_layout::contiguous;
                } else if (value == "strided") {
                    layout = animated_layout::strided;
                } else {
                    return std::nullopt;
                }
            }
            else {
                return std::nullopt;
            }
        }
        if (!opts.json_path.empty() && opts.sweep_object_counts.empty()) {
            return std::nullopt; // only a sweep has one result per object count to write
        }
        if (layout) {
            if (!opts.animated) {
                return std::nullopt; // a layout with no boxes to spin
            }
            opts.animated->layout = *layout;
        }
        return opts;
    }

    struct window_placement {
        std::string monitor_name;
        int refresh_hz{};
        int32_t x{};
        int32_t y{};
    };

    // Centers a `width` x `height` window in the work area of monitor `index` (`nullopt`: the primary monitor).
    // Needs GLFW initialized. On a bad index, lists the monitors and throws.
    window_placement place_on_monitor(const std::optional<size_t> index, const int32_t width, const int32_t height)
    {
        int count = 0;
        GLFWmonitor* const* const monitors = ::glfwGetMonitors(&count);
        GLFWmonitor* monitor = index ? nullptr : ::glfwGetPrimaryMonitor();
        if (index && *index < static_cast<size_t>(count)) {
            monitor = monitors[*index];
        }
        if (!monitor) {
            for (int i = 0; i < count; ++i) {
                const GLFWvidmode* const mode = ::glfwGetVideoMode(monitors[i]);
                std::fprintf(stderr, "  monitor %d: %s, %d Hz%s",
                    i, ::glfwGetMonitorName(monitors[i]), mode ? mode->refreshRate : 0, i == 0 ? " (primary)\n" : "\n");
            }
            throw std::runtime_error("no such monitor");
        }

        int32_t area_x{}, area_y{}, area_width{}, area_height{};
        ::glfwGetMonitorWorkarea(monitor, &area_x, &area_y, &area_width, &area_height);
        const GLFWvidmode* const mode = ::glfwGetVideoMode(monitor);
        const char* const name = ::glfwGetMonitorName(monitor);

        window_placement placement;
        placement.monitor_name = name ? name : "";
        placement.refresh_hz = mode ? mode->refreshRate : 0;
        placement.x = area_x + (area_width - width) / 2;
        placement.y = area_y + (area_height - height) / 2;
        return placement;
    }

    // A box that `--animate` spins in place about its initial rotation axis.
    struct animated_object {
        std::shared_ptr<triengine::geometry::mesh_object> object;
        triengine::vec3_f32 axis;
        float initial_angle{};
    };

    // Index in the object list of animated box `j`, out of `animated_count` boxes among `object_count`.
    // Increases with `j`, so the boxes can be picked while the list is built.
    size_t get_animated_index(const size_t j, const size_t object_count, const size_t animated_count, const animated_layout layout)
    {
        return layout == animated_layout::contiguous ? j : j * object_count / animated_count;
    }

    // Lays the boxes out on a cube-shaped grid centered at the origin, and returns the ones `animated` picks.
    std::vector<animated_object> populate_scene(triengine::scene& scn, const size_t object_count,
        const std::optional<animated_options>& animated)
    {
        const size_t side = static_cast<size_t>(std::ceil(std::cbrt(static_cast<double>(object_count))));
        const float half_extent = 0.5f * kGridSpacing * static_cast<float>(side - 1);

        std::mt19937 rng{ kSceneSeed };
        std::uniform_real_distribution<float> unit{ 0.0f, 1.0f };
        std::uniform_real_distribution<float> signed_unit{ -1.0f, 1.0f };

        const size_t animated_count = animated ? (object_count * animated->percent + 99) / 100 : 0;
        std::vector<animated_object> animated_objects;
        animated_objects.reserve(animated_count);

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
            const float angle = unit(rng) * 3.14159265f;
            box->rotate(triengine::quat_f32{ Eigen::AngleAxisf{ angle, axis } });
            box->translate(triengine::vec3_f32{
                kGridSpacing * static_cast<float>(x) - half_extent,
                kGridSpacing * static_cast<float>(y) - half_extent,
                kGridSpacing * static_cast<float>(z) - half_extent });

            if (animated_objects.size() < animated_count
                && i == get_animated_index(animated_objects.size(), object_count, animated_count, animated->layout)) {
                animated_objects.push_back(animated_object{ box, axis, angle });
            }
            scn.add_geometry(box);
        }

        scn.switch_camera_type(triengine::camera_type::arcball);
        auto& cam = *scn.get_camera()->as<triengine::arcball_camera>();
        cam.set_pivot_point(triengine::vec3_f32{ 0.0f, 0.0f, 0.0f });
        cam.set_yaw(30.0f);
        cam.set_pitch(25.0f);
        cam.set_zoom_distance(1.0f + 4.6f * half_extent); // fits the grid's bounding sphere in the 45 deg vertical fov
        return animated_objects;
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

    struct frame_stage_summary {
        std::string name;
        stage_stats cpu;
    };

    // Timings of the frames with an ID in a closed range.
    struct frames_summary {
        size_t frames{};
        double frame_p50_ms{};
        double frame_p95_ms{};
        double scene_render_p50_ms{};
        double swap_p50_ms{};
        // `visualizer` stages, then "Untracked": the part of each frame outside every stage.
        std::vector<frame_stage_summary> frame_stages;
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
        uint64_t named_buffer_sub_data{};
    };
#endif

    struct sweep_result {
        size_t object_count{};
        size_t animated_object_count{};
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
            // Created hidden and shown once placed, so the window never appears on another monitor first.
            _vis->create_window("Triengine Draw Bench", false/* show_window */, kWindowWidth, kWindowHeight,
                false/* fullscreen */, false/* enable_vsync */, 0/* max_fps: uncapped */);
            _placement = place_on_monitor(_opts.monitor_index, kWindowWidth, kWindowHeight);
            _vis->set_window_position(_placement.x, _placement.y);
            _vis->set_window_visible(true);
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

            const std::string animate = _opts.animated
                ? std::to_string(_opts.animated->percent) + "% (" + to_string(_opts.animated->layout) + ")"
                : "off";
            std::printf("monitor=%s (%d Hz) postfx=%s animate=%s\n",
                _placement.monitor_name.c_str(), _placement.refresh_hz, _opts.postfx ? "on" : "off", animate.c_str());
            if (this->_is_sweep()) {
                this->_start_sweep_step();
            } else {
                _animated_objects = populate_scene(*_scene, _opts.object_count, _opts.animated);
                std::printf("objects=%zu animated=%zu\n", _opts.object_count, _animated_objects.size());
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
                this->_animate_objects();
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

        // Spins the animated boxes in place, each about its initial rotation axis. The angle depends only on
        // the number of frames since the scene was built, so every run with the same options renders the same frames.
        void _animate_objects()
        {
            if (_animated_objects.empty()) {
                return;
            }
            const float advance = static_cast<float>(
                std::fmod(kAnimatedRadiansPerFrame * static_cast<double>(_animated_frames++), 2.0 * triengine::math::kPi));
            for (const animated_object& animated : _animated_objects) {
                animated.object->rotate(triengine::quat_f32{ Eigen::AngleAxisf{ animated.initial_angle + advance, animated.axis } });
            }
        }

        // Rebuilds the scene with the next object count and starts its warm-up.
        void _start_sweep_step()
        {
            const size_t object_count = _opts.sweep_object_counts[_sweep_index];
            _scene->clear_geometries();
            _animated_objects = populate_scene(*_scene, object_count, _opts.animated);
            _animated_frames = 0;

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
                result.animated_object_count = _animated_objects.size();
                result.frames = this->_summarize(_measure_first_frame_id, _measure_last_frame_id);
                std::printf("objects=%zu animated=%zu\n", result.object_count, result.animated_object_count);
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

            for (size_t stage = 0; stage < profiler.stage_count(); ++stage) {
                const stage_stats cpu = get_stage_stats(profiler, stage, first_frame_id, last_frame_id, _pass_ms_samples);
                if (!std::isnan(cpu.p50_ms)) {
                    summary.frame_stages.push_back(frame_stage_summary{ profiler.get_stage(stage).name, cpu });
                }
            }
            summary.frame_stages.push_back(frame_stage_summary{ "Untracked", this->_untracked_stats(first_frame_id, last_frame_id) });

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

        // Frame time minus the sum of the measured stages, per frame.
        stage_stats _untracked_stats(const uint64_t first_frame_id, const uint64_t last_frame_id)
        {
            const auto& profiler = _vis->get_cpu_frame_profiler();
            _pass_ms_samples.clear();
            for (size_t i = profiler.record_count(); i-- > 0;) {
                const auto record = profiler.get_record(i);
                if (record.frame_id() < first_frame_id) {
                    break;
                }
                if (record.frame_id() > last_frame_id) {
                    continue;
                }
                double untracked_ms = record.frame_ms();
                for (size_t stage = 0; stage < profiler.stage_count(); ++stage) {
                    const double ms = record.stage_ms(stage);
                    if (!std::isnan(ms)) {
                        untracked_ms -= ms;
                    }
                }
                _pass_ms_samples.push_back(untracked_ms);
            }
            if (_pass_ms_samples.empty()) {
                return {};
            }
            return stage_stats{ quantile(_pass_ms_samples, 0.50), *std::max_element(_pass_ms_samples.begin(), _pass_ms_samples.end()) };
        }

        static void _print_summary(const frames_summary& summary)
        {
            if (summary.frames == 0) {
                return;
            }
            std::printf("frames=%4zu  frame p50=%7.3f ms p95=%7.3f ms  scene_render p50=%7.3f ms  swap p50=%7.3f ms\n",
                summary.frames, summary.frame_p50_ms, summary.frame_p95_ms, summary.scene_render_p50_ms, summary.swap_p50_ms);

            std::printf("  frame stage p50/max ms:");
            for (const auto& stage : summary.frame_stages) {
                std::printf("  %s %.3f/%.3f", stage.name.c_str(), stage.cpu.p50_ms, stage.cpu.max_ms);
            }
            std::printf("\n");

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
            counts.named_buffer_sub_data = gl_calls.calls_with_prefix("glNamedBufferSubData");
            return counts;
        }

        void _report_gl_calls(const bool with_top_sites)
        {
            const gl_call_counts counts = this->_count_gl_calls();
            std::printf("  gl calls/frame: total %llu  glProgramUniform* %llu  glDraw* %llu  glBindVertexArray %llu  glNamedBufferSubData %llu\n",
                static_cast<unsigned long long>(counts.total),
                static_cast<unsigned long long>(counts.uniform),
                static_cast<unsigned long long>(counts.draw),
                static_cast<unsigned long long>(counts.bind_vertex_array),
                static_cast<unsigned long long>(counts.named_buffer_sub_data));

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
            meta["animated"] = _opts.animated
                ? json{ { "percent", _opts.animated->percent }, { "layout", to_string(_opts.animated->layout) } }
                : json(nullptr);
            meta["monitor"] = { { "name", _placement.monitor_name }, { "refresh_hz", _placement.refresh_hz } };
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

            json frame_stages = json::object();
            for (const frame_stage_summary& stage : frames.frame_stages) {
                frame_stages[stage.name] = {
                    { "p50_ms", round_ms(stage.cpu.p50_ms) },
                    { "max_ms", round_ms(stage.cpu.max_ms) },
                };
            }

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
                { "animated_objects", result.animated_object_count },
                { "frames", frames.frames },
                { "frame_p50_ms", round_ms(frames.frame_p50_ms) },
                { "frame_p95_ms", round_ms(frames.frame_p95_ms) },
                { "scene_render_p50_ms", round_ms(frames.scene_render_p50_ms) },
                { "swap_p50_ms", round_ms(frames.swap_p50_ms) },
                { "gpu_dropped_frames", frames.gpu_dropped_frames },
                { "frame_stages", std::move(frame_stages) },
                { "passes", std::move(passes) },
            };
#if defined(TRIENGINE_DEBUG_MODE)
            run["gl_calls"] = {
                { "total", result.gl_calls.total },
                { "uniform", result.gl_calls.uniform },
                { "draw", result.gl_calls.draw },
                { "bind_vertex_array", result.gl_calls.bind_vertex_array },
                { "named_buffer_sub_data", result.gl_calls.named_buffer_sub_data },
            };
#endif
            return run;
        }

    private:
        enum class sweep_phase { warmup, measure, wait_gpu_results };

        bench_options _opts;
        std::unique_ptr<triengine::visualization::visualizer> _vis;
        std::shared_ptr<triengine::scene> _scene;
        window_placement _placement;

        // Boxes spun every frame, in the object list order
        std::vector<animated_object> _animated_objects;
        uint64_t _animated_frames{}; // frames animated since the scene was built

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
