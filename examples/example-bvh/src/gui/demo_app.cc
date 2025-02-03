#pragma once
#include "demo_app.hh"

#include <utils/scrolling_buffer.hh>
#include <utils/bit_cast.hh>

#include <memory>
#include <array>
#include <magic_enum.hpp>

#include <triengine/math.hh>
#include <triengine/io/file_obj_loader.hh>

namespace gui
{
    void demo_app::set_close_callback(triengine::visualizer_window::close_callback cb)
    {
        _off_window->set_close_callback(std::move(cb));
    }

    void demo_app::set_key_callback(triengine::visualizer_window::key_callback cb)
    {
        _off_window->set_key_callback(std::move(cb));
    }

    void demo_app::create(
        const std::filesystem::path& triengine_resource_dir)
    {
        LOG_TRACE("%s() ENTER", __func__);

        _off_window = std::make_unique<triengine::visualizer_window>();
        _off_window->create_window("Triengine Orbbec Demo"
            " (Build: " __DATE__ ", " __TIME__
#if defined (_DEBUG)
            " DBG"
#else  // ^^^ _DEBUG ^^^ / vvv !_DEBUG vvv
            " REL"
#endif // ^^^ !_DEBUG ^^^
            ")"
        );

        _scene = _off_window->get_current_scene();
        _scene->render_config.pcd_point_size = 2.5f;
        _scene->render_config.show_origin_xz_grid = true;
        _scene->render_config.light_opts.point_light.position = triengine::vec3_f32{ 0.0f, -1.5f, -1.5f };
        _scene->get_camera()->set_mirror_mode(false);

        _off_window->set_key_callback(
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
                        _off_window->enable_main_menu(!_off_window->is_main_menu_enabled());
                        break;
                    case GLFW_KEY_A:
                        _scene->render_config.light_opts.point_light.position.x() -= 0.05f; // left
                        break;
                    case GLFW_KEY_D:
                        _scene->render_config.light_opts.point_light.position.x() += 0.05f; // right
                        break;
                    case GLFW_KEY_W:
                        _scene->render_config.light_opts.point_light.position.z() += 0.05f; // forward
                        break;
                    case GLFW_KEY_S:
                        _scene->render_config.light_opts.point_light.position.z() -= 0.05f; // backward
                        break;
                    case GLFW_KEY_UP:
                        _scene->render_config.light_opts.point_light.position.y() -= 0.05f; // up
                        break;
                    case GLFW_KEY_DOWN:
                        _scene->render_config.light_opts.point_light.position.y() += 0.05f; // down
                        break;
                    }
                }
            });

        // add render stats window
        _render_stats_window = std::make_shared<triengine::gui::render_stats_window>();
        _render_stats_window->set_visible(false);
        _off_window->add_gui_window(_render_stats_window);

        // add log window
        _log_window = std::make_shared<triengine::gui::log_window>();
        _log_window->set_visible(false);
        _off_window->add_gui_window(_log_window);

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

        //{
        //    auto obj_axis_frame = triengine::geometry::triangle_mesh_object::create_coordinate_frame();

        //    //obj_axis_frame->paint_uniform_color(_get_next_color());
        //    obj_axis_frame->translate(Eigen::Vector3f{ 1.8f, 0.0f, -1.5f });

        //    _scene->add_object(obj_axis_frame);
        //}
    }

    void demo_app::destroy()
    {
        LOG_TRACE("%s() ENTER", __func__);

        _log_window.reset();
        _render_stats_window.reset();
        
        _off_window->destroy_window();
        _off_window.reset();

        LOG_TRACE("%s() LEAVE", __func__);
    }

    void demo_app::run()
    {
        LOG_TRACE("%s() ENTER", __func__);

        LOG_INFO("polling start..");
        while (_off_window->update_window())
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

            this->_render();
        } // while

        LOG_TRACE("%s() LEAVE", __func__);
    }

    void demo_app::_render()
    {
        _off_window->render();
    }

} // namespace