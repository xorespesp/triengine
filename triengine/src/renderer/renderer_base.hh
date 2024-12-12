#pragma once
#include "../common.h"
#include "../misc/opengl_utils.hh"
#include "../shader.hh"
#include "../texture.hh"
#include "../lighting_options.hh"

namespace triengine::renderer
{
    template <typename _RenderObject>
    class renderer_base
    {
    public:
        using render_object_type = _RenderObject;

    private:
        bool _creation_flag{ false };
        std::list<std::shared_ptr<render_object_type>> _render_objects;

    protected:
        void set_creation_flag(bool created) {
            _creation_flag = created;
        }

    public:
        renderer_base() = default;
        virtual ~renderer_base() = default;

        renderer_base(const renderer_base&) = delete;
        renderer_base& operator= (const renderer_base&) = delete;

        const auto& get_objects() const noexcept {
            return _render_objects;
        }

        void add_object(std::shared_ptr<render_object_type> object) {
            _render_objects.push_back(object);
        }

        void remove_object(std::shared_ptr<render_object_type> object) {
            _render_objects.remove(object);
        }

        void clear_objects() {
            _render_objects.clear();
        }

        bool is_created() const noexcept {
            return _creation_flag;
        }

        virtual void create(GLFWwindow* window) = 0;
        virtual void destroy() = 0;
        virtual void render(
            const mat4_f32& view,
            const mat4_f32& projection,
            const lighting_options& light_opts
        ) = 0;

    }; // class

} // namespace
