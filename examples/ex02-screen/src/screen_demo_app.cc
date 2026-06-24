#include "screen_demo_app.hh"

#include <xutl/debug/logger.hh>

#include <triengine/gui/iwindow.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/image_buffer.hh>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <condition_variable>
#include <execution>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

namespace demo
{
    namespace
    {
        enum class pattern_type {
            checkerboard = 0,
            plasma,
        };

        // Fixed square texture size for the 3D quad: a 2x2 square quad needs a square
        // texture to keep pattern cells square (2D mode uses a frame-sized texture).
        constexpr int32_t kScreen3dTexSize = 512;

        // Fixed 16:9 texture size for the screen_overlay mode. The background image
        // keeps this resolution regardless of the window size; the renderer fits it to the
        // viewport via scene_render_config::bg_image_fit, so a non-square size makes the
        // cover/contain/stretch differences visible.
        constexpr int32_t kBackgroundTexWidth = 640;
        constexpr int32_t kBackgroundTexHeight = 360;

        // Angular speed of the overlay mesh rotation, in degrees per second.
        constexpr float kOverlayRotSpeedDegPerSec = 45.0f;

        // Distance of the overlay box in front of the camera, along the camera-frame +z axis.
        // The box must sit at positive z so the calibrated pinhole camera (at the origin looking
        // down +z) has it in front of the near plane; the orbit camera frames the same point.
        constexpr float kOverlayBoxDepth = 4.0f;

        // Image-generation parameters (copied by value to the worker thread).
        struct pattern_params {
            pattern_type pattern{ pattern_type::checkerboard };
            float animate_speed{ 0.5f };   // animation offset increment per frame, in pixels
            int32_t feature_size{ 128 };   // checker cell size / plasma scale, in pixels
            triengine::color3_f32 tint{ 1.0f, 1.0f, 1.0f }; // RGB tint multiplier
        };

        // Fills `img` with the selected pattern. Rows are independent and filled in
        // parallel; `row_indices` is the caller-cached [0, height) range.
        void fill_pattern_rows(
            triengine::image_buffer& img,
            const pattern_params& p,
            const float animate_offset,
            const std::vector<int32_t>& row_indices)
        {
            uint8_t* const base = img.data();
            const int32_t w = img.width_pixels();
            const uint32_t stride = img.stride_bytes();

            const int32_t fs = (p.feature_size > 0) ? p.feature_size : 1;
            const int32_t shift = static_cast<int32_t>(animate_offset);
            const pattern_type pattern = p.pattern;

            const float tr = p.tint.r();
            const float tg = p.tint.g();
            const float tb = p.tint.b();

            std::for_each(std::execution::par, row_indices.begin(), row_indices.end(),
                [=](const int32_t y)
                {
                    const auto put = [&](uint8_t* px, float r, float g, float b) {
                        px[0] = static_cast<uint8_t>(std::min(std::max(r * tr, 0.0f), 1.0f) * 255.0f);
                        px[1] = static_cast<uint8_t>(std::min(std::max(g * tg, 0.0f), 1.0f) * 255.0f);
                        px[2] = static_cast<uint8_t>(std::min(std::max(b * tb, 0.0f), 1.0f) * 255.0f);
                    };

                    uint8_t* const row = base + static_cast<size_t>(y) * stride;
                    for (int32_t x = 0; x < w; ++x) {
                        uint8_t* const px = row + static_cast<size_t>(x) * 3;

                        switch (pattern) {
                        case pattern_type::checkerboard: {
                            const int32_t cx = (x + shift) / fs;
                            const int32_t cy = y / fs;
                            const bool on = ((cx + cy) & 1) == 0;
                            const float v = on ? 1.0f : 0.0f;
                            put(px, v, v, v);
                            break;
                        }
                        case pattern_type::plasma: {
                            const float fx = static_cast<float>(x) / static_cast<float>(fs);
                            const float fy = static_cast<float>(y) / static_cast<float>(fs);
                            const float ph = animate_offset * 0.05f;
                            const float r = 0.5f + 0.5f * std::sin(fx + ph);
                            const float g = 0.5f + 0.5f * std::sin(fy - ph);
                            const float b = 0.5f + 0.5f * std::sin((fx + fy) * 0.5f + ph);
                            put(px, r, g, b);
                            break;
                        }
                        } // switch
                    }
                });
        }

        // ImGui controls owning the image-generation parameters (UI thread only).
        class pattern_controls
        {
        public:
            void render_controls()
            {
                ImGui::TextUnformatted("Image Pattern");
                const char* const pattern_names[] = {
                    "Checkerboard", "Plasma"
                };
                int pattern = static_cast<int>(_p.pattern);
                ImGui::Combo("Pattern", &pattern, pattern_names, IM_ARRAYSIZE(pattern_names));
                _p.pattern = static_cast<pattern_type>(pattern);

                ImGui::SliderFloat("Animate speed", &_p.animate_speed, 0.0f, 16.0f, "%.1f px/frame");
                ImGui::SliderInt("Feature size", &_p.feature_size, 1, 128, "%d px");
                ImGui::ColorEdit3("Tint Color", _p.tint.data());
            }

