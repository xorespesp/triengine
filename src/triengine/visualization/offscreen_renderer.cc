#include "offscreen_renderer.hh"
#include <triengine/scene_list.hh>
#include <triengine/core/gl_context.hh>
#include <triengine/core/scene_renderer.hh>
#include <triengine/core/frame_buffer.hh>
#include <triengine/utility/string_format.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

#include <iostream>
#include <memory>

/**
 * Offscreen rendering in GLFW
 * https://github.com/glfw/glfw/blob/master/examples/offscreen.c
 */

namespace triengine::visualization
{
    struct offscreen_renderer::impl
    {
        bool is_created{ false };
        bool flag_invalidate_fbo{ true };

        core::gl_context glctx;
        vec2_i32 curr_frame_size{};

        core::scene_renderer scn_renderer;

        scene_list scenes;

        core::frame_buffer fb_main;

        // frame time calculation
        double frame_time_delta{ 0.0 }, last_frame_time{ 0.0 };

    public:
        void render(image_buffer& frame_image, image_format_type frame_image_format);
        void begin_frame();
        void end_frame();
    };

    offscreen_renderer::offscreen_renderer()
        : _imp{ std::make_unique<impl>() }
    {
        // Window half (GLFW main thread): create the hidden window and its GL context
        // now. The context is initialized later by `create()` on the render thread.
        // 
        // The window is only a carrier for the GL context; offscreen output goes
        // to an FBO sized by `create()`/`resize_frame()`, so a 1x1 placeholder window
        // is enough here and the real size is committed later.
        _imp->glctx.create_window(
            "",
            false, // hidden window: offscreen output only
            1, 1,  // placeholder size. (the render target is sized by `create()`)
            false
        );
    }

    offscreen_renderer::~offscreen_renderer()
    {
        // `destroy()` must run on the render thread before this object is destroyed;
        // the destructor (GLFW main thread) cannot tear down the GL context safely.
        // A failure here means `destroy()` was skipped, leaving the GL context to be
        // released in a disorderly way (see the class threading contract).
        TRIENGINE_ASSERT(!_imp->is_created);
        _imp->glctx.destroy_window();
    }

    const core::graphics_device_info* offscreen_renderer::get_graphics_device_info() const noexcept
    {
        return _imp->glctx.get_device_info();
    }

    void offscreen_renderer::create(const vec2_i32 initial_frame_size)
    {
        if (_imp->is_created) {
            TRIENGINE_PANIC("offscreen_renderer::create() called again without a matching destroy()");
        }

        _imp->glctx.init_context();

        // Set the initial offscreen render target size.
        this->resize_frame(initial_frame_size);
        _imp->scn_renderer.create(&_imp->glctx);

        _imp->is_created = true;
        TRIENGINE_TRACE("%s() LEAVE", __func__);
    }

    void offscreen_renderer::destroy()
    {
        if (_imp->is_created)
        {
            _imp->is_created = false;

            // NOTE: `fb_main` owns GL objects outside the context's GPU resource manager, so release it while this context is current.
            // If left to the destructor(GLFW main thread), the delete hits that thread's current context and destroys its same-named objects.
            _imp->fb_main.destroy();
            _imp->flag_invalidate_fbo = true; // reset invalidate flag to later `create()` allocates the framebuffer again

            _imp->scn_renderer.destroy();
            _imp->glctx.reset_context();
        }
    }

    std::shared_ptr<scene> offscreen_renderer::add_scene()
    {
        auto new_scn = std::make_shared<scene>(_imp->glctx.get_gpu_resource_manager());
        _imp->scenes.add(new_scn);
        return new_scn;
    }

    void offscreen_renderer::remove_scene(scene_id_t scn_id)
    {
        _imp->scenes.remove(scn_id);
    }

    void offscreen_renderer::switch_scene(scene_id_t scn_id)
    {
        _imp->scenes.switch_to(scn_id);
    }

    void offscreen_renderer::switch_to_previous_scene()
    {
        _imp->scenes.switch_to_previous();
    }

