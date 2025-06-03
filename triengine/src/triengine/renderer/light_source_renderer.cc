#include "light_source_renderer.hh"

#include <triengine/math/constants.hh>
#include <triengine/shaders/light_source_shaders.h>
#include <triengine/utility/debug_utils.hh>

namespace triengine::renderer
{
    namespace
    {
        void _create_sphere_vertices(
            const float radius,
            std::vector<vec3_f32>& vertex_positions/* [out] List of triangle vertex positions (3 * num_triangles) */, 
            std::vector<vec3_f32>& vertex_normals/* [out] List of triangle vertex normals (3 * num_triangles) */, 
            std::vector<vec3_i32>& triangle_indices/* [out] List of triangles denoted by the index of points forming the triangle. (num_triangles) */)
        {
            TRIENGINE_ASSERT(radius > 0);
    
            constexpr int sector_count = 36; // longitude, # of slices
            constexpr int stack_count = 18; // latitude, # of stacks
            constexpr float sector_step = 2 * math::pi<float>() / sector_count;
            constexpr float stack_step = math::pi<float>() / stack_count;
    
            const float radius_inv = 1.0f / radius; // normal
    
            float sector_angle = 0, stack_angle = 0;
            float x = 0.f, y = 0.f, z = 0.f, xy = 0.f;
            float nx = 0.f, ny = 0.f, nz = 0.f;
    
            for (int i = 0; i <= stack_count; ++i) {
                stack_angle = math::pi<float>() / 2 - i * stack_step; // starting from pi/2 to -pi/2
                xy = radius * std::cos(stack_angle); // r * cos(u)
                z = radius * std::sin(stack_angle); // r * sin(u)
    
                // add (sectorCount+1) vertices per stack
                for (int j = 0; j <= sector_count; ++j) {
                    sector_angle = j * sector_step; // starting from 0 to 2pi
    
                    // vertex position
                    x = xy * std::cos(sector_angle); // r * cos(u) * cos(v)
                    y = xy * std::sin(sector_angle); // r * cos(u) * sin(v)
    
                    // normalized vertex normal
                    nx = x * radius_inv;
                    ny = y * radius_inv;
                    nz = z * radius_inv;
    
                    vertex_positions.emplace_back(x, y, z);
                    vertex_normals.emplace_back(nx, ny, nz);
                } // for
            } // for
    
            // indices
            //  k1--k1+1
            //  |  / |
            //  | /  |
            //  k2--k2+1
            uint32_t k1 = 0, k2 = 0;
            for (int i = 0; i < stack_count; ++i) {
                k1 = i * (sector_count + 1); // beginning of current stack
                k2 = k1 + sector_count + 1;  // beginning of next stack
    
                for (int j = 0; j < sector_count; ++j, ++k1, ++k2) {
                    // 2 triangles per sector excluding 1st and last stacks
                    if (i != 0) {
                        // (k1)---(k2)---(k1+1)
                        triangle_indices.emplace_back(k1, k2, k1 + 1);
                    }
    
                    if (i != (stack_count - 1)) {
                        // (k1+1)---(k2)---(k2+1)
                        triangle_indices.emplace_back(k1 + 1, k2, k2 + 1);
                    }
                } // for
            } // for

            TRIENGINE_ASSERT(vertex_normals.size() == vertex_positions.size() || vertex_normals.empty());
            TRIENGINE_ASSERT(!triangle_indices.empty());
        }

    } // namespace

    light_source_renderer::light_source_renderer()
    {}

