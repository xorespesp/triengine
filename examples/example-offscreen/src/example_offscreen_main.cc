#pragma once
#include <cxlib/utils/logger.hh>
#include <cxlib/utils/path_utils.hh>

#include "tiny_image_viewer.hh"

#include <triengine/visualization/offscreen_renderer.hh>
#include <triengine/math/math3d.hh>
#include <triengine/io/file_obj_loader.hh>

#include <memory>
#include <array>

#include <iostream>
#include <conio.h>

#undef EXAMPLE_HAS_OPENCV
#if defined(EXAMPLE_HAS_OPENCV)
#  include <opencv2/opencv.hpp>
#endif // ^^^ EXAMPLE_HAS_OPENCV ^^^

namespace demo
{
    class offscreen_demo_app
    {
    public:
        offscreen_demo_app() = default;
        ~offscreen_demo_app() = default;

        void create()
        {
            CXLIB_TRACE("{}() ENTER", __func__);

            const auto rsrc_dir_path = triengine::global_options::instance()->get_resource_directory();

            _renderer = std::make_unique<triengine::visualization::offscreen_renderer>();
            _renderer->create_renderer(1280, 720);
            
            auto scn = _renderer->add_scene();
            scn->get_render_config()->bg_color = triengine::color4_f32::all(0.0f);
            scn->get_render_config()->bg_color.a() = 1.0f;

            scn->get_render_config()->show_origin_xz_grid = true;
            scn->get_render_config()->light_opts.point_light.position = triengine::vec3_f32{ 0.0f, 1.5f, -1.5f };
            scn->get_render_config()->light_opts.point_light.ambientIntensity = 0.0f;
            scn->get_render_config()->light_opts.point_light.diffuseIntensity = 3.25f;
            scn->get_render_config()->light_opts.point_light.specularIntensity = 1.35f;

            scn->switch_camera_type(triengine::camera_type::arcball);
            auto& arcball_cam = *scn->get_camera()->as<triengine::arcball_camera>();
            arcball_cam.set_pivot_point(triengine::vec3_f32(0.0f, 0.5f, 0.0f));
            arcball_cam.set_yaw(120.0f);
            arcball_cam.set_pitch(0.0f);
            arcball_cam.set_zoom_distance(2.0f);

            auto mesh_axis_frame = triengine::geometry::triangle_mesh_object::create_coordinate_frame(0.5f);
            scn->add_geometry(mesh_axis_frame);

            if (auto new_obj = std::make_shared<triengine::geometry::triangle_mesh_object>();
                triengine::io::load_triangle_mesh_from_obj(
                    rsrc_dir_path / "objects/skull/12140_Skull_v3_L2.obj",
                    false,
                    *new_obj
                ))
            {
                //new_obj->compute_vertex_normals();
                new_obj->set_model(
                    triengine::math::scale(new_obj->get_model(), triengine::vec3_f32(0.0125f, 0.0125f, 0.0125f))
                );

                new_obj->apply_model_in_place();

                Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
                R = Eigen::AngleAxisf(triengine::math::deg2rad(0.0f), Eigen::Vector3f::UnitZ())
                    * Eigen::AngleAxisf(triengine::math::deg2rad(180.0f), Eigen::Vector3f::UnitY())
                    * Eigen::AngleAxisf(triengine::math::deg2rad(-90.0f), Eigen::Vector3f::UnitX());
                new_obj->rotate(R, true);
                new_obj->translate(triengine::vec3_f32(0.0f, 0.5f, 0.0f), true);
                scn->add_geometry(new_obj);
                _obj_texcolor_mesh = new_obj;
            }

            _scene = scn;
        }

        void destroy()
        {
            CXLIB_TRACE("{}() ENTER", __func__);

            _renderer->destroy_renderer();
            _renderer.reset();

            CXLIB_TRACE("{}() LEAVE", __func__);
        }

