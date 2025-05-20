#include "scene_renderer.hh"

#include <triengine/utility/string_format.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

#include <triengine/extern/smaa_area_texture.h>
#include <triengine/extern/smaa_search_texture.h>

#include <triengine/shaders/includes/phong_lighting_shaders.h>
#include <triengine/shaders/includes/smaa_shaders.h>
#include <triengine/shaders/smaa_pass_shaders.h>
#include <triengine/shaders/overlay_pass_shaders.h>

namespace triengine::core
{
    namespace {
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
        _glctx = glctx;

        // Context Settings
        GLCall(::glEnable(GL_DEPTH_TEST));
        //GLCall(::glEnable(GL_MULTISAMPLE));
        GLCall(::glDisable(GL_BLEND));
        GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
        GLCall(::glClearDepth(1.0f));

        _shader_prep = std::make_unique<shader_preprocessor>();
        _shader_prep->register_system_include_from_memory("phong_lighting", shaders::includes::kPhongLightingShader);
        _shader_prep->register_system_include_from_memory("SMAA.hlsl", shaders::includes::kSMAAShaders);

        _inf_plane_renderer.create(*glctx, *_shader_prep);
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
            .attach_vertex_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kOverlayCompositePassVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kOverlayCompositePassFragmentShader).c_str() })
            .link();

        _smaa_edge_detect_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kSMAAEdgeDetectionPassVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kSMAAEdgeDetectionPassFragmentShader).c_str() })
            .link();

        _smaa_blend_weight_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kSMAABlendingWeightPassVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kSMAABlendingWeightPassFragmentShader).c_str() })
            .link();

        _smaa_neighbor_blend_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kSMAANeighborBlendingPassVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, _shader_prep->process_from_memory(shaders::kSMAANeighborBlendingPassFragmentShader).c_str() })
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

        // Load SMAA textures
        {
            texture_params_t texparams;
            texparams.wrap_s = GL_CLAMP_TO_EDGE;
            texparams.wrap_t = GL_CLAMP_TO_EDGE;
            texparams.min_filter = GL_LINEAR;
            texparams.mag_filter = GL_LINEAR;

            std::vector<uint8_t> areaTexBuffer(AREATEX_SIZE);
            for (uint32_t y = 0; y < AREATEX_HEIGHT; y++) {
                //uint32_t srcY = AREATEX_HEIGHT - 1 - y; // flip image
                uint32_t srcY = y;
                std::memcpy(
                    &areaTexBuffer[y * AREATEX_PITCH], 
                    areaTexBytes + srcY * AREATEX_PITCH, 
                    AREATEX_PITCH
                );
            }

            _smaa_area_tex.create_from_memory(
                areaTexBuffer.data(),
                image_format_type::rg,
                AREATEX_WIDTH,
                AREATEX_HEIGHT,
                texparams,
                false
            );

            std::vector<uint8_t> searchTexBuffer(SEARCHTEX_SIZE);
            for (uint32_t y = 0; y < SEARCHTEX_HEIGHT; y++) {
                //uint32_t srcY = SEARCHTEX_HEIGHT - 1 - y; // flip image
                uint32_t srcY = y;
                std::memcpy(
                    &searchTexBuffer[y * SEARCHTEX_PITCH], 
                    searchTexBytes + srcY * SEARCHTEX_PITCH, 
                    SEARCHTEX_PITCH
                );
            }

            _smaa_search_tex.create_from_memory(
                searchTexBuffer.data(),
                image_format_type::greyscale,
                SEARCHTEX_WIDTH,
                SEARCHTEX_HEIGHT,
                texparams,
                false
            );
        }
    }

    void scene_renderer::destroy()
    {
        _inf_plane_renderer.destroy();
        _light_source_renderer.destroy();
        _mesh_renderer.destroy();
        _lineset_renderer.destroy();
        _pcd_renderer.destroy();
        _skeleton_renderer.destroy();

        _wboit_composite_shader.destroy();
        _screen_quad_shader.destroy();
        _overlay_composite_shader.destroy();
        _smaa_edge_detect_shader.destroy();
        _smaa_blend_weight_shader.destroy();
        _smaa_neighbor_blend_shader.destroy();

        if (_vao_screen_quad) { ::glDeleteVertexArrays(1, &_vao_screen_quad); }
        if (_vbo_screen_quad) { ::glDeleteBuffers(1, &_vbo_screen_quad); }
        _glctx = nullptr;
    }

    void scene_renderer::render(
        const frame_buffer& target_fb,
        scene& scn)
    {
        _glctx->get_gpu_resource_manager()->process_pending_requests();
        
        scene_render_config& scn_render_config = *scn.get_render_config();
        const camera& scn_camera = *scn.get_camera();
        const view_port viewport = scn_camera.get_view_port();

        {
            if (scn_render_config.light_opts.dir_light.follow_camera) {
                scn_render_config.light_opts.dir_light.direction = scn_camera.get_direction();
            }

            if (scn_render_config.pcd_point_size) { _pcd_renderer.set_pcd_point_size(scn_render_config.pcd_point_size.value()); }
            _mesh_renderer.enable_object_normal_rendering(scn_render_config.show_object_normals);
            _skeleton_renderer.show_joint_axis(scn_render_config.skeleton_mode == scene_render_config::skeleton_render_mode::overlay_with_joint_axis);
            _inf_plane_renderer.set_options(scn_render_config.inf_plane_opts);
        }

        renderer::render_context render_ctx; {
            scn_camera.get_view_projection(render_ctx.view, render_ctx.projection);
            render_ctx.light_opts = &scn_render_config.light_opts;
            render_ctx.camera = &scn_camera;
        }

        // set viewport (global state)
        GLCall(::glViewport(viewport.x, viewport.y, viewport.width, viewport.height));

        if (!scn_render_config.show_wireframe)
        {
            // set polygon mode (global state)
            GLCall(::glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));

            // NOTE: currently, overlay render pass is only for the skeleton_renderer.
            const bool overlay_render_pass_required =
                !scn.get_skeleton_geometries().empty() &&
                (scn_render_config.skeleton_mode == scene_render_config::skeleton_render_mode::skeleton_overlay ||
                 scn_render_config.skeleton_mode == scene_render_config::skeleton_render_mode::overlay_with_joint_axis);

            // ---------------------------------------------------------------------------------
            // WBOIT pass
            // ---------------------------------------------------------------------------------

            // resize WBOIT framebuffer
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
                * wboit_opaque_color_attach = _wboit_fb.color_attachment(0),
                * wboit_accum_color_attach = _wboit_fb.color_attachment(1),
                * wboit_reveal_color_attach = _wboit_fb.color_attachment(2);

            // bind WBOIT framebuffer
            GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _wboit_fb.fbo_id()));

            // 
            // WBOIT solid pass
            // 
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
                    scn_render_config.bg_color.r(), 
                    scn_render_config.bg_color.g(), 
                    scn_render_config.bg_color.b(),
                    scn_render_config.bg_color.a())
                );

                // clear frame buffers
                GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

                // render solid(opaque) objects
                _lineset_renderer.render(render_ctx, scn.get_lineset_geometries());
                _pcd_renderer.render(render_ctx, scn.get_pcd_geometries());
                _light_source_renderer.render(render_ctx);
                _mesh_renderer.render(render_ctx, scn.get_mesh_geometries());
                if (!overlay_render_pass_required) {
                    _skeleton_renderer.render(render_ctx, scn.get_skeleton_geometries());
                }
            }

            // 
            // WBOIT transparent pass
            // 
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
                _pcd_renderer.render(render_ctx, scn.get_pcd_geometries());

                _mesh_renderer.enable_object_normal_rendering(false); // override setting
                _mesh_renderer.render(
                    render_ctx,
                    scn.get_mesh_geometries()
                );

                if (scn_render_config.show_origin_xz_grid) {
                    // NOTE: The infinite plane renderer must be rendered last to allow for alpha-blending.
                    //       (except the skeleton renderer, which sometimes causes the depth buffer to be reset).
                    _inf_plane_renderer.render(render_ctx);
                }
            }

            // 
            // WBOIT composite pass (render composite image)
            // 
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
                GLCall(::glBindTexture(GL_TEXTURE_2D, wboit_accum_color_attach->buffer_id));
                GLCall(::glActiveTexture(GL_TEXTURE1));
                GLCall(::glBindTexture(GL_TEXTURE_2D, wboit_reveal_color_attach->buffer_id));
                GLCall(::glBindVertexArray(_vao_screen_quad));
                GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
            }

            // ---------------------------------------------------------------------------------
            // overlay rendering pass (currently, this render pass is only for the skeleton_renderer.)
            // ---------------------------------------------------------------------------------

            if (overlay_render_pass_required)
            {
                render_ctx.curr_render_pass = renderer::render_pass_type::wboit_solid_rendering;

                // resize overlay framebuffer
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

                // configure render states (global state)
                GLCall(::glEnable(GL_DEPTH_TEST));
                GLCall(::glDepthFunc(GL_LESS));
                GLCall(::glDepthMask(GL_TRUE)); // enable depth buffer writes so glClear won't ignore clearing the depth buffer
                GLCall(::glEnable(GL_BLEND));
                GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 0.0f));

                // bind overlay framebuffer
                GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _overlay_fb.fbo_id()));
                GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT0/* overlay color buffer */));

                GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

                // render skeletons to overlay color buffer
                _skeleton_renderer.render(render_ctx, scn.get_skeleton_geometries());

                // bind WBOIT framebuffer back
                GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _wboit_fb.fbo_id()));

                _overlay_composite_shader.use();

                GLCall(::glActiveTexture(GL_TEXTURE0));
                GLCall(::glBindTexture(GL_TEXTURE_2D, _overlay_fb.color_attachment()->buffer_id));

                GLCall(::glBindVertexArray(_vao_screen_quad));
                GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
            }

            // ---------------------------------------------------------------------------------
            // SMAA pass
            // ---------------------------------------------------------------------------------

            // resize SMAA framebuffer
            if (!_smaa_fb.is_valid())
            {
                _smaa_fb = frame_buffer::create_color_stencil_only_buffer(
                    {
                        GL_RG16F/* edge detection buffer */,
                        GL_RGBA16F/* blending weight calculation buffer */,
                        GL_RGBA16F/* neighborhood blending buffer */
                    },
                    GL_STENCIL_INDEX8,
                    target_fb.width_pixels(),
                    target_fb.height_pixels()
                );
            }
            else
            {
                // It is okay to call reallocate every frame, 
                // as there is an internal reallocation-skip optimization implemented.
                _smaa_fb.reallocate(
                    target_fb.width_pixels(),
                    target_fb.height_pixels()
                );
            }

            const auto
                * smaa_edge_color_attach = _smaa_fb.color_attachment(0),
                * smaa_blend_color_attach = _smaa_fb.color_attachment(1),
                * smaa_neighbor_color_attach = _smaa_fb.color_attachment(2);

            if (scn_render_config.enable_anti_aliasing)
            {
                const vec4_f32 smaa_rt_metrics{
                    1.0f / static_cast<float>(_smaa_fb.width_pixels()),
                    1.0f / static_cast<float>(_smaa_fb.height_pixels()),
                    static_cast<float>(_smaa_fb.width_pixels()),
                    static_cast<float>(_smaa_fb.height_pixels())
                };

                // bind SMAA framebuffer
                GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _smaa_fb.fbo_id()));

                // configure render states (global state)
                GLCall(::glDisable(GL_BLEND));
                GLCall(::glDisable(GL_DEPTH_TEST));
                GLCall(::glEnable(GL_STENCIL_TEST));
                GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 0.0f)); // color 버퍼 클리어 색상 결정
                GLCall(::glClearStencil(0)); // stencil 버퍼 클리어 색상 설정

                //
                // SMAA edge detection pass
                //
                {
                    GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT0/* edge color buffer */));
                    GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

                    // 스텐실 테스트 항상 통과(통과 시 stencil = 1)하도록 설정
                    GLCall(::glStencilFunc(GL_ALWAYS, 1/* ref */, 0xFF/* mask */));

                    // 프래그먼트 셰이더에서 에지가 그려지는 경우에만 stencil 버퍼의 해당 픽셀에 1 출력.
                    // (에지가 감지되지 않으면 fragment shader에서 discard 수행 -> 이 경우 stencil 버퍼의 해당 픽셀은 이전값(0) 유지.)
                    GLCall(::glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));

                    auto& draw_shader = _smaa_edge_detect_shader;
                    draw_shader.use();
                    draw_shader.set_uniform_vec4("u_smaaRTMetrics", smaa_rt_metrics);

                    GLCall(::glActiveTexture(GL_TEXTURE0));
                    GLCall(::glBindTexture(GL_TEXTURE_2D, wboit_opaque_color_attach->buffer_id)); // main color buffer

                    // draw screen quad
                    GLCall(::glBindVertexArray(_vao_screen_quad));
                    GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));

                    // NOTE: 이 시점부터 stencil 버퍼는 edge인 영역은 1, edge가 아닌 영역은 0으로 설정된 상태이다.
                    GLCall(::glStencilFunc(GL_EQUAL, 1/* ref */, 0xFF/* mask */)); // 이제부터 스텐실 == 1인 픽셀만 처리. (해당하지 않으면 버리고 fragment shader 실행 X)
                    GLCall(::glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP)); // 이제부터 Stencil buffer의 값을 변경하지 않고 유지한다.
                }

                //
                // SMAA blending weight pass
                //
                {
                    GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT1/* blend color buffer */));
                    GLCall(::glClear(GL_COLOR_BUFFER_BIT));

                    auto& draw_shader = _smaa_blend_weight_shader;
                    draw_shader.use();
                    draw_shader.set_uniform_vec4("u_smaaRTMetrics", smaa_rt_metrics);
                    draw_shader.set_uniform_vec4("u_subsampleIndices", vec4_f32(0.0f, 0.0f, 0.0f, 0.0f));

                    GLCall(::glActiveTexture(GL_TEXTURE0));
                    GLCall(::glBindTexture(GL_TEXTURE_2D, smaa_edge_color_attach->buffer_id));
                    GLCall(::glActiveTexture(GL_TEXTURE1));
                    GLCall(::glBindTexture(GL_TEXTURE_2D, _smaa_area_tex.id()));
                    GLCall(::glActiveTexture(GL_TEXTURE2));
                    GLCall(::glBindTexture(GL_TEXTURE_2D, _smaa_search_tex.id()));

                    // draw screen quad
                    GLCall(::glBindVertexArray(_vao_screen_quad));
                    GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
                }

                //
                // SMAA neighbor blending pass
                //
                {
                    GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT2/* neighbor color buffer */));

                    // NOTE: In this pass, there is no need to clear color buffer since the entire color buffer is refreshed every time.
                    //       Therefore, we skip `glClear(GL_COLOR_BUFFER_BIT)` to improve performance.
                    //GLCall(::glClear(GL_COLOR_BUFFER_BIT));

                    GLCall(::glDisable(GL_STENCIL_TEST));

                    auto& draw_shader = _smaa_neighbor_blend_shader;
                    draw_shader.use();
                    draw_shader.set_uniform_vec4("u_smaaRTMetrics", smaa_rt_metrics);

                    GLCall(::glActiveTexture(GL_TEXTURE0));
                    GLCall(::glBindTexture(GL_TEXTURE_2D, wboit_opaque_color_attach->buffer_id)); // main color buffer
                    GLCall(::glActiveTexture(GL_TEXTURE1));
                    GLCall(::glBindTexture(GL_TEXTURE_2D, smaa_blend_color_attach->buffer_id));

                    // draw screen quad
                    GLCall(::glBindVertexArray(_vao_screen_quad));
                    GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
                }
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

                // bind frame buffers
                GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, target_fb.fbo_id()));
                GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT0/* main color buffer */));

                // NOTE: In this pass, there is no need to clear color buffer since the entire color buffer is refreshed every time.
                //       Therefore, we skip `glClear(GL_COLOR_BUFFER_BIT)` to improve performance.
                GLCall(::glClear(/*GL_COLOR_BUFFER_BIT | */GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

                // use screen shader
                _screen_quad_shader.use();

                GLCall(::glActiveTexture(GL_TEXTURE0));
                GLCall(::glBindTexture(GL_TEXTURE_2D, 
                    (scn_render_config.enable_anti_aliasing)
                    ? smaa_neighbor_color_attach->buffer_id
                    : wboit_opaque_color_attach->buffer_id
                ));

                // draw screen quad
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
                scn_render_config.bg_color.r(), 
                scn_render_config.bg_color.g(), 
                scn_render_config.bg_color.b(), 
                scn_render_config.bg_color.a())
            );

            GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, target_fb.fbo_id()));
            GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT0));

            // clear frame buffers
            GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)); 

            _lineset_renderer.render(render_ctx, scn.get_lineset_geometries());
            _pcd_renderer.render(render_ctx, scn.get_pcd_geometries());
            _light_source_renderer.render(render_ctx);
            _mesh_renderer.render(render_ctx, scn.get_mesh_geometries());
            _skeleton_renderer.render(render_ctx, scn.get_skeleton_geometries());
        }
    }

} // namespace