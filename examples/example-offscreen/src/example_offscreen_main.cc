#pragma once
#include <cxlib/utils/logger.hh>
#include <cxlib/utils/path_utils.hh>

#include <triengine/visualization/offscreen_renderer.hh>
#include <triengine/math/math3d.hh>
#include <triengine/io/file_obj_loader.hh>

#include <opencv2/opencv.hpp>

#include <memory>
#include <array>

#include <iostream>
#include <conio.h>

namespace gui
{
    class offscreen_demo_app
    {
    public:
        offscreen_demo_app() = default;
        ~offscreen_demo_app() = default;

        void create()
        {
            CXLIB_TRACE("{}() ENTER", __func__);

            _renderer = std::make_unique<triengine::visualization::offscreen_renderer>();
            _renderer->create_renderer(1280, 720);

            _scene = _renderer->add_scene();
            _scene->get_render_config()->bg_color = triengine::color4_f32::all(0.0f);
            _scene->get_render_config()->bg_color.a() = 0.0f;
            _scene->get_render_config()->pcd_point_size = 2.5f;
            _scene->get_render_config()->show_origin_xz_grid = false;
            _scene->get_render_config()->infgrid_opts.grid_color = triengine::vec3_f32{ 1.0f, 0.0f, 0.0f };
            _scene->get_render_config()->light_opts.point_light.position = triengine::vec3_f32{ 0.0f, -1.5f, -1.5f };
            _scene->get_render_config()->light_opts.dir_light.diffuseIntensity = 0.8f;

            auto& scn_camera = *_scene->get_camera();
            scn_camera.set_mirror_mode(false);
            scn_camera.set_perspective_scale_factor(1.0f);
            scn_camera.set_fovy(65.0f);
            scn_camera.set_zoom(1.0f);
            scn_camera.set_direction(triengine::vec3_f32{ 0.744f, 0.153f, -0.651f });
            scn_camera.set_position(triengine::vec3_f32{ -1.240f, -0.847f, 1.113f });

            if (auto new_obj = std::make_shared<triengine::geometry::triangle_mesh_object>();
                triengine::io::load_triangle_mesh_from_obj(
                    triengine_resource_dir / "objects/skull/12140_Skull_v3_L2.obj",
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
                    * Eigen::AngleAxisf(triengine::math::deg2rad(90.0f), Eigen::Vector3f::UnitX());
                new_obj->rotate(R, true);
                new_obj->translate(triengine::vec3_f32(0.0f, -0.5f, 0.0f), true);
                _scene->add_geometry(new_obj);
                _obj_texcolor_mesh = new_obj;
            }

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

                    R = Eigen::AngleAxisf(triengine::math::deg2rad(90.0f), Eigen::Vector3f::UnitX()) *
                        Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitZ());

                    _obj_texcolor_mesh->rotate(R);
                }

                _renderer->render(render_frame);

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

            } // for

            cv::destroyAllWindows();
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
    gui::offscreen_demo_app app;

    CXLIB_INFO("Creating app..");
    app.create();

    CXLIB_INFO("Running app..");
    app.run();

    CXLIB_INFO("Destroying app..");
    app.destroy();
}

int main(int argc, char** argv)
{
    _CXLIB utils::logger::instance().init(_CXLIB utils::logger::init_options()
        .set_logger_name("example-offscreen")
        .set_logger_level(_CXLIB utils::logger::level::trace)
        .enable_stdout_logging()
        .enable_async_mode()
    );

    const std::filesystem::path curr_image_dir_path{ utils::get_current_module_image_path().parent_path() };

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