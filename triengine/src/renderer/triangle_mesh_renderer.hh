#pragma once
#include "renderer_base.hh"
#include "../geometry/triangle_mesh_object.hh"

#include <list>

namespace triengine::renderer
{
    class triangle_mesh_renderer
        : public renderer_base<geometry::triangle_mesh_object>
    {
    private:
        // OpenGL shaders
        shader_program 
            _shader_vertmode,
            _shader_texmode,
            _shader_normal_view;

        // OpenGL objects
        GLuint // vertex array object
            _vao_vertmode{}, // Coloring by linear-interpolation from vertices, requires vertex colors (as rgb)
            _vao_texmode{}; // Coloring by texture, requires texture uv coordinates (color information)
        GLuint // vertex buffer objects
            _vbo_positions{},
            _vbo_normals{},
            _vbo_colors{},
            _vbo_texcoords{}; // texture uv coordinates

        GLuint _ibo{}; // index buffer object

        // OpenGL shader uniform locations
        GLint _uloc_vertmode_model{}; // model matrix
        GLint _uloc_vertmode_view{}; // view matrix
        GLint _uloc_vertmode_proj{}; // projection matrix

        GLint _uloc_texmode_model{}; // model matrix
        GLint _uloc_texmode_view{}; // view matrix
        GLint _uloc_texmode_proj{}; // projection matrix

        bool _show_object_normals{ false };

    public:
        triangle_mesh_renderer();

        // Toggle object's normal visualization.
        void enable_object_normal_rendering(bool enable);

        // Renderer functions
        void create(GLFWwindow* window) override;
        void destroy() override;
        void render(
            const mat4_f32& view,
            const mat4_f32& projection,
            const lighting_options& light_opts
        ) override;

        void render(
            const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects,
            const mat4_f32& view,
            const mat4_f32& projection,
            const lighting_options& light_opts
        );

    private:
        void _render_vertex_shading_objects(
            const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects,
            const mat4_f32& view,
            const mat4_f32& projection,
            const lighting_options& light_opts
        );

        void _render_texture_shading_objects(
            const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects,
            const mat4_f32& view,
            const mat4_f32& projection,
            const lighting_options& light_opts
        );
        
        void _render_objects_normals(
            const std::list<std::shared_ptr<geometry::triangle_mesh_object>>& render_objects,
            const mat4_f32& view,
            const mat4_f32& projection
        );

    }; // class

} // namespace