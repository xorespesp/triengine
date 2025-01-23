#pragma once
#include <utils/logger.hh>
#include <utils/path_utils.hh>

#include <triengine/offscreen_window.hh>
#include <triengine/math.hh>
#include <triengine/io/file_obj_loader.hh>

#include <opencv2/opencv.hpp>

#include <memory>
#include <array>

#include <iostream>
#include <conio.h>

namespace gui
{
    class demo_app
    {
    private:
        std::unique_ptr<triengine::offscreen_window> _off_window;
        triengine::scene* _scene{ nullptr };
        triengine::camera* _camera{ nullptr };

        std::shared_ptr<triengine::geometry::triangle_mesh_object> _obj_texcolor_mesh;

    public:
        demo_app() = default;
        ~demo_app() = default;

        void create(
            const std::filesystem::path& triengine_resource_dir)
        {
            LOG_TRACE("%s() ENTER", __func__);

            _off_window = std::make_unique<triengine::offscreen_window>();
            _off_window->create_window(1280, 720);

            _camera = _off_window->get_current_camera();

            {
                auto& camera_params = _camera->get_parameters();
                camera_params.lookat_center = triengine::vec3_f32{ -0.069978f, -0.646604f, -0.001912f };
                camera_params.camera_front = triengine::vec3_f32{ 0.630178f, -0.076719f, -0.772651f };
                camera_params.camera_right = triengine::vec3_f32{ -0.774935f, -0.0f, -0.63204f };
                camera_params.camera_up = triengine::vec3_f32{ -0.048490f, -0.997053f, 0.059452f };
                camera_params.yaw = 140.799164f;
                camera_params.pitch = -4.400012f;
                camera_params.zoom = 0.985002f;
            }

            _scene = _off_window->get_default_scene();
            _scene->scn_config.bg_color = triengine::color4_f32::all(0.0f);
            _scene->scn_config.bg_color.a() = 0.0f;
            _scene->scn_config.pcd_point_size = 2.5f;
            _scene->scn_config.show_origin_xz_grid = false;
            _scene->scn_config.infgrid_opts.grid_color = triengine::vec3_f32{ 1.0f, 0.0f, 0.0f };
            _scene->scn_config.light_opts.point_light.position = triengine::vec3_f32{ 0.0f, -1.5f, -1.5f };
            _scene->scn_config.light_opts.dir_light.diffuseIntensity = 0.8f;

            _off_window->enable_mirror_mode(false);

            if (auto new_obj = std::make_shared<triengine::geometry::triangle_mesh_object>();
                triengine::io::load_obj_file(
                    triengine_resource_dir / "objects/skull/12140_Skull_v3_L2.obj",
                    //triengine_resource_dir / "objects/car_engine/car_engine.obj",
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
                _scene->add_object(new_obj);
                _obj_texcolor_mesh = new_obj;
            }

        }

        void destroy()
        {
            LOG_TRACE("%s() ENTER", __func__);

            _off_window->destroy_window();
            _off_window.reset();

            LOG_TRACE("%s() LEAVE", __func__);
        }

        void run()
        {
            LOG_TRACE("%s() ENTER", __func__);

            cv::VideoCapture cap;
            cap.open("source.mp4");

            cv::Mat bg_frame;
            if (!cap.read(bg_frame)) {
                LOG_ERROR("failed to read bg frame");
                return;
            }

            cv::cvtColor(bg_frame, bg_frame, cv::COLOR_BGR2BGRA);

            LOG_DEBUG("bg frame size: [%dx%d] (type %d)", bg_frame.cols, bg_frame.rows, bg_frame.type());
            cv::imshow("bg frame", bg_frame);
            cv::waitKey(0);

            triengine::image frame;
            for(bool flag_stop{ false }; !flag_stop && _off_window->update_window();)
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

                _off_window->render(frame);

                cv::Mat cv_frame(
                    frame.height_pixels(),
                    frame.width_pixels(),
                    CV_8UC4,
                    static_cast<void*>(frame.data()),
                    frame.stride_bytes()
                );
                cv::flip(cv_frame, cv_frame, 0);

                // 2. 알파 채널 추출
                std::vector<cv::Mat> channels;
                cv::split(cv_frame, channels); // RGBA 채널 분리
                cv::Mat alphaChannel = channels[3]; // 알파 채널

                cv::Mat cv_mask;
                cv::threshold(alphaChannel, cv_mask, 1, 255, cv::THRESH_BINARY);

                cv::Mat cv_result = bg_frame.clone();
                cv_frame.copyTo(cv_result, cv_mask);
                cv::imshow("offscreen rendering", cv_result);
                switch (cv::waitKey(1)) {
                case 'w':
                case 'W':
                    cv::imwrite("frame.png", cv_frame);
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
            LOG_TRACE("%s() LEAVE", __func__);
        }

    }; // class

} // namespace

void run_demo(
    const std::filesystem::path& triengine_resource_dir)
{
    gui::demo_app app;

    LOG_INFO("Creating app..");
    app.create(triengine_resource_dir);

    LOG_INFO("Running app..");
    app.run();

    LOG_INFO("Destroying app..");
    app.destroy();
}

int main(int argc, char** argv)
{
    int retval = -1;

    const std::filesystem::path curr_image_dir_path{ utils::get_current_module_image_path().parent_path() };
    //::SetCurrentDirectoryW(curr_image_dir_path.c_str());
    ::SetConsoleOutputCP(CP_UTF8); // https://github.com/gabime/spdlog/issues/762

    utils::logger::instance().init(utils::logger::init_option()
        .set_logger_name("example-offscreen")
        .set_logger_level(utils::logger::level::trace)
        .enable_stdout_logging()
        .enable_async_mode()
    );

    LOG_TRACE("Build: " __DATE__ ", " __TIME__);
    LOG_TRACE("----- %s() ENTER", __func__);

    try
    {
        run_demo(
            curr_image_dir_path / "../../../triengine-installed/x64-Release/resources"
        );

        retval = 0;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("----- %s() EXCEPTION -> %s", __func__, e.what());
    }
    catch (...)
    {
        LOG_CRITICAL("----- %s() UNKNOWN EXCEPTION", __func__);
#if defined(_DEBUG)
        ::puts("\nPress any key to debug application ...");
        static_cast<void>(::_getch());
        std::exception_ptr eptr = std::current_exception();
        ::__debugbreak();
        std::rethrow_exception(eptr);
#endif // ^^^ _DEBUG ^^^
    }

    LOG_TRACE("----- %s() LEAVE", __func__);

    utils::logger::instance().deinit();
    return retval;
}