        void run()
        {
            CXLIB_TRACE("{}() ENTER", __func__);

#if !defined(EXAMPLE_HAS_OPENCV)
            tiny_viewer::tiny_image_viewer viewer("Offscreen Rendering Demo", 700, 500);
            if (!viewer.create_window()) {
                throw std::runtime_error{ "Failed to create viewer window" };
            }

            viewer.show_window();
            viewer.set_scale_mode(tiny_viewer::scale_mode::stretch_to_fill);
            viewer.set_flip_axis(tiny_viewer::flip_axis::vertical);

            viewer.set_resize_callback(
                [this, &viewer](const int32_t new_width, const int32_t new_height)
                {
                    TRIENGINE_TRACE("viewer window resize: %dx%d", new_width, new_height);
                    this->_renderer->resize_frame(new_width, new_height);
                }
            );

            viewer.set_key_callback(
                [this, &viewer](
                    const tiny_viewer::key_action action, 
                    const tiny_viewer::special_key skey, 
                    const uint32_t character_code, 
                    const tiny_viewer::key_modifiers mods)
                {
                    std::stringstream dump;

                    dump << "Action: " << (action == tiny_viewer::key_action::press) ? "Pressed" : "Released";

                    if (skey != tiny_viewer::special_key::none) {
                        // For a real application, you'd map special_key enum to string
                        dump << " Special KeyCode: " << static_cast<int>(skey);
                    }

                    if (character_code != 0 && skey == tiny_viewer::special_key::none) { // Character is primary if not a special key mapped
                        dump << " Character: '" << static_cast<char>(character_code) << "' (code: " << character_code << ")";
                    }

                    dump << " Modifiers: ";
                    if (mods.shift) dump << "[Shift] ";
                    if (mods.ctrl)  dump << "[Ctrl] ";
                    if (mods.alt)   dump << "[Alt] ";

                    TRIENGINE_TRACE("%s", dump.str().c_str());

                    if (action == tiny_viewer::key_action::press && skey == tiny_viewer::special_key::escape) {
                        TRIENGINE_INFO("Escape pressed!");
                        viewer.close_window();
                    }
                }
            );

#endif // ^^^ EXAMPLE_HAS_OPENCV ^^^

            triengine::image render_frame;
            for(bool flag_stop{ false }; !flag_stop;)
            {
                // for testing
                if (_obj_texcolor_mesh)
                {
                    constexpr float rotSpeed = triengine::math::pi<float>() / 8.0f;
                    const float dT = static_cast<float>(::glfwGetTime());

                    Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
                    //R = Eigen::AngleAxisf(rotSpeed * dT * 0.1f, Eigen::Vector3f::UnitZ()) *
                    //    Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitY()) *
                    //    Eigen::AngleAxisf(rotSpeed * dT * 0.5f, Eigen::Vector3f::UnitX());

                    R = Eigen::AngleAxisf(triengine::math::deg2rad(-90.0f), Eigen::Vector3f::UnitX()) *
                        Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitZ());

                    _obj_texcolor_mesh->rotate(R);
                }

                _renderer->render(
                    render_frame, 
                    triengine::image_format_type::bgra
                );

#if defined(EXAMPLE_HAS_OPENCV)
                cv::Mat cv_render_frame(
                    render_frame.height_pixels(),
                    render_frame.width_pixels(),
                    CV_8UC4,
                    static_cast<void*>(render_frame.data()),
                    render_frame.stride_bytes()
                );
                cv::flip(cv_render_frame, cv_render_frame, 0);
                cv::imshow("offscreen rendering", cv_render_frame);
                switch (cv::waitKey(1)) {
                case 'w':
                case 'W':
                    cv::imwrite("frame.png", cv_render_frame);
                    break;
                case 'q':
                case 'Q':
                    flag_stop = true;
                    break;
                default:
                    break;
                }
#else // ^^^ EXAMPLE_HAS_OPENCV ^^^ / vvv !EXAMPLE_HAS_OPENCV vvv
                viewer.set_image(
                    render_frame.data(),
                    render_frame.width_pixels(),
                    render_frame.height_pixels(),
                    tiny_viewer::image_format::bgra
                );
                if (!viewer.is_open()) { flag_stop = true; }
                viewer.process_events();
#endif // ^^^ EXAMPLE_HAS_OPENCV ^^^

                using namespace std::chrono_literals;
                static int32_t frameCount = 0;
                ++frameCount;
                static auto lastTime = std::chrono::steady_clock::now();
                const auto currentTime = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime) >= 1s)
                {
                    CXLIB_TRACE("Render FPS: {}", frameCount);
                    lastTime = currentTime;
                    frameCount = 0;
                }

            } // for

