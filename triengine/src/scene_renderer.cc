#include "scene_renderer.hh"
#include "misc/string_utils.hh"
#include "misc/debug_utils.hh"
#include "misc/gl_utils.hh"

namespace triengine
{
    void scene_renderer::create(gl_context* glctx)
    {
        // Context Settings
        GLCall(::glEnable(GL_DEPTH_TEST));
        //GLCall(::glEnable(GL_MULTISAMPLE));
        GLCall(::glDisable(GL_BLEND));
        GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
        GLCall(::glClearDepth(1.0f));

        _infgrid_renderer.create(glctx->get_glfw_window());
        _light_source_renderer.create(glctx->get_glfw_window());
        _mesh_renderer.create(glctx->get_glfw_window());
        _lineset_renderer.create(glctx->get_glfw_window());
        _pcd_renderer.create(glctx->get_glfw_window());
        _skeleton_renderer.create(glctx->get_glfw_window());

        _point_light_source_object = geometry::light_source_object::create(0.1f);
        _point_light_source_object->set_visible(false);
    }

    void scene_renderer::destroy()
    {
        _infgrid_renderer.destroy();
        _light_source_renderer.destroy();
        _mesh_renderer.destroy();
        _lineset_renderer.destroy();
        _pcd_renderer.destroy();
        _skeleton_renderer.destroy();
    }

    void scene_renderer::render_scene(scene& scn)
    {
        const camera& scn_camera = *scn.get_camera();
        const view_port viewport = scn_camera.get_view_port();

        // Change view port
        GLCall(::glViewport(viewport.x, viewport.y, viewport.width, viewport.height));

        const auto& bg_color = scn.render_config.bg_color;
        GLCall(::glClearColor(bg_color.r(), bg_color.g(), bg_color.b(), bg_color.a())); // set clear color
        GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // clear render buffers

        GLCall(::glPolygonMode(GL_FRONT_AND_BACK, scn.render_config.show_wireframe ? GL_LINE : GL_FILL));

        if (scn.render_config.light_opts.dir_light.follow_camera) {
            scn.render_config.light_opts.dir_light.direction = scn_camera.get_direction();
        }

        renderer::render_context render_ctx; {
            scn_camera.get_view_projection(render_ctx.view, render_ctx.projection);
            render_ctx.light_opts = &scn.render_config.light_opts;
            render_ctx.camera = &scn_camera;
        }

        _point_light_source_object->set_visible(scn.render_config.light_opts.point_light.enabled && scn.render_config.light_opts.point_light.show_light_source);
        _point_light_source_object->translate(scn.render_config.light_opts.point_light.position);
        _point_light_source_object->color = scn.render_config.light_opts.point_light.color;

        //_origin_axis_frame_object->set_visible(scn.scn_config.show_origin_axis);

        _lineset_renderer.render(render_ctx, scn.lineset_objects);

        _mesh_renderer.enable_object_normal_rendering(scn.render_config.show_object_normals);
        _mesh_renderer.render(render_ctx, scn.mesh_objects);

        _light_source_renderer.render(render_ctx, { _point_light_source_object });

        if (scn.render_config.pcd_point_size) {
            _pcd_renderer.set_pcd_point_size(scn.render_config.pcd_point_size.value());
        }
        _pcd_renderer.render(render_ctx, scn.pcd_objects);

        _infgrid_renderer.set_options(scn.render_config.infgrid_opts);
        if (scn.render_config.show_origin_xz_grid) {
            // NOTE: The infinite grid renderer must be rendered last to allow for alpha-blending.
            //       (except the skeleton renderer, which sometimes causes the depth buffer to be reset).
            _infgrid_renderer.render(render_ctx);
        }

        if (scn.render_config.skeleton_mode == scene_render_config::skeleton_render_mode::skeleton_overlay ||
            scn.render_config.skeleton_mode == scene_render_config::skeleton_render_mode::overlay_with_joint_axis)
        {
            GLCall(::glClear(GL_DEPTH_BUFFER_BIT)); // Enable skeleton overlay
        }

        _skeleton_renderer.show_joint_axis(scn.render_config.skeleton_mode == scene_render_config::skeleton_render_mode::overlay_with_joint_axis);
        _skeleton_renderer.render(render_ctx, scn.skeleton_objects);
    }

} // namespace