            const pattern_params& params() const noexcept { return _p; }

        private:
            pattern_params _p;
        }; // class

        // Builds a 2x2 texture-shaded quad on the z=0 plane, centered at the origin.
        // Lighting (lit vs unlit) is left to the caller via set_lighting_mode().
        std::shared_ptr<triengine::geometry::mesh_object> _create_screen_quad_mesh(
            const triengine::texture_handle_t diffuse_map,
            const triengine::texture_handle_t specular_map)
        {
            auto quad = std::make_shared<triengine::geometry::mesh_object>(
                triengine::geometry::mesh_object::shading_mode::texture);
            quad->set_name("screen_quad");

            quad->vertex_positions = {
                { -1.0f, -1.0f, 0.0f }, // bottom-left
                {  1.0f, -1.0f, 0.0f }, // bottom-right
                {  1.0f,  1.0f, 0.0f }, // top-right
                { -1.0f,  1.0f, 0.0f }, // top-left
            };
            quad->vertex_normals = {
                { 0.0f, 0.0f, 1.0f },
                { 0.0f, 0.0f, 1.0f },
                { 0.0f, 0.0f, 1.0f },
                { 0.0f, 0.0f, 1.0f },
            };
            quad->vertex_uvs = {
                { 0.0f, 0.0f },
                { 1.0f, 0.0f },
                { 1.0f, 1.0f },
                { 0.0f, 1.0f },
            };
            quad->triangle_indices = {
                { 0, 1, 2 },
                { 0, 2, 3 },
            };

            auto* const mat = quad->get_texture_shading_material();
            mat->diffuse_map = diffuse_map;
            mat->specular_map = specular_map;

            return quad;
        }

        // 3D mode: an ordinary 3D scene with the usual options on.
        void _init_screen3d_scene(triengine::scene& scn)
        {
            auto& cfg = *scn.get_render_config();
            cfg.show_origin_xz_grid = true;

            scn.add_geometry(triengine::geometry::mesh_object::create_coordinate_frame(0.5f)); // origin frame
        }

        // 2D mode: disable options a flat screen does not need, and view the quad head-on
        // with an orthographic camera. The quad is unlit, so it shows the texture as-is.
        void _init_screen2d_scene(triengine::scene& scn)
        {
            auto& cfg = *scn.get_render_config();
            cfg.bg_color = triengine::color4_f32{ 0.0f, 0.0f, 0.0f, 1.0f };
            cfg.enable_anti_aliasing = false;
            cfg.show_object_normals = false;
            cfg.show_origin_xz_grid = false;
            cfg.light_opts.dir_light.enabled = false;
            cfg.light_opts.point_light.enabled = false;
            cfg.light_opts.point_light.show_light_source = false;
            cfg.light_opts.simple_fog.enabled = false;
            cfg.light_opts.bloom.enabled = false;
            cfg.light_opts.hdr.enabled = false;

            // Orthographic camera on the +z axis (yaw 90, pitch 0) looking at the quad;
            // view height 2 fits the 2x2 quad vertically.
            scn.switch_camera_type(triengine::camera_type::ortho);
            if (auto* ortho_cam = scn.get_camera()->as<triengine::ortho_camera>()) {
                ortho_cam->set_pivot_point(triengine::vec3_f32{ 0.0f, 0.0f, 0.0f });
                ortho_cam->set_yaw(90.0f);
                ortho_cam->set_pitch(0.0f);
                ortho_cam->set_ortho_view_height(2.0f);
            }
        }

        // screen_overlay mode: a perspective 3D scene whose background is filled with the
        // streamed image, with a lit 3D mesh drawn over it. The bg_image handle is assigned by
        // the app once the texture exists; here only the handle-independent config is set.
        void _init_screen_overlay_scene(triengine::scene& scn)
        {
            auto& cfg = *scn.get_render_config();
            cfg.bg_image_fit = triengine::background_fit_mode::contain;
            cfg.show_object_normals = false;
            cfg.show_origin_xz_grid = false;
            cfg.light_opts.dir_light.enabled = true;
            cfg.light_opts.point_light.enabled = true;
            cfg.light_opts.point_light.show_light_source = true;
            cfg.light_opts.simple_fog.enabled = false;
            cfg.light_opts.bloom.enabled = true;
            cfg.light_opts.hdr.enabled = true;

            // Orbit (arcball) view frames the overlay box, which sits in front along +z. The
            // pinhole camera (toggled at runtime) ignores this and views from the origin.
            if (auto* arc = scn.get_camera()->as<triengine::arcball_camera>()) {
                arc->set_pivot_point(triengine::vec3_f32{ 0.0f, 0.0f, kOverlayBoxDepth });
            }
        }

    } // namespace

