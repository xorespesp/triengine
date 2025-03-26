#include "scene_renderer.hh"
#include "misc/string_utils.hh"
#include "misc/debug_utils.hh"
#include "misc/gl_utils.hh"
#include "shaders/includes/phong_lighting_shaders.h"
#include "shaders/overlay_composite_shaders.h"

namespace triengine
{
    namespace
    {
        struct quad_vertex_t {
            vec3_f32 position; // vertex position
            vec2_f32 uv; // texture coordinate (uv coordinate)
        };
        static_assert(sizeof(quad_vertex_t) == 5 * sizeof(float), "!!");

        const std::array<quad_vertex_t, 6> quadVertices = {
            quad_vertex_t{ vec3_f32{ -1.0f, -1.0f, 0.0f }, vec2_f32{ 0.0f, 0.0f } },
            quad_vertex_t{ vec3_f32{  1.0f, -1.0f, 0.0f }, vec2_f32{ 1.0f, 0.0f } },
            quad_vertex_t{ vec3_f32{  1.0f,  1.0f, 0.0f }, vec2_f32{ 1.0f, 1.0f } },
            quad_vertex_t{ vec3_f32{  1.0f,  1.0f, 0.0f }, vec2_f32{ 1.0f, 1.0f } },
            quad_vertex_t{ vec3_f32{ -1.0f,  1.0f, 0.0f }, vec2_f32{ 0.0f, 1.0f } },
            quad_vertex_t{ vec3_f32{ -1.0f, -1.0f, 0.0f }, vec2_f32{ 0.0f, 0.0f } }
        };

    } // namespace

