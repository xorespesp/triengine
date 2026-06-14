#include "plasma_scene.hh"

#include <triengine/math/math3d.hh>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <execution>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

namespace scene
{
    namespace
    {
        // Longest side of the generated image. The pattern is smooth and low-frequency, so a
        // small image bilinear-upscaled by the GPU is visually indistinguishable from a
        // full-resolution one while costing a fraction of the CPU.
        constexpr int32_t kGenMaxSide = 320;

        // Number of pattern cycles across each axis / the diagonal. Kept constant regardless
        // of the generation resolution so the look does not change with the window size.
        constexpr float kCyclesX = 3.0f;
        constexpr float kCyclesY = 3.0f;
        constexpr float kCyclesDiag = 2.0f;

        // Animation rates expressed PER SECOND, so the motion is time-based and its speed is
        // independent of the production/render frame rate (max_fps). The three axes drift at
        // slightly different rates while the palette cycles, giving a continuously flowing,
        // non-repeating-looking plasma. Values are tuned to match the previous look at ~60 fps;
        // scale them to taste.
        constexpr float kPhaseSpeedX = 0.78f;    // radians/s
        constexpr float kPhaseSpeedY = 0.66f;    // radians/s
        constexpr float kPhaseSpeedDiag = 0.54f; // radians/s
        constexpr float kPaletteSpeed = 60.0f;   // palette indices/s

        inline uint8_t unit_to_u8(const float v) {
            return static_cast<uint8_t>(std::min(std::max(v, 0.0f), 1.0f) * 255.0f);
        }

        // A 256-entry cyclic RGB palette (a smooth rainbow). Indexing wraps, so the palette is
        // seamless. Built once.
        using palette_t = std::array<std::array<uint8_t, 3>, 256>;
        palette_t make_palette()
        {
            palette_t pal;
            constexpr float two_pi = 2.0f * triengine::math::pi<float>();
            constexpr float third = two_pi / 3.0f;
            for (int32_t k = 0; k < 256; ++k) {
                const float t = two_pi * static_cast<float>(k) / 256.0f;
                pal[static_cast<size_t>(k)] = {
                    unit_to_u8(0.5f + 0.5f * std::sin(t)),
                    unit_to_u8(0.5f + 0.5f * std::sin(t + third)),
                    unit_to_u8(0.5f + 0.5f * std::sin(t + 2.0f * third)),
                };
            }
            return pal;
        }

    } // namespace

    namespace
    {
        // Builds a 2x2 texture-shaded, unlit quad on the z=0 plane, centered at the origin.
        std::shared_ptr<triengine::geometry::mesh_object> _create_screen_quad_mesh(
            const triengine::texture_handle_t diffuse_map,
            const triengine::texture_handle_t specular_map)
        {
            auto quad = std::make_shared<triengine::geometry::mesh_object>(
                triengine::geometry::mesh_object::shading_mode::texture);
            quad->set_name("plasma_screen_quad");

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

            // The screen is flat, so show the texture as-is with no lighting.
            quad->set_lighting_mode(triengine::geometry::mesh_object::lighting_mode::unlit);

            return quad;
        }

        // Disable options a flat screen does not need, and view the quad head-on with an
        // orthographic camera.
        void _init_screen2d_scene(triengine::scene& scn)
        {
            auto& cfg = *scn.get_render_config();
            cfg.bg_color = triengine::color4_f32{ 0.0f, 0.0f, 0.0f, 1.0f };
            cfg.enable_anti_aliasing = false;
            cfg.show_object_normals = false;
            cfg.show_origin_xz_grid = false;
            cfg.light_opts.dir_light.enabled = false;
            cfg.light_opts.point_light.enabled = false;
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

    } // namespace

