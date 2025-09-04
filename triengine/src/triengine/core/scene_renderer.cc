#include "scene_renderer.hh"

#include <triengine/utility/string_format.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

#include <triengine/extern/smaa_area_texture.h>
#include <triengine/extern/smaa_search_texture.h>

namespace triengine::core
{
    namespace {
        struct quad_vertex_t {
            vec3_f32 position; // vertex position
            vec2_f32 uv; // texture coordinate (uv coordinate)
        };
        static_assert(std::is_standard_layout_v<quad_vertex_t>, "!!");
        static_assert(sizeof(quad_vertex_t) == 5 * sizeof(float), "!!");

        const std::array<quad_vertex_t, 6> quadVertices = {
            quad_vertex_t{ vec3_f32{ -1.0f, -1.0f, 0.0f }, vec2_f32{ 0.0f, 0.0f } },
            quad_vertex_t{ vec3_f32{  1.0f, -1.0f, 0.0f }, vec2_f32{ 1.0f, 0.0f } },
            quad_vertex_t{ vec3_f32{  1.0f,  1.0f, 0.0f }, vec2_f32{ 1.0f, 1.0f } },
            quad_vertex_t{ vec3_f32{  1.0f,  1.0f, 0.0f }, vec2_f32{ 1.0f, 1.0f } },
            quad_vertex_t{ vec3_f32{ -1.0f,  1.0f, 0.0f }, vec2_f32{ 0.0f, 1.0f } },
            quad_vertex_t{ vec3_f32{ -1.0f, -1.0f, 0.0f }, vec2_f32{ 0.0f, 0.0f } }
        };

        inline void create_screen_quad_vao(
            GLuint& vao_id/* out */,
            GLuint& vbo_id/* out */)
        {
            GLCall(::glCreateVertexArrays(1, &vao_id));
            GLCall(::glCreateBuffers(1, &vbo_id));

            // Link VAO's binding index 0 to the VBO
            GLCall(::glVertexArrayVertexBuffer(
                vao_id/*vao id*/,
                0/*bindingindex*/,
                vbo_id/*vbo id*/,
                0/*startoffset*/,
                sizeof(quad_vertex_t)/*stride*/
            ));

            // Allocate (immutable) GPU storage & upload data to VBO
            GLCall(::glNamedBufferStorage(
                vbo_id,
                sizeof(quad_vertex_t) * quadVertices.size(),
                quadVertices.data(),
                GL_MAP_READ_BIT
            ));

            // Setup vertex attribute index 0
            GLCall(::glEnableVertexArrayAttrib(vao_id, 0/*attribindex*/)); // attrib 0 = position
            GLCall(::glVertexArrayAttribBinding(vao_id, 0/*attribindex*/, 0/*bindingindex*/)); // attrib 0 <- binding 0
            GLCall(::glVertexArrayAttribFormat(
                vao_id,
                0/*attribindex*/,
                decltype(quad_vertex_t::position)::SizeAtCompileTime/*size*/,
                GL_FLOAT/*type*/,
                GL_FALSE/*normalize*/,
                offsetof(quad_vertex_t, position)/*relativeoffset*/
            ));

            // Setup vertex attribute index 1
            GLCall(::glEnableVertexArrayAttrib(vao_id, 1/*attribindex*/)); // attrib 1 = uv
            GLCall(::glVertexArrayAttribBinding(vao_id, 1/*attribindex*/, 0/*bindingindex*/)); // attrib 1 <- binding 0
            GLCall(::glVertexArrayAttribFormat(
                vao_id,
                1/*attribindex*/,
                decltype(quad_vertex_t::uv)::SizeAtCompileTime/*size*/,
                GL_FLOAT/*type*/,
                GL_FALSE/*normalize*/,
                offsetof(quad_vertex_t, uv)/*relativeoffset*/
            ));
        }

    } // namespace

