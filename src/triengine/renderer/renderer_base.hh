#pragma once
#include <triengine/common.h>
#include <triengine/camera.hh>
#include <triengine/lighting_options.hh>
#include <triengine/utility/gl_utils.hh>
#include <triengine/utility/noncopyable.hh>
#include <triengine/core/gl_context.hh>

#include <list>

namespace triengine::renderer
{
    enum class render_pass_type
    {
        deferred_opaque_pass = 0, // deferred rendering(geometry + lighting) pass (WBOIT opaque rendering pass)
        forward_opaque_pass, // forward opaque rendering pass (this is also used in final overlay rendering pass)
        forward_transparent_pass, // forward transparent rendering pass (WBOIT transparent rendering pass)
    };

    struct render_context
    {
        mat4_f32 view{};
        mat4_f32 projection{};
        lighting_options const* light_opts{ nullptr };
        color3_f32 simple_fog_color{}; // NOTE: must be same as the background clear color
        abstract_camera const* camera{ nullptr };
        render_pass_type curr_render_pass{};
    };

    template<typename _RenderObject>
    using pred_callback_type = bool(*)(const _RenderObject& obj, void* userdata);

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
        // NOTE: Cleanup MUST not be performed here; `_Derived` has already been destroyed. (Refer to UB, see C.82)
        virtual ~renderer_base() = default;

        bool is_created() const noexcept {
            return _creation_flag;
        }

        template <typename... _Args>
        void create(core::gl_context& glctx, _Args&&... create_args) {
            static_cast<_Derived*>(this)->create_impl(glctx, std::forward<_Args>(create_args)...);
        }

        void destroy() {
            static_cast<_Derived*>(this)->destroy_impl();
        }

        template <typename... _Args>
        void render(const render_context& render_ctx, _Args&&... render_args) {
            static_cast<_Derived*>(this)->render_impl(render_ctx, std::forward<_Args>(render_args)...);
        }

    }; // class

} // namespace
