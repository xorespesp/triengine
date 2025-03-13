#pragma once
#include "renderer_base.hh"
#include "../geometry/pcd_object.hh"

#include <optional>

namespace triengine::renderer
{
    class pcd_renderer
        : public object_renderer_base<pcd_renderer, geometry::pcd_object>
    {
    private:
        // Render options
        std::optional<float> _point_size;

        // OpenGL shaders
        shader_program 
            _solid_shader, 
            _transparent_shader;

        // OpenGL objects
        GLuint _vao{}; // vertex array object
        GLuint // vertex buffer objects
            _vbo_positions{}, 
            _vbo_normals{}, 
            _vbo_colors{};

    public:
        pcd_renderer();
        virtual ~pcd_renderer();

        void set_pcd_point_size(float point_size);

        // CRTP methods
        void create_impl(GLFWwindow* window);
        void destroy_impl();
        void render_impl(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
            pred_callback_type predicate,
            void* predicate_userdata);

    };

} // namespace