    void scene_renderer::create(gl_context* glctx)
    {
        // Context Settings
        GLCall(::glEnable(GL_DEPTH_TEST));
        //GLCall(::glEnable(GL_MULTISAMPLE));
        GLCall(::glDisable(GL_BLEND));
        GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
        GLCall(::glClearDepth(1.0f));

        _shader_prep = std::make_unique<shader_preprocessor>();
        _shader_prep->register_system_include_from_memory("phong_lighting", shaders::includes::kPhongLightingShaders);

        _infgrid_renderer.create(*glctx, *_shader_prep);
        _light_source_renderer.create(*glctx, *_shader_prep);
        _mesh_renderer.create(*glctx, *_shader_prep);
        _lineset_renderer.create(*glctx, *_shader_prep);
        _pcd_renderer.create(*glctx, *_shader_prep);
        _skeleton_renderer.create(*glctx, *_shader_prep);

        _wboit_composite_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kWBOITCompositeVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kWBOITCompositeFragmentShader).c_str() })
            .link();

        _screen_quad_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kScreenQuadVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kScreenQuadFragmentShader).c_str() })
            .link();

        _overlay_composite_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kOverlayCompositeVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kOverlayCompositeFragmentShader).c_str() })
            .link();

        // Create screen-quad VAO
        {
            GLCall(::glGenVertexArrays(1, &_vao_screen_quad));
            GLCall(::glGenBuffers(1, &_vbo_screen_quad));
            GLCall(::glBindVertexArray(_vao_screen_quad));
            GLCall(::glBindBuffer(GL_ARRAY_BUFFER, _vbo_screen_quad));
            GLCall(::glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertex_t) * quadVertices.size(), quadVertices.data(), GL_STATIC_DRAW));
            GLCall(::glEnableVertexAttribArray(0));
            GLCall(::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(quad_vertex_t), (void*)offsetof(quad_vertex_t, position)));
            GLCall(::glEnableVertexAttribArray(1));
            GLCall(::glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(quad_vertex_t), (void*)offsetof(quad_vertex_t, uv)));
            GLCall(::glBindVertexArray(0));
        }

        constexpr size_t kMaxPointLights = 1;
        for (size_t i = 0; i < kMaxPointLights; ++i) {
            _light_source_objects.push_back(geometry::light_source_object::create(0.1f));
            _light_source_objects.back()->set_visible(false);
        }
    }

    void scene_renderer::destroy()
    {
        _infgrid_renderer.destroy();
        _light_source_renderer.destroy();
        _mesh_renderer.destroy();
        _lineset_renderer.destroy();
        _pcd_renderer.destroy();
        _skeleton_renderer.destroy();

        _wboit_composite_shader.destroy();
        _screen_quad_shader.destroy();
        _overlay_composite_shader.destroy();

        if (_vao_screen_quad) { ::glDeleteVertexArrays(1, &_vao_screen_quad); }
        if (_vbo_screen_quad) { ::glDeleteBuffers(1, &_vbo_screen_quad); }
    }

    void scene_renderer::render(
        const frame_buffer& target_fb,
        scene& scn)
    {
        const camera& scn_camera = *scn.get_camera();
        const view_port viewport = scn_camera.get_view_port();

        {
            if (scn.render_config.light_opts.dir_light.follow_camera) {
                scn.render_config.light_opts.dir_light.direction = scn_camera.get_direction();
            }

            {
                auto& light_source_obj = *_light_source_objects.back();
                light_source_obj.set_visible(
                    scn.render_config.light_opts.point_light.enabled &&
                    scn.render_config.light_opts.point_light.show_light_source
                );
                light_source_obj.translate(scn.render_config.light_opts.point_light.position);
                light_source_obj.color = scn.render_config.light_opts.point_light.color;
            }

            if (scn.render_config.pcd_point_size) { _pcd_renderer.set_pcd_point_size(scn.render_config.pcd_point_size.value()); }
            _mesh_renderer.enable_object_normal_rendering(scn.render_config.show_object_normals);
            _skeleton_renderer.show_joint_axis(scn.render_config.skeleton_mode == scene_render_config::skeleton_render_mode::overlay_with_joint_axis);
            _infgrid_renderer.set_options(scn.render_config.infgrid_opts);
        }

        renderer::render_context render_ctx; {
            scn_camera.get_view_projection(render_ctx.view, render_ctx.projection);
            render_ctx.light_opts = &scn.render_config.light_opts;
            render_ctx.camera = &scn_camera;
        }

        // set viewport (global state)
        GLCall(::glViewport(viewport.x, viewport.y, viewport.width, viewport.height));

        if (!scn.render_config.show_wireframe)
        {
            // set polygon mode (global state)
            GLCall(::glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));

            // NOTE: currently, overlay render pass is only for the skeleton_renderer.
            const bool overlay_render_pass_required =
                !scn.skeleton_objects.empty() &&
                (scn.render_config.skeleton_mode == scene_render_config::skeleton_render_mode::skeleton_overlay ||
                 scn.render_config.skeleton_mode == scene_render_config::skeleton_render_mode::overlay_with_joint_axis);

            if (!_wboit_fb.is_valid())
            {
                _wboit_fb = frame_buffer::create_color_depth_stencil_buffer(
                    { 
                        GL_RGBA16F/* GL_COLOR_ATTACHMENT0: WBOIT opaque */,
                        GL_RGBA16F/* GL_COLOR_ATTACHMENT1: WBOIT accum */, 
                        GL_R8     /* GL_COLOR_ATTACHMENT2: WBOIT reveal */
                    },
                    GL_DEPTH_COMPONENT24,
                    GL_STENCIL_INDEX8,
                    target_fb.width_pixels(),
                    target_fb.height_pixels()
                );
            }
            else
            {
                // It is okay to call reallocate every frame, 
                // as there is an internal reallocation-skip optimization implemented.
                _wboit_fb.reallocate(
                    target_fb.width_pixels(),
                    target_fb.height_pixels()
                );
            }

            const auto
                * opaque_color_attach = _wboit_fb.color_attachment(0),
                * accum_color_attach = _wboit_fb.color_attachment(1),
                * reveal_color_attach = _wboit_fb.color_attachment(2);

            if (overlay_render_pass_required)
            {
                if (!_overlay_fb.is_valid())
                {
                    _overlay_fb = frame_buffer::create_color_depth_only_buffer(
                        GL_RGBA16F,
                        GL_DEPTH_COMPONENT24,
                        target_fb.width_pixels(),
                        target_fb.height_pixels()
                    );
                }
                else
                {
                    // It is okay to call reallocate every frame, 
                    // as there is an internal reallocation-skip optimization implemented.
                    _overlay_fb.reallocate(
                        target_fb.width_pixels(),
                        target_fb.height_pixels()
                    );
                }
            }

            // bind WBOIT framebuffer
            GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _wboit_fb.fbo_id()));

            // ---------------------------------------------------------------------------------
            // WBOIT solid pass
            // ---------------------------------------------------------------------------------
            {
                render_ctx.curr_render_pass = renderer::render_pass_type::wboit_solid_rendering;

                // Explicitly specifies that the drawing buffer of the 
                // currently bound framebuffer is an opaque color buffer.
                GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT0/* opaque color buffer */));

                // configure render states (global state)
                GLCall(::glEnable(GL_DEPTH_TEST));
                GLCall(::glDepthFunc(GL_LESS));
                GLCall(::glDepthMask(GL_TRUE)); // enable depth buffer writes so glClear won't ignore clearing the depth buffer
                GLCall(::glDisable(GL_BLEND));
                GLCall(::glClearColor(
                    scn.render_config.bg_color.r(), 
                    scn.render_config.bg_color.g(), 
                    scn.render_config.bg_color.b(),
                    scn.render_config.bg_color.a())
                );

                // clear frame buffers
                GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

                // render solid(opaque) objects
                _lineset_renderer.render(render_ctx, scn.lineset_objects);
                _pcd_renderer.render(render_ctx, scn.pcd_objects);
                _light_source_renderer.render(render_ctx, _light_source_objects);
                _mesh_renderer.render(render_ctx, scn.mesh_objects);
                if (!overlay_render_pass_required) {
                    _skeleton_renderer.render(render_ctx, scn.skeleton_objects);
                }
            }

            // ---------------------------------------------------------------------------------
            // WBOIT transparent pass
            // ---------------------------------------------------------------------------------
            {
                render_ctx.curr_render_pass = renderer::render_pass_type::wboit_transparent_rendering;

                // Explicitly specifies that the drawing buffer of the 
                // currently bound framebuffer is an accum & reveal color buffer.
                const std::array<GLenum, 2> curr_draw_buffers = {
                    GL_COLOR_ATTACHMENT1/* [0]: accum color buffer */,
                    GL_COLOR_ATTACHMENT2/* [1]: reveal color buffer */
                };
                GLCall(::glDrawBuffers(static_cast<GLsizei>(curr_draw_buffers.size()), curr_draw_buffers.data()));

                static const vec4_f32
                    accum_fill_vec(0.0f, 0.0f, 0.0f, 0.0f),
                    reveal_fill_vec(1.0f, 1.0f, 1.0f, 1.0f);

                // configure render states (global state)
                GLCall(::glDepthMask(GL_FALSE)); // disable depth buffer writes
                GLCall(::glEnable(GL_BLEND));
                GLCall(::glBlendFunci(0/* index of the draw buffer; [0]: accum color buffer */, GL_ONE, GL_ONE));
                GLCall(::glBlendFunci(1/* index of the draw buffer; [1]: reveal color buffer */, GL_ZERO, GL_ONE_MINUS_SRC_COLOR));
                GLCall(::glBlendEquation(GL_FUNC_ADD));

                // clear accum & reveal color buffers
                GLCall(::glClearBufferfv(GL_COLOR, 0/* index of the draw buffer; [0]: accum color buffer */, accum_fill_vec.data()));
                GLCall(::glClearBufferfv(GL_COLOR, 1/* index of the draw buffer; [1]: reveal color buffer */, reveal_fill_vec.data()));

                // render transparent objects
                _pcd_renderer.render(render_ctx, scn.pcd_objects);

                _mesh_renderer.enable_object_normal_rendering(false); // override setting
                _mesh_renderer.render(
                    render_ctx,
                    scn.mesh_objects
                );

                if (scn.render_config.show_origin_xz_grid) {
                    // NOTE: The infinite grid renderer must be rendered last to allow for alpha-blending.
                    //       (except the skeleton renderer, which sometimes causes the depth buffer to be reset).
                    _infgrid_renderer.render(render_ctx);
                }
            }

            // ---------------------------------------------------------------------------------
            // WBOIT composite pass (render composite image)
            // ---------------------------------------------------------------------------------
            {
                // Explicitly specifies that the drawing buffer of the 
                // currently bound framebuffer is an opaque color buffer.
                GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT0/* opaque color buffer */));

                // configure render states (global state)
                GLCall(::glDepthFunc(GL_ALWAYS));
                GLCall(::glEnable(GL_BLEND));
                GLCall(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

                // use composite shader
                _wboit_composite_shader.use();

                // draw screen quad to opaque color buffer
                GLCall(::glActiveTexture(GL_TEXTURE0));
                GLCall(::glBindTexture(GL_TEXTURE_2D, accum_color_attach->buffer_id));
                GLCall(::glActiveTexture(GL_TEXTURE1));
                GLCall(::glBindTexture(GL_TEXTURE_2D, reveal_color_attach->buffer_id));
                GLCall(::glBindVertexArray(_vao_screen_quad));
                GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
            }

            // ---------------------------------------------------------------------------------
            // overlay rendering pass (currently, this render pass is only for the skeleton_renderer.)
            // ---------------------------------------------------------------------------------
            if (overlay_render_pass_required)
            {
                render_ctx.curr_render_pass = renderer::render_pass_type::wboit_solid_rendering;

                // configure render states (global state)
                GLCall(::glEnable(GL_DEPTH_TEST));
                GLCall(::glDepthFunc(GL_LESS));
                GLCall(::glDepthMask(GL_TRUE)); // enable depth buffer writes so glClear won't ignore clearing the depth buffer
                GLCall(::glEnable(GL_BLEND));
                GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 0.0f));

                // bind & clear overlay framebuffer
                GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _overlay_fb.fbo_id()));
                GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

                // render skeletons to overlay color buffer
                _skeleton_renderer.render(render_ctx, scn.skeleton_objects);

                // bind WBOIT framebuffer back
                GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _wboit_fb.fbo_id()));

                _overlay_composite_shader.use();
                GLCall(::glActiveTexture(GL_TEXTURE0));
                GLCall(::glBindTexture(GL_TEXTURE_2D, _overlay_fb.color_attachment()->buffer_id));
                GLCall(::glBindVertexArray(_vao_screen_quad));
                GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
            }

            // ---------------------------------------------------------------------------------
            // final pass (draw to backbuffer)
            // ---------------------------------------------------------------------------------
            {
                // configure render states (global state)
                GLCall(::glDisable(GL_DEPTH_TEST));
                GLCall(::glDepthMask(GL_TRUE)); // enable depth buffer writes so glClear won't ignore clearing the depth buffer
                GLCall(::glDisable(GL_BLEND));
                GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 0.0f));

                // bind & clear frame buffers
                GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, target_fb.fbo_id()));
                GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

                // use screen shader
                _screen_quad_shader.use();

                // draw final screen quad
                GLCall(::glActiveTexture(GL_TEXTURE0));
                GLCall(::glBindTexture(GL_TEXTURE_2D, opaque_color_attach->buffer_id));
                GLCall(::glBindVertexArray(_vao_screen_quad));
                GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
            }
        }
        else /// wireframe mode
        {
            render_ctx.curr_render_pass = renderer::render_pass_type::wireframe_rendering;

            // set polygon mode (global state)
            GLCall(::glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));

            // configure render states (global state)
            GLCall(::glEnable(GL_DEPTH_TEST));
            GLCall(::glDepthFunc(GL_LESS));
            GLCall(::glDepthMask(GL_TRUE)); // enable depth buffer writes so glClear won't ignore clearing the depth buffer
            GLCall(::glDisable(GL_BLEND));
            GLCall(::glClearColor(
                scn.render_config.bg_color.r(), 
                scn.render_config.bg_color.g(), 
                scn.render_config.bg_color.b(), 
                scn.render_config.bg_color.a())
            );

            GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, target_fb.fbo_id()));
            GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT0));

            // clear frame buffers
            GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)); 

            _lineset_renderer.render(render_ctx, scn.lineset_objects);
            _pcd_renderer.render(render_ctx, scn.pcd_objects);
            _light_source_renderer.render(render_ctx, _light_source_objects);
            _mesh_renderer.render(render_ctx, scn.mesh_objects);
            _skeleton_renderer.render(render_ctx, scn.skeleton_objects);
        }
    }

} // namespace