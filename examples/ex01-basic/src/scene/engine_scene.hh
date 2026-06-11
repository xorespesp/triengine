#pragma once
#include "../scene_wrapper.hh"

#include <triengine/math/math3d.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/io/file_obj_loader.hh>

namespace demo::scene
{
    using namespace triengine;

    class engine_scene
        : public scene_wrapper
    {
        std::shared_ptr<geometry::mesh_object> _engine_mesh;
        color3_f32 _engine_color{ 0.8f, 0.8f, 0.8f }; // base color applied on demand via the gui

    public:
        engine_scene(
            visualization::visualizer& vis)
            : scene_wrapper(vis.add_scene())
        {
            const auto rsrc_dir_path = global_options::instance()->get_resource_directory();
            auto scn = this->get_scene();
            scn->set_name("engine");

            scn->get_render_config()->show_origin_xz_grid = false;
            scn->get_render_config()->bg_color = color4_f32{ 0.08f, 0.08f, 0.08f, 1.0f };
            scn->get_render_config()->light_opts.point_light.enabled = false;
            scn->get_render_config()->light_opts.dir_light.enabled = true;
            scn->get_render_config()->light_opts.dir_light.follow_camera = true;
            scn->get_render_config()->light_opts.dir_light.ambient_intensity = 0.1f;
            scn->get_render_config()->light_opts.dir_light.diffuse_intensity = 0.45f;
            scn->get_render_config()->light_opts.dir_light.specular_intensity = 1.5f;

            scn->switch_camera_type(triengine::camera_type::fly);
            auto fly_cam = scn->get_camera()->as<triengine::fly_camera>();
            fly_cam->set_position(vec3_f32{ 0.0f, 1.0f, -9.5f });
            fly_cam->set_direction(vec3_f32{ 0.0f, 0.0f, 1.0f });

            _engine_mesh = std::make_shared<geometry::mesh_object>();

            if (io::load_mesh_from_obj(
                rsrc_dir_path / "objects/car_engine/car_engine.obj",
                false,
                *scn,
                *_engine_mesh
            ))
            {
                //_engine_mesh->compute_vertex_normals();
                _engine_mesh->set_model(
                    math::scale(_engine_mesh->get_model(), vec3_f32{ 0.0125f, 0.0125f, 0.0125f })
                );

                _engine_mesh->apply_model_in_place();
                _engine_mesh->translate(vec3_f32(0.0f, 0.5f, 0.0f), true);
                _engine_mesh->get_vertex_shading_material()->specular_intensity = 1.0f;
                _engine_mesh->get_vertex_shading_material()->alpha = 0.4f;
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

            R = Eigen::AngleAxisf(math::deg2rad(-90.0f), Eigen::Vector3f::UnitX()) *
                Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitZ());

            if (_engine_mesh) {
                _engine_mesh->rotate(R);
            }
        }

        void render_gui(
            [[maybe_unused]] const gui::window_render_context& render_ctx) override
        {
            ImGui::Text("engine scene gui!");

            using lighting_mode = geometry::mesh_object::lighting_mode;
            auto* const mat = _engine_mesh->get_vertex_shading_material();

            // Lighting mode (lit / unlit)
            const char* const lighting_items[] = { "Lit", "Unlit" };
            int lighting_idx = (_engine_mesh->get_lighting_mode() == lighting_mode::unlit) ? 1 : 0;
            if (ImGui::Combo("lighting", &lighting_idx, lighting_items, IM_ARRAYSIZE(lighting_items))) {
                _engine_mesh->set_lighting_mode(lighting_idx == 1 ? lighting_mode::unlit : lighting_mode::lit);
            }

            // Engine base color: valid in both lit and unlit
            if (ImGui::ColorEdit3("color", _engine_color.data())) {
                _engine_mesh->paint_uniform_color(_engine_color);
                _engine_mesh->mark_dirty();
            }

            // Phong material: only meaningful when lit
            if (_engine_mesh->get_lighting_mode() == lighting_mode::lit) {
                ImGui::DragFloat("ambient", &mat->ambient_intensity, 0.01f, 0.0f, 4.0f);
                ImGui::DragFloat("diffuse", &mat->diffuse_intensity, 0.01f, 0.0f, 4.0f);
                ImGui::DragFloat("specular", &mat->specular_intensity, 0.01f, 0.0f, 4.0f);
                int shininess = static_cast<int>(mat->shininess);
                if (ImGui::SliderInt("shininess", &shininess, 1, 256)) {
                    mat->shininess = static_cast<uint16_t>(shininess);
                }
            }

            // Alpha: valid in both lit and unlit; < 1.0 routes the mesh through the WBOIT transparent pass
            ImGui::DragFloat("alpha", &mat->alpha, 0.01f, 0.0f, 1.0f);
        }

    }; // class

} // namespace