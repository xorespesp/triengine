#pragma once
#include <utils/logger.hh>
#include <utils/path_utils.hh>

#include <triengine/visualizer_window.hh>
#include <triengine/frame_buffer.hh>
#include <triengine/math.hh>
#include <triengine/io/file_obj_loader.hh>

#include <memory>
#include <array>

#include <iostream>
#include <conio.h>

namespace gui
{
    class demo_app
    {
    private:
        std::unique_ptr<triengine::visualizer_window> _vis_window;
        std::shared_ptr<triengine::frame_buffer> _frame_buffer;

        std::shared_ptr<triengine::geometry::triangle_mesh_object> _obj_texcolor_mesh;

    public:
        demo_app() = default;
        ~demo_app() = default;

        void set_close_callback(
            triengine::visualizer_window::close_callback cb
        );

        void set_key_callback(
            triengine::visualizer_window::key_callback cb
        );

        void create(
            const std::filesystem::path& triengine_resource_dir)
        {
            LOG_TRACE("%s() ENTER", __func__);

            _vis_window = std::make_unique<triengine::visualizer_window>();
            _vis_window->create_window("Triengine Orbbec Demo"
                " (Build: " __DATE__ ", " __TIME__
#if defined (_DEBUG)
                " DBG"
#else  // ^^^ _DEBUG ^^^ / vvv !_DEBUG vvv
                " REL"
#endif // ^^^ !_DEBUG ^^^
                ")"
            );

            _vis_window->enable_mirror_mode(false);
            _vis_window->get_render_config().pcd_point_size = 2.5f;
            //_vis_window->get_render_config().show_origin_xz_plane = true;
            _vis_window->get_render_config().light_opts.point_light.position = triengine::vec3_f32{ 0.0f, -1.5f, -1.5f };

            _vis_window->set_key_callback(
                [this](
                    [[maybe_unused]] triengine::visualizer_window& vis,
                    [[maybe_unused]] const int key,
                    [[maybe_unused]] const int scancode,
                    [[maybe_unused]] const int action,
                    [[maybe_unused]] const int mods,
                    [[maybe_unused]] bool& handled)
                {
                    if (action != GLFW_RELEASE)
                    {
                        switch (key) {
                        case GLFW_KEY_ESCAPE:
                            break;
                        case GLFW_KEY_F12:
                            _vis_window->enable_main_menu(!_vis_window->is_main_menu_enabled());
                            break;
                        case GLFW_KEY_A:
                            _vis_window->get_render_config().light_opts.point_light.position.x() -= 0.05f; // left
                            break;
                        case GLFW_KEY_D:
                            _vis_window->get_render_config().light_opts.point_light.position.x() += 0.05f; // right
                            break;
                        case GLFW_KEY_W:
                            _vis_window->get_render_config().light_opts.point_light.position.z() += 0.05f; // forward
                            break;
                        case GLFW_KEY_S:
                            _vis_window->get_render_config().light_opts.point_light.position.z() -= 0.05f; // backward
                            break;
                        case GLFW_KEY_UP:
                            _vis_window->get_render_config().light_opts.point_light.position.y() -= 0.05f; // up
                            break;
                        case GLFW_KEY_DOWN:
                            _vis_window->get_render_config().light_opts.point_light.position.y() += 0.05f; // down
                            break;
                        }
                    }
                });

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
                _vis_window->add_render_object(new_obj);
                _obj_texcolor_mesh = new_obj;
            }
        }

        void destroy()
        {
            LOG_TRACE("%s() ENTER", __func__);

            _vis_window->destroy_window();
            _vis_window.reset();

            LOG_TRACE("%s() LEAVE", __func__);
        }

        void run()
        {
            LOG_TRACE("%s() ENTER", __func__);

            while (_vis_window->poll_events())
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

                _vis_window->render();
            } // while

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