    void offscreen_renderer::switch_to_next_scene()
    {
        _imp->scenes.switch_to_next();
    }

    std::shared_ptr<const scene> offscreen_renderer::find_scene(scene_id_t scn_id) const
    {
        return _imp->scenes.find(scn_id);
    }

    std::shared_ptr<scene> offscreen_renderer::find_scene(scene_id_t scn_id)
    {
        return _imp->scenes.find(scn_id);
    }

    std::shared_ptr<const scene> offscreen_renderer::get_current_scene() const
    {
        return _imp->scenes.current();
    }

    std::shared_ptr<scene> offscreen_renderer::get_current_scene()
    {
        return _imp->scenes.current();
    }

    vec2_i32 offscreen_renderer::get_frame_size() const noexcept
    {
        return _imp->curr_frame_size;
    }

    void offscreen_renderer::resize_frame(const vec2_i32 new_frame_size)
    {
        if (new_frame_size.x() <= 0 || new_frame_size.y() <= 0) {
            TRIENGINE_PANIC("Invalid frame size (%d, %d)", new_frame_size.x(), new_frame_size.y());
        }
        _imp->curr_frame_size = new_frame_size;
        _imp->flag_invalidate_fbo = true;
    }

    void offscreen_renderer::render(
        image_buffer& frame_image,
        const image_format_type frame_image_format)
    {
        _imp->render(frame_image, frame_image_format);
    }

    void offscreen_renderer::impl::render(
        image_buffer& frame_image,
        const image_format_type frame_image_format)
    {
        const std::shared_ptr<scene> curr_scn = scenes.current();
        if (!curr_scn) {
            TRIENGINE_PANIC("No scenes added");
        }

        // Calculate frame delta time
        const double curr_frame_time = ::glfwGetTime();
        frame_time_delta = curr_frame_time - last_frame_time;
        last_frame_time = curr_frame_time;
        const float frame_delta_f32 = static_cast<float>(frame_time_delta);

        glctx.swap_buffers();

        scene& target_scn = *curr_scn;
        abstract_camera& target_scn_camera = *target_scn.get_camera();
        target_scn_camera.set_viewport(view_port{ 0, 0, curr_frame_size.x(), curr_frame_size.y() });

        // Process camera input
        target_scn_camera.update_animation(frame_delta_f32);

        this->begin_frame();
        scn_renderer.render(
            fb_main.fbo_id(),
            fb_main.width_pixels(),
            fb_main.height_pixels(),
            target_scn
        );
        this->end_frame();

        GLCall(::glBindTexture(GL_TEXTURE_2D, fb_main.color_attachment()->buffer_id));
        frame_image.prepare(curr_frame_size.x(), curr_frame_size.y(), frame_image_format);

        // TODO: validate image format
        const GLenum frame_image_gl_format{ static_cast<GLenum>(frame_image_format) };
        GLCall(::glGetTexImage(
            GL_TEXTURE_2D,         /* GLenum target */
            0,                     /* GLint level */
            frame_image_gl_format, /* GLenum format */
            GL_UNSIGNED_BYTE,      /* GLenum type */
            frame_image.data()   /* void* pixels */
        ));

        GLCall(::glBindTexture(GL_TEXTURE_2D, 0));
    }

    void offscreen_renderer::impl::begin_frame()
    {
        if (flag_invalidate_fbo)
        {
            // invalidate framebuffer

            const int32_t
                frame_width_pixels = curr_frame_size.x(),
                frame_height_pixels = curr_frame_size.y();

            if (!fb_main.is_valid())
            {
                fb_main = core::frame_buffer::create_color_only_buffer(
                    GL_RGBA16F,
                    frame_width_pixels,
                    frame_height_pixels
                );
            }
            else
            {
                // It is okay to call reallocate every frame, 
                // as there is an internal reallocation-skip optimization implemented.
                fb_main.reallocate(
                    frame_width_pixels,
                    frame_height_pixels
                );
            }

            flag_invalidate_fbo = false;
        }

        fb_main.bind();
    }

    void offscreen_renderer::impl::end_frame()
    {
        fb_main.unbind();
    }

} // namespace
