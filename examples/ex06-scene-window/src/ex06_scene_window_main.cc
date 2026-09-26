#include <xutl/debug/logger.hh>

#include <triengine/global_options.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/visualization/visualizer.hh>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>

namespace
{
    // A scene-only window: no GUI is created, and the camera orbits on its own until
    // the user takes over with the mouse.
    //   - drag / wheel / WASD : move the camera (pauses the orbit)
    //   - SPACE : resume/pause the orbit
    //   - C     : toggle the user camera interaction on and off
    //   - ESC   : close the window
    class scene_window_demo_app
    {
    public:
        void create()
        {
            XUTL_TRACE("{}() ENTER", __func__);

            _vis = std::make_unique<triengine::visualization::visualizer>();
            _vis->create_window("Triengine Scene Window", true, 1280, 720);

            XUTL_INFO("keys: [SPACE] orbit, [C] camera interaction, [ESC] close");

            // Hand the camera over to the user as soon as they grab it.
            _vis->set_mouse_button_callback(
                [this](
                    const int32_t button,
                    const int32_t action,
                    [[maybe_unused]] const int32_t mods,
                    [[maybe_unused]] bool& handled)
                {
                    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && _flag_orbit) {
                        _flag_orbit = false;
                        XUTL_INFO("orbit: paused (the camera is yours)");
                    }
                });

            // The input callbacks fire even with the camera interaction disabled.
            _vis->set_key_callback(
                [this](
                    const int32_t key,
                    [[maybe_unused]] const int32_t scancode,
                    const int32_t action,
                    [[maybe_unused]] const int32_t mods,
                    [[maybe_unused]] bool& handled)
                {
                    if (action != GLFW_PRESS) { return; }

                    switch (key) {
                    case GLFW_KEY_ESCAPE:
                        _vis->close_window();
                        break;
                    case GLFW_KEY_C:
                        _vis->enable_camera_interaction(!_vis->is_camera_interaction_enabled());
                        XUTL_INFO("camera interaction: {}", _vis->is_camera_interaction_enabled() ? "on" : "off");
                        break;
                    case GLFW_KEY_SPACE:
                        _flag_orbit = !_flag_orbit;
                        break;
                    }
                });

            auto scn = _vis->add_scene();
            scn->get_render_config()->show_origin_xz_grid = true;

            scn->switch_camera_type(triengine::camera_type::arcball);
            auto& arcball_cam = *scn->get_camera()->as<triengine::arcball_camera>();
            arcball_cam.set_pivot_point(triengine::vec3_f32{ 0.0f, 0.4f, 0.0f });
            arcball_cam.set_pitch(20.0f);
            arcball_cam.set_zoom_distance(4.0f);

            scn->add_geometry(triengine::geometry::mesh_object::create_coordinate_frame(0.5f));

            auto box = triengine::geometry::mesh_object::create_box(0.6f, 0.6f, 0.6f);
            box->paint_uniform_color(triengine::color3_f32{ 0.85f, 0.35f, 0.25f });
            box->translate(triengine::vec3_f32{ -0.8f, 0.3f, 0.0f }, true);
            scn->add_geometry(box);

            auto sphere = triengine::geometry::mesh_object::create_sphere(0.35f);
            sphere->paint_uniform_color(triengine::color3_f32{ 0.25f, 0.55f, 0.85f });
            sphere->translate(triengine::vec3_f32{ 0.8f, 0.35f, 0.0f }, true);
            scn->add_geometry(sphere);

            scn->add_label_3d("scene only", triengine::vec3_f32{ 0.0f, 1.2f, 0.0f }, 0.15f, triengine::color3_f32{ 1.0f, 1.0f, 1.0f });

            _scene = scn;
        }

        void destroy()
        {
            XUTL_TRACE("{}() ENTER", __func__);

            _scene.reset();

            _vis->destroy_window();
            _vis.reset();

            XUTL_TRACE("{}() LEAVE", __func__);
        }

        void run()
        {
            XUTL_TRACE("{}() ENTER", __func__);

            constexpr float kOrbitSpeedDegPerSec{ 20.0f };

            auto prev_time = std::chrono::steady_clock::now();
            while (_vis->update_window())
            {
                const auto curr_time = std::chrono::steady_clock::now();
                const float delta_sec = std::chrono::duration<float>(curr_time - prev_time).count();
                prev_time = curr_time;

                if (_flag_orbit) {
                    auto& arcball_cam = *_scene->get_camera()->as<triengine::arcball_camera>();
                    arcball_cam.set_yaw(std::fmod(arcball_cam.get_yaw() + kOrbitSpeedDegPerSec * delta_sec, 360.0f));
                }

                _vis->render();
            } // while

            XUTL_TRACE("{}() LEAVE", __func__);
        }

    private:
        std::unique_ptr<triengine::visualization::visualizer> _vis;
        std::shared_ptr<triengine::scene> _scene;
        bool _flag_orbit{ true };
    };

    void run_demo()
    {
        scene_window_demo_app app;

        XUTL_INFO("Creating app..");
        app.create();

        XUTL_INFO("Running app..");
        app.run();

        XUTL_INFO("Destroying app..");
        app.destroy();
    }

} // namespace

int main(
    [[maybe_unused]] int argc,
    [[maybe_unused]] char** argv)
{
    _XUTL debug::logger::construct(_XUTL debug::logger::init_options()
        .set_logger_name("ex06-scene-window")
        .enable_stdout_logging(_XUTL debug::logger::level::trace)
        .enable_async_mode()
    );

    XUTL_TRACE("Build: {}, {}", __DATE__, __TIME__);
    XUTL_TRACE("----- {}() ENTER", __func__);

    int retval{ -1 };

    try
    {
        triengine::global_options::instance()->get_logger().set_log_level(triengine::utility::log_level::info);
        const auto engine_log_subscription = triengine::global_options::instance()->get_logger().subscribe(
            [](triengine::utility::log_level lv, std::string_view msg_sv)
            {
                _XUTL debug::logger::instance()->log(
                    [lv]() -> _XUTL debug::logger::level {
                        switch (lv) {
                        case triengine::utility::log_level::trace: return _XUTL debug::logger::level::trace;
                        case triengine::utility::log_level::debug: return _XUTL debug::logger::level::debug;
                        case triengine::utility::log_level::info: return _XUTL debug::logger::level::info;
                        case triengine::utility::log_level::warn: return _XUTL debug::logger::level::warn;
                        case triengine::utility::log_level::error: return _XUTL debug::logger::level::error;
                        case triengine::utility::log_level::critical: return _XUTL debug::logger::level::critical;
                        default: return _XUTL debug::logger::level::warn;
                        }
                    }(), msg_sv);
            });

        run_demo();
        retval = 0;
    }
    catch (const std::exception& e)
    {
        XUTL_ERROR("----- {}() EXCEPTION -> {}", __func__, e.what());
    }
    catch (...)
    {
        XUTL_CRITICAL("----- {}() UNKNOWN EXCEPTION", __func__);
    }

    XUTL_TRACE("----- {}() LEAVE", __func__);
    _XUTL debug::logger::destruct();

    return retval;
}
