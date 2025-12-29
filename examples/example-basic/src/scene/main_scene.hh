#pragma once
#include "../scene_wrapper.hh"

#include <triengine/math/math3d.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/io/file_obj_loader.hh>

namespace demo::scene
{
    using namespace triengine;

    class main_scene
        : public scene_wrapper
    {
        std::shared_ptr<geometry::mesh_object> _skull_mesh;
        std::shared_ptr<geometry::mesh_object> _skull_mesh2;

    public:
        main_scene(
            visualization::visualizer& vis)
            : scene_wrapper(vis.add_scene())
        {
            const auto rsrc_dir_path = global_options::instance()->get_resource_directory();
            auto scn = this->get_scene();
            scn->set_name("main");

            scn->get_render_config()->show_origin_xz_grid = true;
            scn->get_render_config()->light_opts.point_light.position = vec3_f32{ 0.0f, 1.5f, -1.5f };
            scn->get_render_config()->light_opts.point_light.ambient_intensity = 0.0f;
            scn->get_render_config()->light_opts.point_light.diffuse_intensity = 2.5f;
            scn->get_render_config()->light_opts.point_light.specular_intensity = 1.35f;

            scn->switch_camera_type(triengine::camera_type::arcball);

            scn->add_label_3d("+X", { 0.6f, 0.0f, 0.0f }, 0.4f, color3_f32(1.0f, 0.0f, 0.0f));
            scn->add_label_3d("+Y", { 0.0f, 0.6f, 0.0f }, 0.4f, color3_f32(0.0f, 1.0f, 0.0f));
            scn->add_label_3d("+Z", { 0.0f, 0.0f, 0.6f }, 0.4f, color3_f32(0.0f, 0.0f, 1.0f));

            auto mesh_axis_frame = geometry::mesh_object::create_coordinate_frame(0.5f);
            //mesh_axis_frame->paint_uniform_color(_get_next_color());
            //mesh_axis_frame->translate(Eigen::Vector3f{ 1.8f, 0.0f, -1.5f });
            scn->add_geometry(mesh_axis_frame);

            _skull_mesh = std::make_shared<geometry::mesh_object>();
            if (io::load_mesh_from_obj(
                rsrc_dir_path / "objects/skull/12140_Skull_v3_L2.obj",
                false,
                *scn,
                *_skull_mesh
            ))
            {
                //_skull_mesh->compute_vertex_normals();

                _skull_mesh->set_model(
                    math::scale(_skull_mesh->get_model(), vec3_f32(0.0125f, 0.0125f, 0.0125f))
                );
                Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
                R = Eigen::AngleAxisf(math::deg2rad(0.0f), Eigen::Vector3f::UnitZ())
                    * Eigen::AngleAxisf(math::deg2rad(180.0f), Eigen::Vector3f::UnitY())
                    * Eigen::AngleAxisf(math::deg2rad(-90.0f), Eigen::Vector3f::UnitX());
                _skull_mesh->rotate(R, true);
                _skull_mesh->apply_model_in_place();

                _skull_mesh->translate(vec3_f32(0.4f, 0.8f, 0.0f), true);

                scn->add_geometry(_skull_mesh);
            }

            _skull_mesh2 = std::make_shared<geometry::mesh_object>();
            if (io::load_mesh_from_obj(
                rsrc_dir_path / "objects/skull/12140_Skull_v3_L2.obj",
                false,
                *scn,
                *_skull_mesh2
            ))
            {
                //_skull_mesh2->compute_vertex_normals();
                _skull_mesh2->set_model(
                    math::scale(_skull_mesh2->get_model(), vec3_f32(0.0125f, 0.0125f, 0.0125f))
                );
                Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
                R = Eigen::AngleAxisf(math::deg2rad(0.0f), Eigen::Vector3f::UnitZ())
                    * Eigen::AngleAxisf(math::deg2rad(180.0f), Eigen::Vector3f::UnitY())
                    * Eigen::AngleAxisf(math::deg2rad(-90.0f), Eigen::Vector3f::UnitX());
                _skull_mesh2->rotate(R, true);
                _skull_mesh2->apply_model_in_place();
                _skull_mesh2->translate(vec3_f32(0.0f, 0.8f, 0.0f), true);
                _skull_mesh2->get_texture_shading_material()->alpha = 0.5f;

                scn->add_geometry(_skull_mesh2);
            }
        }

        void update_animation() override
        {
            constexpr float rotSpeed = math::pi<float>() / 8.0f;
            const float dT = static_cast<float>(::glfwGetTime());

            Eigen::Matrix3f R1, R2;
            R1 = Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitY());
            R2 = Eigen::AngleAxisf(rotSpeed * dT + math::deg2rad(90.0f), Eigen::Vector3f::UnitY());

            if (_skull_mesh) { _skull_mesh->rotate(R1); }
            if (_skull_mesh2) { _skull_mesh2->rotate(R2); }
        }

        void render_gui([[maybe_unused]] const gui::window_render_context& render_ctx) override
        {
            ImGui::Text("main scene gui!");
        }
    };

} // namespace