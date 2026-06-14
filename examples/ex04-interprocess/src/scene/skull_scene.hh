#pragma once
// offscreen_renderer_dx.hh pulls in <windows.h>; include it before any header that pulls in
// GLFW (via the scene/GL chain) so <windows.h> defines APIENTRY first and GLFW does not
// redefine it (avoids C4005).
#include <triengine/visualization/offscreen_renderer_dx.hh>

#include "../scene_wrapper.hh"

#include <triengine/global_options.hh>
#include <triengine/math/math3d.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/io/file_obj_loader.hh>

namespace scene
{
    using namespace triengine;

    // A rotating skull mesh (with a translucent second copy) lit by a point light, viewed
    // with an arcball camera.
    class skull_scene
        : public scene_wrapper
    {
    public:
        explicit skull_scene(visualization::offscreen_renderer_dx& renderer)
            : scene_wrapper(renderer.add_scene())
        {
            const auto rsrc_dir_path = global_options::instance()->get_resource_directory();
            auto scn = this->get_scene();
            scn->set_name("skull");

            scn->get_render_config()->show_origin_xz_grid = true;
            scn->get_render_config()->light_opts.point_light.position = vec3_f32{ 0.0f, 1.5f, -1.5f };
            scn->get_render_config()->light_opts.point_light.ambient_intensity = 0.0f;
            scn->get_render_config()->light_opts.point_light.diffuse_intensity = 2.5f;
            scn->get_render_config()->light_opts.point_light.specular_intensity = 1.35f;

            scn->switch_camera_type(camera_type::arcball);
            scn->get_camera()->as<arcball_camera>()->get_options().damping_factor = 11.0f;

            auto mesh_axis_frame = geometry::mesh_object::create_coordinate_frame(0.5f);
            scn->add_geometry(mesh_axis_frame);

            // Z-Y-X (Yaw-Pitch-Roll) order, shared by both skull copies.
            Eigen::Matrix3f R;
            R = Eigen::AngleAxisf(math::deg2rad(0.0f), Eigen::Vector3f::UnitZ())
                * Eigen::AngleAxisf(math::deg2rad(180.0f), Eigen::Vector3f::UnitY())
                * Eigen::AngleAxisf(math::deg2rad(-90.0f), Eigen::Vector3f::UnitX());

            _skull_mesh = std::make_shared<geometry::mesh_object>();
            if (io::load_mesh_from_obj(
                rsrc_dir_path / "objects/skull/12140_Skull_v3_L2.obj",
                false,
                *scn,
                *_skull_mesh))
            {
                _skull_mesh->set_model(
                    math::scale(_skull_mesh->get_model(), vec3_f32(0.0125f, 0.0125f, 0.0125f)));
                _skull_mesh->apply_model_in_place();
                _skull_mesh->rotate(R, true);
                _skull_mesh->translate(vec3_f32(0.0f, 0.5f, 0.0f), true);
                scn->add_geometry(_skull_mesh);
            }

            _skull_mesh2 = std::make_shared<geometry::mesh_object>();
            if (io::load_mesh_from_obj(
                rsrc_dir_path / "objects/skull/12140_Skull_v3_L2.obj",
                false,
                *scn,
                *_skull_mesh2))
            {
                _skull_mesh2->set_model(
                    math::scale(_skull_mesh2->get_model(), vec3_f32(0.0125f, 0.0125f, 0.0125f)));
                _skull_mesh2->get_texture_shading_material()->alpha = 0.5f;
                _skull_mesh2->apply_model_in_place();
                _skull_mesh2->rotate(R, true);
                _skull_mesh2->translate(vec3_f32(0.0f, 0.6f, 0.0f), true);
                scn->add_geometry(_skull_mesh2);
            }
        }

        void update([[maybe_unused]] vec2_i32 frame_size) override
        {
            constexpr float rotSpeed = math::pi<float>() / 8.0f;
            const float dT = static_cast<float>(::glfwGetTime());

            Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
            R = Eigen::AngleAxisf(math::deg2rad(-90.0f), Eigen::Vector3f::UnitX()) *
                Eigen::AngleAxisf(rotSpeed * dT, Eigen::Vector3f::UnitZ());

            if (_skull_mesh) {
                _skull_mesh->rotate(R);
            }
        }

    private:
        std::shared_ptr<geometry::mesh_object> _skull_mesh;
        std::shared_ptr<geometry::mesh_object> _skull_mesh2;

    }; // class

} // namespace