    void light_source_renderer::create_impl(
        core::gl_context& glctx, 
        const core::shader_preprocessor& shader_prep)
    {
        TRIENGINE_ASSERT(!this->is_created());
        this->set_creation_flag(true);

        glctx.make_context_current();

        //
        // Context Settings
        //

        // Create shader program
        _point_light_source_shader
            .attach_vertex_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kLightSourceVertexShader).c_str() })
            .attach_fragment_shader({ shaders::glslShaderVersion, shader_prep.process_from_memory(shaders::kLightSourceFragmentShader).c_str() })
            .link();

        //////////////////////////////////////////////////////////////////////////////////////////////////////////////

        using position_value_type = vec3_f32;
        using normal_value_type = vec3_f32;
        using triangle_index_value_type = vec3_i32;

        std::vector<position_value_type> vertex_positions;
        std::vector<normal_value_type> vertex_normals;
        std::vector<triangle_index_value_type> triangle_indices;

        _create_sphere_vertices(
            0.04f,
            vertex_positions,
            vertex_normals,
            triangle_indices
        );
        
        const GLsizei 
            positions_size_bytes = static_cast<GLsizei>(vertex_positions.size() * sizeof(position_value_type)),
            normals_size_bytes = static_cast<GLsizei>(vertex_normals.size() * sizeof(normal_value_type));
        
        const GLintptr
            positions_offset = 0,
            normals_offset   = 0 + positions_size_bytes;

        const GLsizeiptr
            total_vertices_size_bytes = static_cast<GLsizeiptr>(positions_size_bytes + normals_size_bytes),
            total_indices_size_bytes = static_cast<GLsizeiptr>(triangle_indices.size() * sizeof(triangle_index_value_type));

        //////////////////////////////////////////////////////////////////////////////////////////////////////////////

        GLCall(::glCreateVertexArrays(1, &_vao));
        GLCall(::glCreateBuffers(1, &_vbo));
        GLCall(::glCreateBuffers(1, &_ibo));
        
        GLCall(::glEnableVertexArrayAttrib(_vao, 0/*attribindex*/)); // attrib 0 = positions
        GLCall(::glEnableVertexArrayAttrib(_vao, 1/*attribindex*/)); // attrib 1 = normals

        GLCall(::glVertexArrayAttribBinding(_vao, 0/*attribindex*/, 0/*bindingindex*/)); // attrib 0 <- binding 0
        GLCall(::glVertexArrayAttribBinding(_vao, 1/*attribindex*/, 1/*bindingindex*/)); // attrib 1 <- binding 1

        GLCall(::glVertexArrayElementBuffer(_vao, _ibo));

        //////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // Link VAO's binding index to the VBO.
        GLCall(::glVertexArrayVertexBuffer(_vao, 0/*bindingindex*/, _vbo, positions_offset, sizeof(position_value_type)/*stride*/));
        GLCall(::glVertexArrayVertexBuffer(_vao, 1/*bindingindex*/, _vbo, normals_offset, sizeof(normal_value_type)/*stride*/));
        
        // Define the format of each vertex attributes
        GLCall(::glVertexArrayAttribFormat(_vao, 0/*attribindex*/, position_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));
        GLCall(::glVertexArrayAttribFormat(_vao, 1/*attribindex*/, normal_value_type::SizeAtCompileTime/*size*/, GL_FLOAT, GL_FALSE, 0/*relativeoffset*/));

        //////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // Allocate & upload vertices to VBO
        GLCall(::glNamedBufferStorage(_vbo, total_vertices_size_bytes, nullptr/* initial data */, GL_DYNAMIC_STORAGE_BIT)); // allocate space only
        GLCall(::glNamedBufferSubData(_vbo, positions_offset, positions_size_bytes, vertex_positions.data()));
        GLCall(::glNamedBufferSubData(_vbo, normals_offset, normals_size_bytes, vertex_normals.data()));

        // Allocate & upload indices to IBO
        GLCall(::glNamedBufferStorage(_ibo, total_indices_size_bytes, triangle_indices.data()/* initial data */, GL_DYNAMIC_STORAGE_BIT)); // allocate & copy data
        
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////

        _num_triangle_indices = static_cast<GLsizei>(triangle_indices.size() * triangle_index_value_type::SizeAtCompileTime);
    }

    void light_source_renderer::destroy_impl()
    {
        if (this->is_created())
        {
            this->set_creation_flag(false);

            _num_triangle_indices = 0;

            GLCall(::glDeleteBuffers(1, &_ibo));
            GLCall(::glDeleteBuffers(1, &_vbo));
            GLCall(::glDeleteVertexArrays(1, &_vao));

            _point_light_source_shader.destroy();
        }
    }

    void light_source_renderer::render_impl(
        const render_context& render_ctx)
    {
        if (render_ctx.curr_render_pass != render_pass_type::wboit_solid_rendering) {
            return;
        }

        // Enable depth testing
        //GLCall(::glEnable(GL_DEPTH_TEST));

        const auto& point_light = render_ctx.light_opts->point_light;
        
        if (point_light.enabled &&
            point_light.show_light_source)
        {
            mat4_f32 model{ math::mat4_identity<float>() };
            model.block<3, 1>(0, 3) = point_light.position;

            auto& draw_shader = _point_light_source_shader;
            draw_shader.use();
    
            // Update view/projective matrices in shader
            draw_shader.set_uniform_mat4("u_view", render_ctx.view);
            draw_shader.set_uniform_mat4("u_proj", render_ctx.projection);
    
            // Update model(transform) matrix in shader
            draw_shader.set_uniform_mat4("u_model", model);

            // Update color
            draw_shader.set_uniform_vec3("u_color", point_light.color.to_eigen() * point_light.light_source_color_intensity);

            // Render triangles
            GLCall(::glBindVertexArray(_vao)); // Bind VAO
            GLCall(::glDrawElementsBaseVertex(
                GL_TRIANGLES,
                _num_triangle_indices,
                GL_UNSIGNED_INT,
                nullptr/* const GLvoid* indices */,
                0/* GLint basevertex */
            ));
            //GLCall(::glBindVertexArray(0)); // Unbind VAO (optional)
        }

    }

} // namespace