    void scene_renderer::create(gl_context* glctx)
    {
        _glctx = glctx;

        //
        // Context Settings (global state)
        //

        GLCall(::glEnable(GL_DEPTH_TEST));
        //GLCall(::glEnable(GL_MULTISAMPLE));
        GLCall(::glDisable(GL_BLEND));
        GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
        GLCall(::glClearDepth(1.0f));
        GLCall(::glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)); // or GL_LINE

        auto shader_ldr = glctx->get_shader_loader();

        _inf_plane_renderer.create(*glctx);
        _light_source_renderer.create(*glctx);
        _mesh_renderer.create(*glctx);
        _lineset_renderer.create(*glctx);
        _pcd_renderer.create(*glctx);
        _skeleton_renderer.create(*glctx);

        _bloom_effect.create(*glctx);

        _deferred_lighting_shader
            .attach_vertex_shader({ shader_ldr->load("deferred_lighting_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("deferred_lighting_pass.frag")->c_str() })
            .link();

        _wboit_composite_shader
            .attach_vertex_shader({ shader_ldr->load("wboit_composite_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("wboit_composite_pass.frag")->c_str() })
            .link();

        _screen_quad_shader
            .attach_vertex_shader({ shader_ldr->load("screen_quad.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("screen_quad.frag")->c_str() })
            .link();

        _hdr_screen_quad_shader
            .attach_vertex_shader({ shader_ldr->load("screen_quad.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("screen_quad_hdr.frag")->c_str() })
            .link();

        _clear_color_screen_quad_shader
            .attach_vertex_shader({ shader_ldr->load("screen_quad.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("screen_quad_clear_color.frag")->c_str() })
            .link();

        _overlay_composite_shader
            .attach_vertex_shader({ shader_ldr->load("overlay_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("overlay_pass.frag")->c_str() })
            .link();

        _smaa_edge_detect_shader
            .attach_vertex_shader({ shader_ldr->load("smaa_edge_detect_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("smaa_edge_detect_pass.frag")->c_str() })
            .link();

        _smaa_blend_weight_shader
            .attach_vertex_shader({ shader_ldr->load("smaa_blend_weight_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("smaa_blend_weight_pass.frag")->c_str() })
            .link();

        _smaa_neighbor_blend_shader
            .attach_vertex_shader({ shader_ldr->load("smaa_neighbor_blend_pass.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("smaa_neighbor_blend_pass.frag")->c_str() })
            .link();

        // Create screen-quad VAO
        create_screen_quad_vao(_vao_screen_quad, _vbo_screen_quad);

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
                false,
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
                false,
                SEARCHTEX_WIDTH,
                SEARCHTEX_HEIGHT,
                texparams,
                false
            );
        }
    }

    void scene_renderer::destroy()
    {
        _glctx = nullptr;

        _inf_plane_renderer.destroy();
        _light_source_renderer.destroy();
        _mesh_renderer.destroy();
        _lineset_renderer.destroy();
        _pcd_renderer.destroy();
        _skeleton_renderer.destroy();

        _bloom_effect.destroy();

        _wboit_fb.destroy();
        _gbuffer_fb.destroy();
        _overlay_fb.destroy();
        _smaa_fb.destroy();

        _deferred_lighting_shader.destroy();
        _wboit_composite_shader.destroy();
        _overlay_composite_shader.destroy();
        _screen_quad_shader.destroy();
        _hdr_screen_quad_shader.destroy();
        _clear_color_screen_quad_shader.destroy();

        _smaa_edge_detect_shader.destroy();
        _smaa_blend_weight_shader.destroy();
        _smaa_neighbor_blend_shader.destroy();

        _smaa_area_tex.destroy();
        _smaa_search_tex.destroy();

        if (_vao_screen_quad) { ::glDeleteVertexArrays(1, &_vao_screen_quad); }
        if (_vbo_screen_quad) { ::glDeleteBuffers(1, &_vbo_screen_quad); }
    }

    void scene_renderer::render(
        const GLuint target_fbo_id,
        const int32_t frame_width_pixels,
        const int32_t frame_height_pixels,
        scene& scn)
    {
        _glctx->get_gpu_resource_manager()->process_pending_requests();

        const abstract_camera& scn_camera = *scn.get_camera();
        scene_render_config& scn_render_config = *scn.get_render_config();
        if (scn_render_config.light_opts.dir_light.follow_camera) {
            scn_render_config.light_opts.dir_light.direction = scn_camera.get_direction();
        }

        renderer::render_context render_ctx; {
            scn_camera.get_view_projection(render_ctx.view, render_ctx.projection);
            render_ctx.light_opts = &scn_render_config.light_opts;
            render_ctx.camera = &scn_camera;
        }

        // set viewport (global state)
        const view_port& viewport = scn_camera.get_viewport();
        GLCall(::glViewport(viewport.x, viewport.y, viewport.width, viewport.height));

        if (scn_render_config.pcd_point_size) {
            _pcd_renderer.set_pcd_point_size(scn_render_config.pcd_point_size.value());
        }

        _mesh_renderer.set_render_mode(renderer::mesh_renderer::render_mode_type::shaded_surfaces);
        _skeleton_renderer.show_joint_axis(scn_render_config.skeleton_mode == skeleton_render_mode::overlay_with_joint_axis);
        _inf_plane_renderer.set_options(scn_render_config.inf_plane_opts);

        // NOTE: currently, overlay render pass is only for the skeleton_renderer.
        const bool overlay_render_pass_required =
            !scn.get_skeleton_geometries().empty() &&
            (scn_render_config.skeleton_mode == skeleton_render_mode::skeleton_overlay ||
                scn_render_config.skeleton_mode == skeleton_render_mode::overlay_with_joint_axis);

        // resize WBOIT framebuffer
        if (!_wboit_fb.is_valid())
        {
            _wboit_fb = frame_buffer::create_color_depth_stencil_buffer(
                {
                    GL_RGBA16F/* GL_COLOR_ATTACHMENT0: WBOIT opaque color buffer, used in WBOIT opaque pass */,
                    GL_RGBA16F/* GL_COLOR_ATTACHMENT1: WBOIT accum color buffer, used in WBOIT transparent pass */,
                    GL_R8     /* GL_COLOR_ATTACHMENT2: WBOIT reveal color buffer, used in WBOIT transparent pass */
                },
                GL_DEPTH24_STENCIL8,
                frame_width_pixels,
                frame_height_pixels
            );
        }
        else
        {
            // It is okay to call reallocate every frame, 
            // as there is an internal reallocation-skip optimization implemented.
            _wboit_fb.reallocate(
                frame_width_pixels,
                frame_height_pixels
            );
        }

        // resize GBuffer
        if (!_gbuffer_fb.is_valid())
        {
            _gbuffer_fb = frame_buffer::create_color_depth_stencil_buffer(
                { // G-Buffer color attachments
                    GL_RGBA16F, // GL_COLOR_ATTACHMENT0: position buffer; fragPos(RGB)
                    GL_RGBA16F, // GL_COLOR_ATTACHMENT1: normal buffer; fragNormal(RGB)
                    GL_RGBA16F, // GL_COLOR_ATTACHMENT2: color buffer; albedoColor(RGB), specularColor(A)
                    GL_RGBA16F  // GL_COLOR_ATTACHMENT3: material buffer; ambientIntensity(R), diffuseIntensity(G), specularIntensity(B), shininess(A)
                },
                GL_DEPTH24_STENCIL8,
                frame_width_pixels,
                frame_height_pixels
            );
        }
        else
        {
            // It is okay to call reallocate every frame, 
            // as there is an internal reallocation-skip optimization implemented.
            _gbuffer_fb.reallocate(
                frame_width_pixels,
                frame_height_pixels
            );
        }

        // ---------------------------------------------------------------------------------
        // 1) Deferred geometry pass (WBOIT opaque pass)
        // render scene's geometry to G-Buffer
        // ---------------------------------------------------------------------------------
        {
            render_ctx.curr_render_pass = renderer::render_pass_type::deferred_opaque_pass;

            // bind gbuffer framebuffer
            GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _gbuffer_fb.fbo_id()));

            // Explicitly specifies that which color attachments we'll use (of this framebuffer) for rendering 
            constexpr std::array<GLenum, 4> draw_buffers{
                GL_COLOR_ATTACHMENT0, // position buffer; fragPos(RGB)
                GL_COLOR_ATTACHMENT1, // normal buffer; fragNormal(RGB)
                GL_COLOR_ATTACHMENT2, // color buffer; albedoColor(RGB), specularColor(A)
                GL_COLOR_ATTACHMENT3, // material buffer; ambientIntensity(R), diffuseIntensity(G), specularIntensity(B), shininess(A)
            };
            GLCall(::glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data()));

            // configure render states
            GLCall(::glDisable(GL_BLEND));
            GLCall(::glEnable(GL_STENCIL_TEST)); // Enable stencil test for background rendering

            GLCall(::glEnable(GL_DEPTH_TEST));
            GLCall(::glDepthFunc(GL_LESS));
            GLCall(::glDepthMask(GL_TRUE)); // enable depth writes so glClear won't ignore clearing the depth buffer

            GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 1.0f)); // clear color MUST be set to opaque black
            GLCall(::glClearStencil(0)); // stencil buffer clear color MUST be set to 0
            GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

            // Configure stencil test to pass always
            // (when stencil test passed, set stencil = 1)
            GLCall(::glStencilFunc(GL_ALWAYS, 1/* ref */, 0xFF/* mask */));

            // In the fragment shader, write 1 to the stencil buffer only for pixels where a vertex is "actually drawn".
            // (In other words, keep the stencil buffer value as previous value(0) for pixels, belonging to the background.)
            GLCall(::glStencilOp(
                GL_KEEP,   // Operation to take when stencil test fails
                GL_KEEP,   // Operation to take when stencil test passes, but depth test fails
                GL_REPLACE // Operation to take when both stencil test & depth test passes
            ));

            // Begin drawing the objects to be rendered in the deferred opaque pass
            // (fill G-buffer with opaque objects)
            {
                _pcd_renderer.render(render_ctx, scn.get_pcd_geometries());
                _mesh_renderer.render(render_ctx, scn.get_mesh_geometries());
                if (!overlay_render_pass_required) {
                    _skeleton_renderer.render(render_ctx, scn.get_skeleton_geometries());
                }
            }

            // NOTE: At this point, the stencil buffer is set to 1 for "object" regions, and remains 0 for "background" regions.
            //       From now on, keep the stencil buffer values unchanged.
            GLCall(::glStencilOp(
                GL_KEEP, // Operation to take when stencil test fails
                GL_KEEP, // Operation to take when stencil test passes, but depth test fails
                GL_KEEP  // Operation to take when both stencil test & depth test passes
            ));
        }

        // To provide the subsequent forward passes with the correct depth & stencil information from the geometry pass, 
        // transfer the depth & stencil attachment from the G-Buffer FBO to the main(WBOIT) FBO.
        //
        // Optimization:
        //   Instead of performing a costly blit with `glBlitFramebuffer()`, 
        //   just simply swap the attachments between the two FBOs. 
        //   (This works as a highly efficient "move" operation.)
        //   
        // Reason:
        //   - The G-Buffer's depth & stencil buffer is no longer needed for the remainder of this frame.
        //   - The old attachment that the G-Buffer receives in return is irrelevant, as it will be
        //     completely cleared by `glClear()` at the start of the next frame's geometry pass.
        //
        // This creates a "ping-pong" buffering pattern, where the two framebuffers exchange
        // their depth & stencil attachments every frame, avoiding redundant GPU memory operations.
        _gbuffer_fb.swap_depth_stencil_attachment(_wboit_fb);

        // ---------------------------------------------------------------------------------
        // 2) Deferred lighting pass
        // calculate lighting by iterating over a screen filled quad 
        // pixel-by-pixel using the gbuffer's content.
        // ---------------------------------------------------------------------------------
        {
            // bind main framebuffer
            GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, _wboit_fb.fbo_id()));

            // Explicitly specifies that which color attachments we'll use (of this framebuffer) for rendering 
            GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT0));

            GLCall(::glClearColor(
                scn_render_config.bg_color.r(),
                scn_render_config.bg_color.g(),
                scn_render_config.bg_color.b(),
                scn_render_config.bg_color.a()
            ));

            // Only clear color buffer, keep depth & stencil buffer for forward pass
            GLCall(::glClear(GL_COLOR_BUFFER_BIT));

            // set render states
            GLCall(::glDisable(GL_DEPTH_TEST));
            GLCall(::glDepthMask(GL_FALSE)); // disable depth writes during lighting pass
            GLCall(::glDisable(GL_BLEND));

            // --- Stencil Optimization ---
            // From now on, we will only run the fragment shader for pixels where the stencil == 1 (rendered object pixels).
            // (i.e., we will not render the background part of the stencil buffer.)
            GLCall(::glStencilFunc(GL_EQUAL, 1/* ref */, 0xFF/* mask */));

            auto& draw_shader = _deferred_lighting_shader;
            draw_shader.use();

            // Update light options in shader
            render_ctx.light_opts->dir_light.apply_to_shader(draw_shader, render_ctx.view);
            render_ctx.light_opts->point_light.apply_to_shader(draw_shader, render_ctx.view);

            GLCall(::glBindTextureUnit(0, _gbuffer_fb.color_attachment(0)->buffer_id));
            GLCall(::glBindTextureUnit(1, _gbuffer_fb.color_attachment(1)->buffer_id));
            GLCall(::glBindTextureUnit(2, _gbuffer_fb.color_attachment(2)->buffer_id));
            GLCall(::glBindTextureUnit(3, _gbuffer_fb.color_attachment(3)->buffer_id));

            // draw screen quad
            GLCall(::glBindVertexArray(_vao_screen_quad));
            GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
        }

        const auto
            * wboit_opaque_color_attach = _wboit_fb.color_attachment(0),
            * wboit_accum_color_attach = _wboit_fb.color_attachment(1),
            * wboit_reveal_color_attach = _wboit_fb.color_attachment(2);

        // ---------------------------------------------------------------------------------
        // 3) Forward pass
        // render objects that need forward rendering (like transparent objects, light sources)
        // ---------------------------------------------------------------------------------
        {
            //
            // Clear background color
            // NOTE: `glClear()` function is not affected by the stencil test(`glStencilFunc()` function),
            //       so when the stencil test is enabled, draw a full-screen quad manually to render the background color.
            //
            
            // From now on, run the fragment shader only for pixels where stencil == 0 (background region pixels).
            // (while keeping the rendered object regions from the deferred pass,
            // clear only the background regions to a specific color in the forward pass.)
            GLCall(::glStencilFunc(GL_EQUAL, 0/* ref */, 0xFF/* mask */));

            _clear_color_screen_quad_shader.use();
            _clear_color_screen_quad_shader.set_uniform_vec4("u_clearColor", scn_render_config.bg_color.to_eigen());

            // draw screen quad (clear background region color)
            GLCall(::glBindVertexArray(_vao_screen_quad));
            GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
                
            // Disable stencil test for forward pass
            GLCall(::glDisable(GL_STENCIL_TEST));

            // configure render states for forward pass
            GLCall(::glEnable(GL_DEPTH_TEST));
            GLCall(::glDepthFunc(GL_LESS)); // Use LEQUAL for objects at same depth
            GLCall(::glDepthMask(GL_TRUE)); // enable depth writes

            //
            // Forward opaque pass start
            //

            render_ctx.curr_render_pass = renderer::render_pass_type::forward_opaque_pass;

            _light_source_renderer.render(render_ctx);
            _lineset_renderer.render(render_ctx, scn.get_lineset_geometries());

            // render mesh object normals (if enabled)
            if (scn_render_config.show_object_normals) {
                _mesh_renderer.set_render_mode(renderer::mesh_renderer::render_mode_type::vertex_normals);
                _mesh_renderer.render(render_ctx, scn.get_mesh_geometries());
            }

            //
            // WBOIT transparent pass start
            //

            render_ctx.curr_render_pass = renderer::render_pass_type::forward_transparent_pass;

            // Explicitly specifies that the drawing buffer of the 
            // currently bound framebuffer is an accum & reveal color buffer.
            constexpr std::array<GLenum, 2> curr_draw_buffers = {
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

            _mesh_renderer.set_render_mode(renderer::mesh_renderer::render_mode_type::shaded_surfaces);
            _mesh_renderer.render(render_ctx, scn.get_mesh_geometries());

            if (scn_render_config.show_origin_xz_grid) {
                // NOTE: The infinite plane renderer must be rendered last to allow for alpha-blending.
                //       (except the skeleton renderer, which sometimes causes the depth buffer to be reset).
                _inf_plane_renderer.render(render_ctx);
            }

            // 
            // WBOIT composite pass (render composite image)
            // 

            // Explicitly specifies that the drawing buffer of the 
            // currently bound framebuffer is an opaque color buffer.
            GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT0/* opaque color buffer */));

            // configure render states (global state)
            GLCall(::glDepthFunc(GL_ALWAYS));
            GLCall(::glEnable(GL_BLEND));
            GLCall(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

            // use composite shader
            _wboit_composite_shader.use();

            GLCall(::glBindTextureUnit(0, wboit_accum_color_attach->buffer_id)); // u_accum
            GLCall(::glBindTextureUnit(1, wboit_reveal_color_attach->buffer_id)); // u_reveal

            // draw screen quad to opaque color buffer
            GLCall(::glBindVertexArray(_vao_screen_quad));
            GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
        }

        // ---------------------------------------------------------------------------------
        // 4) overlay rendering pass (currently, this render pass is only for the skeleton_renderer.)
        // ---------------------------------------------------------------------------------
        if (overlay_render_pass_required)
        {
            render_ctx.curr_render_pass = renderer::render_pass_type::forward_opaque_pass;

            // resize overlay framebuffer
            if (!_overlay_fb.is_valid())
            {
                _overlay_fb = frame_buffer::create_color_depth_only_buffer(
                    GL_RGBA16F,
                    GL_DEPTH_COMPONENT24,
                    frame_width_pixels,
                    frame_height_pixels
                );
            }
            else
            {
                // It is okay to call reallocate every frame, 
                // as there is an internal reallocation-skip optimization implemented.
                _overlay_fb.reallocate(
                    frame_width_pixels,
                    frame_height_pixels
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

            GLCall(::glBindTextureUnit(0, _overlay_fb.color_attachment()->buffer_id)); // u_srcFrame

            // draw screen quad
            GLCall(::glBindVertexArray(_vao_screen_quad));
            GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
        }

        // ---------------------------------------------------------------------------------
        // Phys. Based Bloom pass
        // ---------------------------------------------------------------------------------
        if (scn_render_config.light_opts.bloom.enabled)
        {
            // NOTE: At this point, the target scene texture’s FBO(`_wboit_fb`) 
            //       MUST be bound before applying the bloom effect.
            _bloom_effect.resize({ frame_width_pixels, frame_height_pixels });
            _bloom_effect.apply(
                wboit_opaque_color_attach->buffer_id, // main scene texture
                scn_render_config.light_opts.bloom
            ); 
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
                frame_width_pixels,
                frame_height_pixels
            );
        }
        else
        {
            // It is okay to call reallocate every frame, 
            // as there is an internal reallocation-skip optimization implemented.
            _smaa_fb.reallocate(
                frame_width_pixels,
                frame_height_pixels
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
            GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 0.0f)); // color buffer clear color == (0,0,0,0)
            GLCall(::glClearStencil(0)); // stencil buffer clear color == (0)

            //
            // SMAA edge detection pass
            //
            {
                GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT0/* edge color buffer */));
                GLCall(::glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

                // Configure stencil test to pass always
                // (when stencil test passed, set stencil = 1)
                GLCall(::glStencilFunc(GL_ALWAYS, 1/* ref */, 0xFF/* mask */));

                // In the fragment shader, write 1 to the stencil buffer only for pixels where an edge is drawn.
                // If no edge is detected, the fragment shader performs a discard; so the corresponding stencil buffer pixel keeps its previous value (0).
                GLCall(::glStencilOp(
                    GL_KEEP,   // Operation to take when stencil test fails
                    GL_KEEP,   // Operation to take when stencil test passes, but depth test fails
                    GL_REPLACE // Operation to take when both stencil test & depth test passes
                ));

                auto& draw_shader = _smaa_edge_detect_shader;
                draw_shader.use();
                draw_shader.set_uniform_vec4("u_smaaRTMetrics", smaa_rt_metrics);

                GLCall(::glBindTextureUnit(0, wboit_opaque_color_attach->buffer_id)); // main color buffer (u_colorTex)

                // draw screen quad
                GLCall(::glBindVertexArray(_vao_screen_quad));
                GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));

                // NOTE: At this point, the stencil buffer is set to 1 for "edge" regions, and remains 0 for "non-edge" regions.
                // from now on, process only pixels where stencil == 1 (edge region pixels).
                // (otherwise, discard and do not run the fragment shader)
                GLCall(::glStencilFunc(GL_EQUAL, 1/* ref */, 0xFF/* mask */)); 

                // From now on, keep the stencil buffer values unchanged.
                GLCall(::glStencilOp(
                    GL_KEEP, // Operation to take when stencil test fails
                    GL_KEEP, // Operation to take when stencil test passes, but depth test fails
                    GL_KEEP  // Operation to take when both stencil test & depth test passes
                ));
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

                GLCall(::glBindTextureUnit(0, smaa_edge_color_attach->buffer_id)); // u_edgesTex
                GLCall(::glBindTextureUnit(1, _smaa_area_tex.id())); // u_areaTex
                GLCall(::glBindTextureUnit(2, _smaa_search_tex.id())); // u_searchTex

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

                GLCall(::glBindTextureUnit(0, wboit_opaque_color_attach->buffer_id)); // u_colorTex (main color buffer)
                GLCall(::glBindTextureUnit(1, smaa_blend_color_attach->buffer_id)); // u_blendTex

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
            GLCall(::glBindFramebuffer(GL_FRAMEBUFFER, target_fbo_id));
            GLCall(::glDrawBuffer(GL_COLOR_ATTACHMENT0/* main color buffer */));

            // NOTE: In this pass, there is no need to clear color buffer since the entire color buffer is refreshed every time.
            //       Therefore, we skip `glClear(GL_COLOR_BUFFER_BIT)` to improve performance.
            //GLCall(::glClear(GL_COLOR_BUFFER_BIT));

            if (scn_render_config.light_opts.hdr.enabled)
            {
                // use HDR screen-quad shader
                _hdr_screen_quad_shader.use();
                _hdr_screen_quad_shader.set_uniform_float("u_exposure", scn_render_config.light_opts.hdr.exposure);
                _hdr_screen_quad_shader.set_active_subroutine(
                    shader_object_type::fragment,
                    "u_toneMappingCurve",
                    static_cast<GLuint>(scn_render_config.light_opts.hdr.tone_mapping_curve)
                );
            }
            else
            {
                // use screen-quad shader
                _screen_quad_shader.use();
            }

            // u_screenTexture (or u_screenHdrTexture)
            GLCall(::glBindTextureUnit(0,
                (scn_render_config.enable_anti_aliasing)
                ? smaa_neighbor_color_attach->buffer_id
                : wboit_opaque_color_attach->buffer_id
            ));

            // draw screen quad
            GLCall(::glBindVertexArray(_vao_screen_quad));
            GLCall(::glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadVertices.size())));
        }
    }

} // namespace