    // Generates animated CPU images on a worker thread and hands the latest finished frame
    // to the render thread. A small image_buffer pool cycles between worker and consumer:
    // the worker fills one (rows in parallel), publishes it as the single "ready" frame
    // (latest-wins), then waits until the consumer takes it before producing the next, so
    // production is paced to the consumer. Touches only CPU buffers; no GL/scene calls.
    class async_pattern_producer
    {
    public:
        async_pattern_producer() = default;
        ~async_pattern_producer() { stop(); }

        async_pattern_producer(const async_pattern_producer&) = delete;
        async_pattern_producer& operator=(const async_pattern_producer&) = delete;

        // Launch the worker thread, generating at the given size/format.
        void start(
            const int32_t width,
            const int32_t height,
            const triengine::image_format_type format,
            const pattern_params& params)
        {
            {
                std::scoped_lock lk{ _mtx };
                _width = width;
                _height = height;
                _format = format;
                _params = params;
                _running = true;
            }
            _worker = std::thread{ [this] { _run(); } };
        }

        // Signal the worker to exit and join it. Safe to call repeatedly.
        void stop()
        {
            {
                std::scoped_lock lk{ _mtx };
                if (!_running) { return; }
                _running = false;
            }
            _cv.notify_all();
            if (_worker.joinable()) { _worker.join(); }

            std::scoped_lock lk{ _mtx };
            _ready.reset();
            _free.clear();
        }

        // Update the generation parameters (applied to the next frame).
        void set_params(const pattern_params& params)
        {
            std::scoped_lock lk{ _mtx };
            _params = params;
        }

        // Change the generated image size/format. Frames already produced at the old size
        // are dropped by the consumer's size check.
        void set_target_size(
            const int32_t width,
            const int32_t height,
            const triengine::image_format_type format)
        {
            {
                std::scoped_lock lk{ _mtx };
                if (width == _width && height == _height && format == _format) { return; }
                _width = width;
                _height = height;
                _format = format;
            }
            _cv.notify_all();
        }

        // Take the latest finished frame, or nullptr if none is ready. Freeing the slot
        // lets the worker produce the next frame.
        std::shared_ptr<triengine::image_buffer> acquire_latest()
        {
            std::shared_ptr<triengine::image_buffer> out;
            {
                std::scoped_lock lk{ _mtx };
                out = std::move(_ready);
                _ready.reset();
            }
            if (out) { _cv.notify_all(); }
            return out;
        }

        // Return a consumed frame to the pool. Call only after the render() that used its
        // upload has returned.
        void recycle(std::shared_ptr<triengine::image_buffer> buf)
        {
            if (!buf) { return; }
            {
                std::scoped_lock lk{ _mtx };
                _free.push_back(std::move(buf));
            }
            _cv.notify_all();
        }

    private:
        void _run()
        {
            float animate_offset = 0.0f;
            std::vector<int32_t> row_indices;
            std::shared_ptr<triengine::image_buffer> work;

            for (;;)
            {
                pattern_params params;
                int32_t width{}, height{};
                triengine::image_format_type format{};
                {
                    std::unique_lock lk{ _mtx };
                    // Wait until the previous frame is consumed, pacing production to the
                    // consumer instead of busy-spinning.
                    _cv.wait(lk, [this] { return !_running || _ready == nullptr; });
                    if (!_running) { break; }

                    params = _params;
                    width = _width;
                    height = _height;
                    format = _format;

                    if (!_free.empty()) {
                        work = std::move(_free.back());
                        _free.pop_back();
                    }
                }

                if (!work) {
                    work = std::make_shared<triengine::image_buffer>();
                }
                if (work->width_pixels() != width ||
                    work->height_pixels() != height ||
                    work->format() != format) {
                    work->prepare(width, height, format);
                }
                if (static_cast<int32_t>(row_indices.size()) != height) {
                    row_indices.resize(static_cast<size_t>(height));
                    std::iota(row_indices.begin(), row_indices.end(), 0);
                }

                animate_offset += params.animate_speed;
                fill_pattern_rows(*work, params, animate_offset, row_indices);

                {
                    std::scoped_lock lk{ _mtx };
                    if (_ready) { _free.push_back(std::move(_ready)); } // latest-wins: drop the older frame
                    _ready = std::move(work); // work becomes null; next iteration grabs a fresh buffer
                }
            }
        }

    private:
        std::mutex _mtx;
        std::condition_variable _cv;
        std::thread _worker;
        bool _running{ false };

        int32_t _width{ 0 }, _height{ 0 };
        triengine::image_format_type _format{ triengine::image_format_type::rgb };
        pattern_params _params{};

