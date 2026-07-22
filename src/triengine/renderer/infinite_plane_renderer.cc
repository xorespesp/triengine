#include "infinite_plane_renderer.hh"

#include <triengine/utility/debug_utils.hh>

namespace triengine::renderer
{
    infinite_plane_renderer::infinite_plane_renderer()
    { }

    infinite_plane_renderer::~infinite_plane_renderer()
    {
        this->destroy_impl();
    }

    void infinite_plane_renderer::create_impl(core::gl_context& glctx)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        glctx.make_context_current();

        //
        // Context Settings
        //

        const auto shader_ldr = glctx.get_shader_loader();

        _transparent_grid_shader
            .attach_vertex_shader({ shader_ldr->load("inf_plane.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("inf_plane_transparent_grid.frag")->c_str() })
            .link();

        _box_filtered_grid_shader
            .attach_vertex_shader({ shader_ldr->load("inf_plane.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("inf_plane_boxed_grid.frag")->c_str() })
            .link();

        _box_filtered_chess_shader
            .attach_vertex_shader({ shader_ldr->load("inf_plane.vert")->c_str() })
            .attach_fragment_shader({ shader_ldr->load("inf_plane_boxed_chess.frag")->c_str() })
            .link();

        // Create empty VAO (for avoid INVALID_OPERATION draw-call error)
        GLCall(::glCreateVertexArrays(1, &_vao));
    }

    void infinite_plane_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            GLCall(::glDeleteVertexArrays(1, &_vao));

            _transparent_grid_shader.destroy();
            _box_filtered_grid_shader.destroy();
            _box_filtered_chess_shader.destroy();
        }
    }

    void infinite_plane_renderer::render_impl(const render_context& render_ctx)
    {
        if (render_ctx.curr_render_pass != render_pass_type::forward_transparent_pass) {
            return;
        }

        /*
        struct GLStateBackupRAIIContext final {
            GLboolean depth_test_enabled{};
            GLboolean blend_enabled{};
            GLint blend_equation_rgb{}, blend_equation_alpha{};
            GLint src_rgb{}, dst_rgb{}, src_alpha{}, dst_alpha{};

            // Backup GL state
            GLStateBackupRAIIContext() {
                depth_test_enabled = ::glIsEnabled(GL_DEPTH_TEST);
                blend_enabled = ::glIsEnabled(GL_BLEND);
                GLCall(::glGetIntegerv(GL_BLEND_EQUATION_RGB, &blend_equation_rgb));
                GLCall(::glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blend_equation_alpha));
                GLCall(::glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb));
                GLCall(::glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb));
                GLCall(::glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha));
                GLCall(::glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha));
            }

            // Restore GL state
            ~GLStateBackupRAIIContext() {
                if (depth_test_enabled) { GLCall(::glEnable(GL_DEPTH_TEST)); }
                else { GLCall(::glDisable(GL_DEPTH_TEST)); }
                if (blend_enabled) { GLCall(::glEnable(GL_BLEND)); }
                else { GLCall(::glDisable(GL_BLEND)); }
                GLCall(::glBlendEquationSeparate(blend_equation_rgb, blend_equation_alpha));
                GLCall(::glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha));
            }
        };

        GLStateBackupRAIIContext state_backup_{};

        // Enable depth testing
        GLCall(::glEnable(GL_DEPTH_TEST));

        // Enable blending
        GLCall(::glEnable(GL_BLEND));
        GLCall(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        */

        std::visit(
            [this, &render_ctx](const auto& pattern_opt)
            {
                using T = std::decay_t<decltype(pattern_opt)>;

                core::shader_program* draw_shader{ nullptr };
                if constexpr (std::is_same_v<T, transparent_grid_plane_option_t>) {
                    draw_shader = &_transparent_grid_shader;
                    draw_shader->use();
                    draw_shader->set_uniform_vec3("u_gridLineColor", pattern_opt.grid_line_color);
                } else if constexpr (std::is_same_v<T, box_filtered_grid_plane_option_t>) {
                    draw_shader = &_box_filtered_grid_shader;
                    draw_shader->use();
                    draw_shader->set_uniform_vec3("u_gridLineColor", pattern_opt.grid_line_color);
                    draw_shader->set_uniform_vec3("u_gridCellColor", pattern_opt.grid_cell_color);

                    // Update light options in shader
                    render_ctx.light_opts->dir_light.apply_to_shader(*draw_shader, render_ctx.view);
                    render_ctx.light_opts->point_light.apply_to_shader(*draw_shader, render_ctx.view);

                    // Update materials
                    auto* material = &pattern_opt.material;
                    draw_shader->set_uniform_float("u_phongMaterial.ambientIntensity", material->ambient_intensity);
                    draw_shader->set_uniform_float("u_phongMaterial.diffuseIntensity", material->diffuse_intensity);
                    draw_shader->set_uniform_float("u_phongMaterial.specularIntensity", material->specular_intensity);
                    draw_shader->set_uniform_float("u_phongMaterial.shininess", static_cast<float>(material->shininess));
                } else if constexpr (std::is_same_v<T, box_filtered_chess_plane_option_t>) {
                    draw_shader = &_box_filtered_chess_shader;
                    draw_shader->use();
                    draw_shader->set_uniform_vec3("u_gridCellColor1", pattern_opt.grid_cell_color1);
                    draw_shader->set_uniform_vec3("u_gridCellColor2", pattern_opt.grid_cell_color2);

                    // Update light options in shader
                    render_ctx.light_opts->dir_light.apply_to_shader(*draw_shader, render_ctx.view);
                    render_ctx.light_opts->point_light.apply_to_shader(*draw_shader, render_ctx.view);

                    // Update materials
                    auto* material = &pattern_opt.material;
                    draw_shader->set_uniform_float("u_phongMaterial.ambientIntensity", material->ambient_intensity);
                    draw_shader->set_uniform_float("u_phongMaterial.diffuseIntensity", material->diffuse_intensity);
                    draw_shader->set_uniform_float("u_phongMaterial.specularIntensity", material->specular_intensity);
                    draw_shader->set_uniform_float("u_phongMaterial.shininess", static_cast<float>(material->shininess));
                } else {
                    TRIENGINE_ASSERT(false);
                }

                draw_shader->set_uniform_mat4("u_view", render_ctx.view);
                draw_shader->set_uniform_mat4("u_proj", render_ctx.projection);
                draw_shader->set_uniform_vec3("u_eyePosInWorld", render_ctx.camera->get_position());
                draw_shader->set_uniform_float("u_planeHalfSize", _options.max_view_distance);
                draw_shader->set_uniform_float("u_gridCellSize", _options.grid_cell_size);

                // --- simple fog ---
                draw_shader->set_uniform_bool("u_simpleFog.enabled", render_ctx.light_opts->simple_fog.enabled);
                if (render_ctx.light_opts->simple_fog.enabled) {
                    draw_shader->set_uniform_vec3("u_simpleFog.color", render_ctx.simple_fog_color.to_eigen());
                    draw_shader->set_uniform_float("u_simpleFog.density", render_ctx.light_opts->simple_fog.fog_density);
                    draw_shader->set_uniform_float("u_simpleFog.startDist", render_ctx.light_opts->simple_fog.fog_start_dist);
                }
            }, _options.plane_option);

        // Render grid
        // NOTE: Bind empty VAO for avoid INVALID_OPERATION draw-call error
        // Ref: https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf (10.3. VERTEX ARRAYS)
        //      "An INVALID_OPERATION error is generated by any commands which modify, draw from, or query vertex array state when no vertex array is bound."
        GLCall(::glBindVertexArray(_vao));
        GLCall(::glDrawArrays(GL_TRIANGLES, 0, 6));
        //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
    }

} // namespace