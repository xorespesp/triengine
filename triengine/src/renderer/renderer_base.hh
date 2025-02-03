#pragma once
#include "../common.h"
#include "../misc/gl_utils.hh"
#include "../shader.hh"
#include "../texture.hh"
#include "../lighting_options.hh"
#include "../camera.hh"

#include <list>

namespace triengine::renderer
{
    struct render_context
    {
        mat4_f32 view{};
        mat4_f32 projection{};
        lighting_options const* light_opts{ nullptr };
        camera const* camera{ nullptr };
    };

    class renderer_base
    {
    private:
        bool _creation_flag{ false };

    protected:
        void set_creation_flag(bool created) {
            _creation_flag = created;
        }

    public:
        renderer_base() = default;
        virtual ~renderer_base() = default;

        renderer_base(const renderer_base&) = delete;
        renderer_base& operator= (const renderer_base&) = delete;

        bool is_created() const noexcept {
            return _creation_flag;
        }

        virtual void create(GLFWwindow* window) = 0;
        virtual void destroy() = 0;
        virtual void render(const render_context& render_ctx) = 0;

    }; // class

    template <typename _RenderObject>
    class object_renderer_base
    {
    public:
        using render_object_type = _RenderObject;

    private:
        bool _creation_flag{ false };

    protected:
        void set_creation_flag(bool created) {
            _creation_flag = created;
        }

    public:
        object_renderer_base() = default;
        virtual ~object_renderer_base() = default;

        object_renderer_base(const object_renderer_base&) = delete;
        object_renderer_base& operator= (const object_renderer_base&) = delete;

        bool is_created() const noexcept {
            return _creation_flag;
        }

        virtual void create(GLFWwindow* window) = 0;
        virtual void destroy() = 0;
        virtual void render(
            const render_context& render_ctx, 
            const std::list<std::shared_ptr<render_object_type>>& render_obj_list
        ) = 0;

    }; // class

} // namespace