    // Generates animated CPU images on a worker thread and hands the latest finished frame to
    // the render thread. A small image_buffer pool cycles between worker and consumer: the
    // worker fills one, publishes it as the single "ready" frame (latest-wins), then waits
    // until the consumer takes it before producing the next, so production is paced to the
    // consumer. Touches only CPU buffers; no GL/scene calls.
    //
    // The plasma is separable: for each frame the worker fills three small 1D arrays
    // (per-column, per-row, per-diagonal sine values) using O(width + height) sine calls,
    // then each pixel is just palette[col[x] + row[y] + diag[x + y]]. There are no
    // per-pixel trig calls or multiplies.
    class plasma_scene::async_pattern_producer
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
            const triengine::image_format_type format)
        {
            {
                std::scoped_lock lk{ _mtx };
                _width = width;
                _height = height;
                _format = format;
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

        // Change the generated image size/format. Frames already produced at the old size are
        // dropped by the consumer's size check.
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

        // Take the latest finished frame, or nullptr if none is ready. Freeing the slot lets
        // the worker produce the next frame.
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
            const palette_t palette = make_palette();

            // Animation phases, advanced by elapsed wall-clock time on each produced frame so
            // the visible speed is independent of how fast frames are produced.
            float phase_x = 0.0f, phase_y = 0.0f, phase_diag = 0.0f;
            float palette_phase = 0.0f; // fractional palette index; wrapped to [0, 256)
            auto last_time = std::chrono::steady_clock::now();

            // Reused per-frame scratch (resized as the target size changes).
            std::vector<uint8_t> col_term;  // [width]
            std::vector<uint8_t> row_term;  // [height]
            std::vector<uint8_t> diag_term; // [width + height - 1]
            std::vector<int32_t> row_indices;
            std::shared_ptr<triengine::image_buffer> work;

            for (;;)
            {
                int32_t width{}, height{};
                triengine::image_format_type format{};
                {
                    std::unique_lock lk{ _mtx };
                    // Wait until the previous frame is consumed, pacing production to the
                    // consumer instead of busy-spinning.
                    _cv.wait(lk, [this] { return !_running || _ready == nullptr; });
                    if (!_running) { break; }

                    width = _width;
                    height = _height;
                    format = _format;

                    if (!_free.empty()) {
                        work = std::move(_free.back());
                        _free.pop_back();
                    }
                }

                if (width <= 0 || height <= 0) { continue; }

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

                // Advance the animation by elapsed wall-clock time (time-based), so the visible
                // speed does not depend on the produced frame rate.
                const auto now = std::chrono::steady_clock::now();
                float dt = std::chrono::duration<float>(now - last_time).count();
                last_time = now;
                // Clamp so a long stall (e.g. a minimized window) does not cause a big jump.
                dt = std::min(dt, 0.1f);

                phase_x += kPhaseSpeedX * dt;
                phase_y += kPhaseSpeedY * dt;
                phase_diag += kPhaseSpeedDiag * dt;
                palette_phase = std::fmod(palette_phase + kPaletteSpeed * dt, 256.0f);

                // Precompute the separable 1D sine terms (O(width + height) sine calls).
                const int32_t diag_count = width + height - 1;
                col_term.resize(static_cast<size_t>(width));
                row_term.resize(static_cast<size_t>(height));
                diag_term.resize(static_cast<size_t>(diag_count));

                const float fx = 2.0f * triengine::math::pi<float>() * kCyclesX / static_cast<float>(width);
                const float fy = 2.0f * triengine::math::pi<float>() * kCyclesY / static_cast<float>(height);
                const float fd = 2.0f * triengine::math::pi<float>() * kCyclesDiag / static_cast<float>(diag_count);

                for (int32_t x = 0; x < width; ++x) {
                    col_term[static_cast<size_t>(x)] = unit_to_u8(0.5f + 0.5f * std::sin(static_cast<float>(x) * fx + phase_x));
                }
                for (int32_t y = 0; y < height; ++y) {
                    row_term[static_cast<size_t>(y)] = unit_to_u8(0.5f + 0.5f * std::sin(static_cast<float>(y) * fy + phase_y));
                }
                for (int32_t d = 0; d < diag_count; ++d) {
                    diag_term[static_cast<size_t>(d)] = unit_to_u8(0.5f + 0.5f * std::sin(static_cast<float>(d) * fd + phase_diag));
                }

                // Fill rows in parallel: each pixel is three table reads + a palette lookup.
                uint8_t* const base = work->data();
                const uint32_t stride = work->stride_bytes();
                const uint8_t* const col = col_term.data();
                const uint8_t* const row = row_term.data();
                const uint8_t* const diag = diag_term.data();
                const std::array<uint8_t, 3>* const pal = palette.data();
                const int32_t shift = static_cast<int32_t>(palette_phase) & 0xFF;

                std::for_each(std::execution::par, row_indices.begin(), row_indices.end(),
                    [=](const int32_t y)
                    {
                        uint8_t* const dst_row = base + static_cast<size_t>(y) * stride;
                        const int32_t rv = row[static_cast<size_t>(y)];
                        for (int32_t x = 0; x < width; ++x) {
                            const int32_t idx = col[static_cast<size_t>(x)] + rv + diag[static_cast<size_t>(x + y)];
                            const std::array<uint8_t, 3>& c = pal[static_cast<uint8_t>(idx + shift)];
                            uint8_t* const px = dst_row + static_cast<size_t>(x) * 3;
                            px[0] = c[0];
                            px[1] = c[1];
                            px[2] = c[2];
                        }
                    });

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

        std::shared_ptr<triengine::image_buffer> _ready; // latest finished frame
        std::vector<std::shared_ptr<triengine::image_buffer>> _free; // recycled buffers
    }; // class

    triengine::vec2_i32 plasma_scene::_calc_gen_size(const triengine::vec2_i32 frame_size)
    {
        const int32_t fw = std::max(frame_size.x(), 1);
        const int32_t fh = std::max(frame_size.y(), 1);
        const int32_t longest = std::max(fw, fh);
        if (longest <= kGenMaxSide) {
            return triengine::vec2_i32{ fw, fh };
        }
        const float s = static_cast<float>(kGenMaxSide) / static_cast<float>(longest);
        return triengine::vec2_i32{
            std::max(static_cast<int32_t>(static_cast<float>(fw) * s), 1),
            std::max(static_cast<int32_t>(static_cast<float>(fh) * s), 1),
        };
    }

    plasma_scene::plasma_scene(
        triengine::visualization::offscreen_renderer_dx& renderer)
        : scene_wrapper(renderer.add_scene())
    {
        auto scn = this->get_scene();
        scn->set_name("plasma");

        _init_screen2d_scene(*scn);

        _frame_size = renderer.get_frame_size();
        _gen_size = _calc_gen_size(_frame_size);

        // Create the small generation texture (its content is filled by the producer).
        auto init_img = std::make_shared<triengine::image_buffer>();
        init_img->prepare(_gen_size.x(), _gen_size.y(), triengine::image_format_type::rgb);

        triengine::texture_params_t tex_params;
        tex_params.generate_mipmap = false; // updated every frame; mag-filter upscales it
        _screen_tex_handle = scn->create_texture_2d(init_img, tex_params);
        // Texture shading also samples a specular map; a white one keeps the material valid.
        const triengine::texture_handle_t specular_tex =
            scn->create_texture_2d_from_uniform_color(triengine::color3_f32(1.0f, 1.0f, 1.0f));

        _screen2d_quad = _create_screen_quad_mesh(_screen_tex_handle, specular_tex);
        this->_fit_quad_to_aspect(_frame_size);
        scn->add_geometry(_screen2d_quad);

        _producer = std::make_unique<async_pattern_producer>();
        _producer->start(_gen_size.x(), _gen_size.y(), triengine::image_format_type::rgb);
    }

    plasma_scene::~plasma_scene()
    {
        // Stop the worker before the CPU buffers it shares are destroyed. The texture/quad
        // live in the scene and are released with it (and the renderer), so no GL teardown is
        // needed here.
        if (_producer) {
            _producer->stop();
        }
    }

    void plasma_scene::update(const triengine::vec2_i32 frame_size)
    {
        if (!_producer) { return; }

        // React to a frame-size change: the small generation texture only changes when its
        // capped size does, while the quad always tracks the frame aspect.
        if (frame_size.x() > 0 && frame_size.y() > 0 &&
            (frame_size.x() != _frame_size.x() || frame_size.y() != _frame_size.y())) {
            _frame_size = frame_size;

            const triengine::vec2_i32 new_gen = _calc_gen_size(frame_size);
            if (new_gen.x() != _gen_size.x() || new_gen.y() != _gen_size.y()) {
                this->_resize_gen_texture(new_gen);
            }
            this->_fit_quad_to_aspect(frame_size);
        }

        auto scn = this->get_scene();

        // Upload the latest finished frame, if any (dropping stale-size frames in flight).
        std::shared_ptr<triengine::image_buffer> frame = _producer->acquire_latest();
        if (frame &&
            frame->width_pixels() == _gen_size.x() &&
            frame->height_pixels() == _gen_size.y()) {
            scn->update_texture_2d(_screen_tex_handle, frame);
            _inflight_frame = std::move(frame);
        } else if (frame) {
            // Stale size: recycle immediately instead of holding it for the next render.
            _producer->recycle(std::move(frame));
        }
    }

    void plasma_scene::post_render()
    {
        if (_producer && _inflight_frame) {
            _producer->recycle(std::move(_inflight_frame));
        }
        _inflight_frame.reset();
    }

    void plasma_scene::_fit_quad_to_aspect(const triengine::vec2_i32 frame_size)
    {
        const float aspect =
            static_cast<float>(std::max(frame_size.x(), 1)) /
            static_cast<float>(std::max(frame_size.y(), 1));
        _screen2d_quad->set_model(
            triengine::math::scale(triengine::math::mat4_identity<float>(),
                triengine::vec3_f32{ aspect, 1.0f, 1.0f }));
    }

    void plasma_scene::_resize_gen_texture(const triengine::vec2_i32 gen_size)
    {
        auto scn = this->get_scene();

        // update_texture_2d() needs a matching size, so recreate the texture at the new
        // generation resolution (its content is overwritten by the next produced frame).
        auto resized_img = std::make_shared<triengine::image_buffer>();
        resized_img->prepare(gen_size.x(), gen_size.y(), triengine::image_format_type::rgb);

        // The handle changes, so repoint the quad.
        scn->destroy_texture(_screen_tex_handle);
        triengine::texture_params_t tex_params;
        tex_params.generate_mipmap = false; // updated every frame; mag-filter upscales it
        _screen_tex_handle = scn->create_texture_2d(resized_img, tex_params);
        _screen2d_quad->get_texture_shading_material()->diffuse_map = _screen_tex_handle;
        _gen_size = gen_size;

        // Retarget the producer; stale-size frames still in flight are dropped in update().
        _producer->set_target_size(gen_size.x(), gen_size.y(),
            triengine::image_format_type::rgb);
    }

} // namespace
