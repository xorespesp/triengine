#pragma once
#include "../scene_wrapper.hh"

#include <triengine/math/math3d.hh>
#include <triengine/geometry/triangle_mesh_object.hh>
#include <triengine/io/file_obj_loader.hh>

namespace demo::scene
{
    using namespace triengine;

    class engine_scene
        : public scene_wrapper
    {
        std::shared_ptr<geometry::triangle_mesh_object> _engine_mesh;

    public:
        engine_scene(
            visualization::visualizer& vis)
            : scene_wrapper(vis.add_scene())
        {
            const auto rsrc_dir_path = global_options::instance()->get_resource_directory();
            auto scn = this->get_scene();
            scn->set_name("engine");

            scn->get_render_config()->show_origin_xz_grid = false;
            scn->get_render_config()->bg_color = color4_f32{ 1.0f, 1.0f, 1.0f, 1.0f };
            scn->get_render_config()->light_opts.point_light.enabled = false;
            scn->get_render_config()->light_opts.dir_light.ambientIntensity = 0.1f;
            scn->get_render_config()->light_opts.dir_light.diffuseIntensity = 0.45f;
            scn->get_render_config()->light_opts.dir_light.specularIntensity = 3.0f;

            scn->get_camera()->set_position(vec3_f32{ 0.0f, -2.2326f, -11.1618f });
            scn->get_camera()->set_mirror_mode(false);

            _engine_mesh = std::make_shared<geometry::triangle_mesh_object>();

            if (io::load_triangle_mesh_from_obj(
                rsrc_dir_path / "objects/car_engine/car_engine.obj",
                false,
                *_engine_mesh
            ))
            {
                //_engine_mesh->compute_vertex_normals();
                _engine_mesh->set_model(
                    math::scale(_engine_mesh->get_model(), vec3_f32{ 0.0125f, 0.0125f, 0.0125f })
                );

                _engine_mesh->apply_model_in_place();
                _engine_mesh->translate(vec3_f32(0.0f, -0.5f, 0.0f), true);

                scn->add_geometry(_engine_mesh);
            }
        }

        void update_animation() override
        {
            constexpr float rotSpeed = math::pi<float>() / 8.0f;
            const float dT = static_cast<float>(::glfwGetTime());

            Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
            //R = Eigen::AngleAxisf(rotSpeed * dT * 0.1f, Eigen::Vector3f::UnitZ()) *
            //    Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitY()) *
            //    Eigen::AngleAxisf(rotSpeed * dT * 0.5f, Eigen::Vector3f::UnitX());

            R = Eigen::AngleAxisf(math::deg2rad(90.0f), Eigen::Vector3f::UnitX()) *
                Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitZ());

            if (_engine_mesh) {
                _engine_mesh->rotate(R);
            }
        }

        void render_gui(
            [[maybe_unused]] const gui::window_render_context& render_ctx) override
        {
            ImGui::Text("engine scene gui!");
        }

    }; // class

} // namespace