        std::shared_ptr<triengine::image_buffer> _ready; // latest finished frame
        std::vector<std::shared_ptr<triengine::image_buffer>> _free; // recycled buffers
    }; // class

    // Control window: owns the image-pattern controls and the screen-mode selector, and
    // edits per-mode options (3D quad material, background fit mode, overlay rotation).
    class screen_control_window
        : public triengine::gui::iwindow
    {
    public:
        const char* get_window_name() const override {
            return "Screen Control Window";
        }

        ImVec2 get_initial_window_size() const override {
            return ImVec2{ 360.0f, 460.0f };
        }

        void render(
            [[maybe_unused]] const triengine::gui::window_render_context& render_ctx) override
        {
            // Cycle through the screen modes with prev/next arrows (wraps at both ends).
            constexpr int mode_count = static_cast<int>(screen_mode::count);
            int mode_idx = static_cast<int>(_curr_screen_mode);

            ImGui::Text("Screen Mode: %s", _mode_name(_curr_screen_mode));
            if (ImGui::Button("<")) { mode_idx = (mode_idx + mode_count - 1) % mode_count; }
            ImGui::SameLine();
            if (ImGui::Button(">")) { mode_idx = (mode_idx + 1) % mode_count; }
            _curr_screen_mode = static_cast<screen_mode>(mode_idx);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Show only the quad controls that apply to the current mode.
            if (_curr_screen_mode == screen_mode::screen_3d && _quad3d) {
                // 3D mode: full material (lighting, phong-when-lit, alpha).
                ImGui::TextUnformatted("3D Screen Quad");

                using lighting_mode = triengine::geometry::mesh_object::lighting_mode;
                auto* const mat = _quad3d->get_texture_shading_material();

                const char* const lighting_items[] = { "Lit", "Unlit" };
                int lighting_idx = (_quad3d->get_lighting_mode() == lighting_mode::unlit) ? 1 : 0;
                if (ImGui::Combo("Lighting", &lighting_idx, lighting_items, IM_ARRAYSIZE(lighting_items))) {
                    _quad3d->set_lighting_mode(lighting_idx == 1 ? lighting_mode::unlit : lighting_mode::lit);
                }

                // Phong params apply only when lit.
                if (_quad3d->get_lighting_mode() == lighting_mode::lit) {
                    ImGui::DragFloat("Ambient", &mat->ambient_intensity, 0.01f, 0.0f, 4.0f);
                    ImGui::DragFloat("Diffuse", &mat->diffuse_intensity, 0.01f, 0.0f, 4.0f);
                    ImGui::DragFloat("Specular", &mat->specular_intensity, 0.01f, 0.0f, 4.0f);
                    int shininess = static_cast<int>(mat->shininess);
                    if (ImGui::SliderInt("Shininess", &shininess, 1, 256)) {
                        mat->shininess = static_cast<uint16_t>(shininess);
                    }
                }

                // Alpha < 1.0 routes the quad through the WBOIT transparent pass.
                ImGui::SliderFloat("Alpha", &mat->alpha, 0.0f, 1.0f, "%.2f");
            } else if (_curr_screen_mode == screen_mode::screen_2d) {
                // 2D mode: a fixed flat, unlit, fullscreen screen has no material options.
                ImGui::TextUnformatted("2D Screen Quad");
                ImGui::TextDisabled("Flat unlit fullscreen screen.\nNo material options in this mode.");
            } else {
                // screen_overlay mode: pick the camera (free orbit vs calibrated pinhole), the
                // background fit, and the overlay mesh spin speed.
                ImGui::TextUnformatted("Screen Image + Overlay");

                ImGui::Checkbox("Calibrated (pinhole) camera", &_overlay_use_pinhole);

                // Background fit. With the pinhole camera, only 'contain' keeps the overlay aligned
                // (the camera letterboxes its projection the same way).
                const char* const fit_items[] = { "Stretch", "Cover", "Contain" };
                int fit_idx = static_cast<int>(_bg_fit_mode);
                if (ImGui::Combo("Background Fit", &fit_idx, fit_items, IM_ARRAYSIZE(fit_items))) {
                    _bg_fit_mode = static_cast<triengine::background_fit_mode>(fit_idx);
                }

                if (_overlay_use_pinhole) {
                    ImGui::SliderFloat("fx", &_overlay_intrinsics.fx, 100.0f, 1200.0f, "%.0f px");
                    ImGui::SliderFloat("fy", &_overlay_intrinsics.fy, 100.0f, 1200.0f, "%.0f px");
                    ImGui::SliderFloat("cx", &_overlay_intrinsics.cx,
                        0.0f, static_cast<float>(_overlay_intrinsics.image_width), "%.0f px");
                    ImGui::SliderFloat("cy", &_overlay_intrinsics.cy,
                        0.0f, static_cast<float>(_overlay_intrinsics.image_height), "%.0f px");
                } else {
                    ImGui::TextDisabled("Arcball: drag to orbit the box.");
                }

                ImGui::SliderFloat("Rotation Speed", &_overlay_rot_speed, 0.0f, 180.0f, "%.0f deg/s");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            _img_pat_ctrl.render_controls();
        }

        screen_mode mode() const noexcept {
            return _curr_screen_mode;
        }

        // The 3D quad whose material this window edits.
        void set_target_quad(std::shared_ptr<triengine::geometry::mesh_object> quad) {
            _quad3d = std::move(quad);
        }

        // Snapshot of the current pattern parameters (fed to the producer).
        pattern_params current_params() const {
            return _img_pat_ctrl.params();
        }

        // Selected background fit mode for the screen_overlay scene.
        triengine::background_fit_mode bg_fit_mode() const noexcept {
            return _bg_fit_mode;
        }

        // Overlay mesh rotation speed, in degrees per second.
        float overlay_rot_speed() const noexcept {
            return _overlay_rot_speed;
        }

        // Whether the screen_overlay scene should use the calibrated pinhole camera.
        bool overlay_use_pinhole() const noexcept {
            return _overlay_use_pinhole;
        }

        // Current pinhole intrinsics edited by the sliders.
        const triengine::pinhole_camera::intrinsics_t& overlay_intrinsics() const noexcept {
            return _overlay_intrinsics;
        }

        // Seed the pinhole intrinsics (image size + sensible defaults) once at startup.
        void set_overlay_intrinsics(const triengine::pinhole_camera::intrinsics_t& intrinsics) {
            _overlay_intrinsics = intrinsics;
        }

    private:
        static const char* _mode_name(screen_mode m) noexcept {
            switch (m) {
            case screen_mode::screen_3d:      return "3D Quad";
            case screen_mode::screen_2d:      return "2D Screen";
            case screen_mode::screen_overlay: return "Image Overlay";
            default:                          return "";
            }
        }

    private:
        pattern_controls _img_pat_ctrl;
        std::shared_ptr<triengine::geometry::mesh_object> _quad3d;
        screen_mode _curr_screen_mode{ screen_mode::screen_3d };
        triengine::background_fit_mode _bg_fit_mode{ triengine::background_fit_mode::contain };
        float _overlay_rot_speed{ kOverlayRotSpeedDegPerSec };
        bool _overlay_use_pinhole{ false };
        triengine::pinhole_camera::intrinsics_t _overlay_intrinsics{};
    }; // class

    screen_demo_app::screen_demo_app() = default;
    screen_demo_app::~screen_demo_app() = default;

    void screen_demo_app::create()
    {
        XUTL_TRACE("{}() ENTER", __func__);

        _vis = std::make_unique<triengine::visualization::visualizer>();
        _vis->create_window("Triengine Screen Rendering Demo"
            " (Build: " __DATE__ ", " __TIME__
#if defined (_DEBUG)
            " DBG"
#else  // ^^^ _DEBUG ^^^ / vvv !_DEBUG vvv
            " REL"
#endif // ^^^ !_DEBUG ^^^
            ")",
            true,
            -1, -1, // default window size
            false, // fullscreen
            true // enable vsync
        );

        // One scene per screen mode. The first one added becomes active, matching the
        // default mode (screen_3d).
        _scenes[static_cast<size_t>(screen_mode::screen_3d)] = _vis->add_scene();
        _scenes[static_cast<size_t>(screen_mode::screen_2d)] = _vis->add_scene();
        _scenes[static_cast<size_t>(screen_mode::screen_overlay)] = _vis->add_scene();

        auto& screen3d_scene = *_scenes[static_cast<size_t>(screen_mode::screen_3d)];
        auto& screen2d_scene = *_scenes[static_cast<size_t>(screen_mode::screen_2d)];
        auto& overlay_scene = *_scenes[static_cast<size_t>(screen_mode::screen_overlay)];

        _init_screen3d_scene(screen3d_scene);
        _init_screen2d_scene(screen2d_scene);
        _init_screen_overlay_scene(overlay_scene);

        _scene_ctrl_window = std::make_shared<triengine::gui::scene_control_window>(_vis.get());
        _vis->add_gui_window(_scene_ctrl_window, triengine::gui::dock_slot::left);

        _screen_ctrl_window = std::make_shared<screen_control_window>();
        _vis->add_gui_window(_screen_ctrl_window, triengine::gui::dock_slot::left);

        // The demo starts in 3D mode, so the texture starts at the square size.
        _tex_size = triengine::vec2_i32{ kScreen3dTexSize, kScreen3dTexSize };

        const pattern_params init_params = _screen_ctrl_window->current_params();

        // Fill the initial texture once so the first frame is not blank; later frames come
        // from the background producer.
        auto init_img = std::make_shared<triengine::image_buffer>();
        init_img->prepare(kScreen3dTexSize, kScreen3dTexSize, triengine::image_format_type::rgb);
        {
            std::vector<int32_t> rows(static_cast<size_t>(kScreen3dTexSize));
            std::iota(rows.begin(), rows.end(), 0);
            fill_pattern_rows(*init_img, init_params, 0.0f, rows);
        }

        // The GPU resource manager is shared across scenes, so this texture is valid for
        // both scenes' quads.
        triengine::texture_params_t tex_params;
        tex_params.generate_mipmap = false; // updated every frame
        _screen_tex_handle = screen3d_scene.create_texture_2d(init_img, tex_params);
        // Texture shading also samples a specular map; a white one keeps the material valid.
        const triengine::texture_handle_t specular_tex =
            screen3d_scene.create_texture_2d_from_uniform_color(
                triengine::color3_f32(1.0f, 1.0f, 1.0f));

        // Each scene gets its own quad referencing the shared diffuse texture; the 2D quad
        // is unlit (flat), the 3D quad stays lit.
        _screen3d_quad = _create_screen_quad_mesh(_screen_tex_handle, specular_tex);
        _screen3d_quad->translate(triengine::vec3_f32{ 0.0f, 1.25f, 2.0f });
        screen3d_scene.add_geometry(_screen3d_quad);

        _screen2d_quad = _create_screen_quad_mesh(_screen_tex_handle, specular_tex);
        _screen2d_quad->set_lighting_mode(triengine::geometry::mesh_object::lighting_mode::unlit);
        screen2d_scene.add_geometry(_screen2d_quad);

        // Overlay scene: a lit box drawn over the background image. Use the shared texture as
        // the background for now; entering the mode resizes it to the 16:9 background size. The
        // box sits in front along +z (run() rotates it each frame in screen_overlay mode).
        _overlay_mesh = triengine::geometry::mesh_object::create_box(1.0f, 1.0f, 1.0f);
        _overlay_mesh->set_name("overlay_box");
        _overlay_mesh->paint_uniform_color(triengine::color3_f32{ 0.9f, 0.5f, 0.2f });
        _overlay_mesh->translate(triengine::vec3_f32{ 0.0f, 0.0f, kOverlayBoxDepth });
        overlay_scene.add_geometry(_overlay_mesh);
        overlay_scene.get_render_config()->bg_image = _screen_tex_handle;

        // Seed the pinhole intrinsics to match the 16:9 background image, principal point
        // centered, with a focal length that frames the box at kOverlayBoxDepth.
        triengine::pinhole_camera::intrinsics_t overlay_intrinsics;
        overlay_intrinsics.image_width = kBackgroundTexWidth;
        overlay_intrinsics.image_height = kBackgroundTexHeight;
        overlay_intrinsics.cx = static_cast<float>(kBackgroundTexWidth) * 0.5f;
        overlay_intrinsics.cy = static_cast<float>(kBackgroundTexHeight) * 0.5f;
        overlay_intrinsics.fx = overlay_intrinsics.fy = 400.0f;
        _screen_ctrl_window->set_overlay_intrinsics(overlay_intrinsics);

        _screen_ctrl_window->set_target_quad(_screen3d_quad);

        _curr_screen_mode = _screen_ctrl_window->mode();

        // Start the producer at the initial (square) texture size.
        _producer = std::make_unique<async_pattern_producer>();
        _producer->start(kScreen3dTexSize, kScreen3dTexSize,
            triengine::image_format_type::rgb, init_params);

        XUTL_TRACE("{}() LEAVE", __func__);
    }

    void screen_demo_app::destroy()
    {
        XUTL_TRACE("{}() ENTER", __func__);

        // Stop the worker first so no frames are produced during teardown.
        if (_producer) {
            _producer->stop();
            _producer.reset();
        }

        _screen_ctrl_window.reset();
        _scene_ctrl_window.reset();
        _overlay_mesh.reset();
        _screen2d_quad.reset();
        _screen3d_quad.reset();
        for (auto& scn : _scenes) {
            scn.reset();
        }

        _vis->destroy_window();
        _vis.reset();

        XUTL_TRACE("{}() LEAVE", __func__);
    }

    void screen_demo_app::run()
    {
        XUTL_TRACE("{}() ENTER", __func__);

        // Last frame size the 2D screen was fitted to (0 = not yet fitted).
        triengine::vec2_i32 prev_screen2d_fitted_frame_size{ 0, 0 };

        while (_vis->update_window())
        {
            // Apply a pending mode switch first, so the texture and quad match the new mode
            // before this frame's upload + render.
            if (const screen_mode new_mode = _screen_ctrl_window->mode();
                new_mode != _curr_screen_mode) {
                _vis->switch_scene(_scenes[static_cast<size_t>(new_mode)]->get_id());
                _curr_screen_mode = new_mode;

                if (new_mode == screen_mode::screen_3d) {
                    this->_fit_screen3d();
                } else if (new_mode == screen_mode::screen_overlay) {
                    this->_fit_screen_overlay();
                }
                // Force a re-fit when (re-)entering 2D mode below.
                prev_screen2d_fitted_frame_size = triengine::vec2_i32{ 0, 0 };
            }

            // In 2D mode, fit to the current scene frame size (excludes ImGui panels) when
            // it changes; { 0, 0 } before the first render() is skipped.
            if (_curr_screen_mode == screen_mode::screen_2d) {
                const triengine::vec2_i32 curr_frame_size = _vis->get_frame_size();
                if (curr_frame_size.x() > 0 && curr_frame_size.y() > 0 &&
                    (curr_frame_size.x() != prev_screen2d_fitted_frame_size.x() ||
                     curr_frame_size.y() != prev_screen2d_fitted_frame_size.y())) {
                    this->_fit_screen2d_to_frame(curr_frame_size);
                    prev_screen2d_fitted_frame_size = curr_frame_size;
                }
            }

            // In screen_overlay mode, apply the chosen camera (orbit vs calibrated pinhole),
            // the background fit, and spin the overlay mesh. A window resize is handled entirely
            // by the renderer's UV-fit, so unlike 2D mode there is no per-frame texture re-fit.
            if (_curr_screen_mode == screen_mode::screen_overlay) {
                auto& overlay_scene = *_scenes[static_cast<size_t>(screen_mode::screen_overlay)];

                // Switch the active camera only when the toggle changes.
                const bool want_pinhole = _screen_ctrl_window->overlay_use_pinhole();
                const bool is_pinhole =
                    (overlay_scene.get_camera()->get_type() == triengine::camera_type::pinhole);
                if (want_pinhole != is_pinhole) {
                    overlay_scene.switch_camera_type(want_pinhole
                        ? triengine::camera_type::pinhole
                        : triengine::camera_type::arcball);

                    if (want_pinhole) {
                        // The box is positioned directly in the camera frame, so the camera pose
                        // is the identity extrinsic (world space and camera space coincide).
                        overlay_scene.get_camera()->as<triengine::pinhole_camera>()
                            ->set_extrinsic(triengine::math::mat4_identity<float>());
                    }
                }

                if (want_pinhole) {
                    overlay_scene.get_camera()->as<triengine::pinhole_camera>()
                        ->set_intrinsics(_screen_ctrl_window->overlay_intrinsics());
                }

                // The pinhole camera letterboxes its projection to the image aspect ratio
                // (contain), so 'contain' (the default) is the fit that keeps the background
                // aligned with the mesh. Other fits are honored too, which visibly misaligns them.
                overlay_scene.get_render_config()->bg_image_fit = _screen_ctrl_window->bg_fit_mode();

                // Spin the box about its own center, kept at kOverlayBoxDepth in front.
                const float elapsed_sec =
                    std::chrono::duration<float>(std::chrono::steady_clock::now() - _start_time).count();
                const float angle_rad = triengine::math::deg2rad(
                    elapsed_sec * _screen_ctrl_window->overlay_rot_speed());
                triengine::mat4_f32 box_model = triengine::math::rotate_around_axis(
                    triengine::math::mat4_identity<float>(),
                    angle_rad,
                    triengine::vec3_f32{ 0.0f, 1.0f, 0.0f });
                box_model = triengine::math::translate_offset(
                    box_model, triengine::vec3_f32{ 0.0f, 0.0f, kOverlayBoxDepth });
                _overlay_mesh->set_model(box_model);
            }

            _producer->set_params(_screen_ctrl_window->current_params());

            // Upload the latest finished frame, if any.
            std::shared_ptr<triengine::image_buffer> frame = _producer->acquire_latest();

            auto& curr_active_scn = _scenes[static_cast<size_t>(_curr_screen_mode)];
            if (frame &&
                frame->width_pixels() == _tex_size.x() &&
                frame->height_pixels() == _tex_size.y()) {
                curr_active_scn->update_texture_2d(_screen_tex_handle, frame);
            }

            // render() applies the queued texture update.
            _vis->render();

            // Reuse the buffer now that render() has consumed the upload.
            if (frame) {
                _producer->recycle(std::move(frame));
            }
        }

        XUTL_TRACE("{}() LEAVE", __func__);
    }

    void screen_demo_app::_fit_screen2d_to_frame(triengine::vec2_i32 frame_size)
    {
        auto& screen2d_scene = *_scenes[static_cast<size_t>(screen_mode::screen_2d)];

        // 1. update_texture_2d() needs a matching size, so recreate the texture at the new
        //    resolution (its content is overwritten by the next produced frame).
        auto resized_img = std::make_shared<triengine::image_buffer>();
        resized_img->prepare(frame_size.x(), frame_size.y(), triengine::image_format_type::rgb);

        // 2. The handle changes, so repoint both quads.
        screen2d_scene.destroy_texture(_screen_tex_handle);
        triengine::texture_params_t tex_params;
        tex_params.generate_mipmap = false; // updated every frame
        _screen_tex_handle = screen2d_scene.create_texture_2d(resized_img, tex_params);
        _screen2d_quad->get_texture_shading_material()->diffuse_map = _screen_tex_handle;
        _screen3d_quad->get_texture_shading_material()->diffuse_map = _screen_tex_handle;
        _tex_size = frame_size;

        // 3. Retarget the producer; stale-size frames still in flight are dropped in run().
        _producer->set_target_size(frame_size.x(), frame_size.y(),
            triengine::image_format_type::rgb);

        // 4. Stretch the quad to the frame aspect so it exactly fills the orthographic
        //    viewport (ortho half_w = half_h * aspect).
        const float aspect =
            static_cast<float>(frame_size.x()) / static_cast<float>(frame_size.y());
        _screen2d_quad->set_model(
            triengine::math::scale(triengine::math::mat4_identity<float>(),
                triengine::vec3_f32{ aspect, 1.0f, 1.0f }));
    }

    void screen_demo_app::_fit_screen3d()
    {
        // Already square: nothing to do.
        if (_tex_size.x() == kScreen3dTexSize && _tex_size.y() == kScreen3dTexSize) {
            return;
        }

        auto& screen3d_scene = *_scenes[static_cast<size_t>(screen_mode::screen_3d)];

        // 1. Recreate the texture at the square size, filling it once so the switch shows
        //    correct content immediately, before the producer catches up.
        const pattern_params params = _screen_ctrl_window->current_params();
        auto square_img = std::make_shared<triengine::image_buffer>();
        square_img->prepare(kScreen3dTexSize, kScreen3dTexSize, triengine::image_format_type::rgb);
        {
            std::vector<int32_t> rows(static_cast<size_t>(kScreen3dTexSize));
            std::iota(rows.begin(), rows.end(), 0);
            fill_pattern_rows(*square_img, params, 0.0f, rows);
        }

        // 2. The handle changes, so repoint both quads.
        screen3d_scene.destroy_texture(_screen_tex_handle);
        triengine::texture_params_t tex_params;
        tex_params.generate_mipmap = false; // updated every frame
        _screen_tex_handle = screen3d_scene.create_texture_2d(square_img, tex_params);
        _screen2d_quad->get_texture_shading_material()->diffuse_map = _screen_tex_handle;
        _screen3d_quad->get_texture_shading_material()->diffuse_map = _screen_tex_handle;
        _tex_size = triengine::vec2_i32{ kScreen3dTexSize, kScreen3dTexSize };

        // 3. Retarget the producer back to the square size.
        _producer->set_target_size(kScreen3dTexSize, kScreen3dTexSize,
            triengine::image_format_type::rgb);
    }

    void screen_demo_app::_fit_screen_overlay()
    {
        // Already at the background size: only (re)assign the background image handle, in case
        // another mode recreated the shared texture while this scene was inactive.
        auto& overlay_scene = *_scenes[static_cast<size_t>(screen_mode::screen_overlay)];
        if (_tex_size.x() == kBackgroundTexWidth && _tex_size.y() == kBackgroundTexHeight) {
            overlay_scene.get_render_config()->bg_image = _screen_tex_handle;
            return;
        }

        // 1. Recreate the texture at the fixed 16:9 background size, filling it once so the
        //    switch shows correct content immediately, before the producer catches up.
        const pattern_params params = _screen_ctrl_window->current_params();
        auto bg_img = std::make_shared<triengine::image_buffer>();
        bg_img->prepare(kBackgroundTexWidth, kBackgroundTexHeight, triengine::image_format_type::rgb);
        {
            std::vector<int32_t> rows(static_cast<size_t>(kBackgroundTexHeight));
            std::iota(rows.begin(), rows.end(), 0);
            fill_pattern_rows(*bg_img, params, 0.0f, rows);
        }

        // 2. The handle changes, so repoint both quads and the overlay background image.
        overlay_scene.destroy_texture(_screen_tex_handle);
        triengine::texture_params_t tex_params;
        tex_params.generate_mipmap = false; // updated every frame
        _screen_tex_handle = overlay_scene.create_texture_2d(bg_img, tex_params);
        _screen2d_quad->get_texture_shading_material()->diffuse_map = _screen_tex_handle;
        _screen3d_quad->get_texture_shading_material()->diffuse_map = _screen_tex_handle;
        overlay_scene.get_render_config()->bg_image = _screen_tex_handle;
        _tex_size = triengine::vec2_i32{ kBackgroundTexWidth, kBackgroundTexHeight };

        // 3. Retarget the producer to the background size.
        _producer->set_target_size(kBackgroundTexWidth, kBackgroundTexHeight,
            triengine::image_format_type::rgb);
    }

} // namespace
