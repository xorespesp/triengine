#pragma once
#include <triengine/common.h>
#include <triengine/camera.hh>
#include <triengine/texture.hh>
#include <triengine/lighting_options.hh>
#include <triengine/utility/gl_utils.hh>
#include <triengine/utility/noncopyable.hh>
#include <triengine/core/gl_context.hh>
#include <triengine/core/shader.hh>
#include <triengine/core/shader_preprocessor.hh>

#include <list>

namespace triengine::renderer
{
    enum class render_pass_type
    {
        wboit_solid_rendering = 0,
        wboit_transparent_rendering,
        wireframe_rendering,
    };

    struct render_context
    {
        mat4_f32 view{};
        mat4_f32 projection{};
        lighting_options const* light_opts{ nullptr };
        camera const* camera{ nullptr };
        render_pass_type curr_render_pass{ renderer::render_pass_type::wboit_solid_rendering };
    };

    template <typename _Derived>
    class renderer_base
        : utility::noncopyable
    {
    private:
        bool _creation_flag{ false };

    protected:
        void set_creation_flag(bool created) {
            _creation_flag = created;
        }

    public:
        renderer_base() = default;
        virtual ~renderer_base() {
            if (this->is_created()) {
                this->destroy();
            }
        }

        bool is_created() const noexcept {
            return _creation_flag;
        }

        void create(core::gl_context& glctx, const core::shader_preprocessor& shader_prep) {
            static_cast<_Derived*>(this)->create_impl(glctx, shader_prep);
        }

        void destroy() {
            static_cast<_Derived*>(this)->destroy_impl();
        }

        void render(const render_context& render_ctx) {
            static_cast<_Derived*>(this)->render_impl(render_ctx);
        }

    }; // class

    template <typename _Derived, typename _RenderObject>
    class object_renderer_base
        : utility::noncopyable
    {
    public:
        using render_object_type = _RenderObject;
        using pred_callback_type = bool(*)(const render_object_type& obj, void* userdata);

    private:
        bool _creation_flag{ false };

    protected:
        void set_creation_flag(bool created) {
            _creation_flag = created;
        }

    public:
        object_renderer_base() = default;
        virtual ~object_renderer_base() {
            if (this->is_created()) {
                this->destroy();
            }
        }

        bool is_created() const noexcept {
            return _creation_flag;
        }

        void create(core::gl_context& glctx, const core::shader_preprocessor& shader_prep) {
            static_cast<_Derived*>(this)->create_impl(glctx, shader_prep);
        }

        void destroy() {
            static_cast<_Derived*>(this)->destroy_impl();
        }

        void render(
            const render_context& render_ctx,
            const std::list<std::shared_ptr<render_object_type>>& render_obj_list,
            pred_callback_type const predicate = nullptr,
            void* const predicate_userdata = nullptr
        ) {
            static_cast<_Derived*>(this)->render_impl(
                render_ctx,
                render_obj_list,
                predicate,
                predicate_userdata
            );
        }

    }; // class

} // namespace