#if defined(EXAMPLE_HAS_OPENCV)
            cv::destroyAllWindows();
#endif // ^^^ EXAMPLE_HAS_OPENCV ^^^

            CXLIB_TRACE("{}() LEAVE", __func__);
        }

    private:
        std::unique_ptr<triengine::visualization::offscreen_renderer> _renderer;
        std::shared_ptr<triengine::scene> _scene;
        std::shared_ptr<triengine::geometry::triangle_mesh_object> _obj_texcolor_mesh;

    }; // class

} // namespace

void run_demo()
{
    demo::offscreen_demo_app app;

    CXLIB_INFO("Creating app..");
    app.create();

    CXLIB_INFO("Running app..");
    app.run();

    CXLIB_INFO("Destroying app..");
    app.destroy();
}

int main(
    [[maybe_unused]] int argc, 
    [[maybe_unused]] char** argv)
{
    _CXLIB utils::logger::instance().init(_CXLIB utils::logger::init_options()
        .set_logger_name("example-offscreen")
        .enable_stdout_logging(_CXLIB utils::logger::level::trace)
        .enable_async_mode()
    );

    const std::filesystem::path curr_image_dir_path{ _CXLIB utils::get_current_module_image_path().parent_path() };

    int retval{ -1 };

    CXLIB_TRACE("Build: {}, {}", __DATE__, __TIME__);
    CXLIB_TRACE("----- {}() ENTER", __func__);

    try
    {
        triengine::global_options::instance()->set_resource_directory(curr_image_dir_path / "../resources");
        triengine::global_options::instance()->get_logger().set_log_level(triengine::utility::log_level::trace);
        triengine::global_options::instance()->get_logger().register_print_callback(
            [](triengine::utility::log_level lv, std::string_view msg_sv)
            {
                _CXLIB utils::logger::instance().log(
                    [lv]() -> _CXLIB utils::logger::level {
                        switch (lv) {
                        case triengine::utility::log_level::trace: return _CXLIB utils::logger::level::trace;
                        case triengine::utility::log_level::debug: return _CXLIB utils::logger::level::debug;
                        case triengine::utility::log_level::info: return _CXLIB utils::logger::level::info;
                        case triengine::utility::log_level::warn: return _CXLIB utils::logger::level::warn;
                        case triengine::utility::log_level::error: return _CXLIB utils::logger::level::error;
                        case triengine::utility::log_level::critical: return _CXLIB utils::logger::level::critical;
                        default: return _CXLIB utils::logger::level::warn;
                        }
                    }(), msg_sv);
            });

        run_demo();

        retval = 0;
    }
    catch (const std::exception& e)
    {
        CXLIB_ERROR("----- {}() EXCEPTION -> {}", __func__, e.what());
    }
    catch (...)
    {
        CXLIB_CRITICAL("----- {}() UNKNOWN EXCEPTION", __func__);
#if defined(_DEBUG)
        ::puts("\nPress <ENTER> to debug application ...");
        static_cast<void>(::getchar());
        std::exception_ptr eptr = std::current_exception();
        ::__debugbreak();
        std::rethrow_exception(eptr);
#endif // ^^^ _DEBUG ^^^
    }

    CXLIB_TRACE("----- {}() LEAVE", __func__);

    _CXLIB utils::logger::instance().deinit();
    return retval;
}