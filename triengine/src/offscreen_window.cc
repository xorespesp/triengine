#include "offscreen_window.hh"
#include "misc/string_utils.hh"
#include "misc/debug_utils.hh"
#include "misc/gl_utils.hh"

#include <iostream>
#include <memory>

/**
 * Offscreen rendering in GLFW
 * https://github.com/glfw/glfw/blob/master/examples/offscreen.c
 */

namespace triengine
{
    offscreen_window::offscreen_window()
    {
    }

    void offscreen_window::create_window(
        const int32_t width,
        const int32_t height,
        const bool multisample)
    {
        if (_flag_initialized) {
            TRIENGINE_PANIC("already created");
        }

        _glctx.create(
            "",
            false,
            width,
            height,
            false
        );

        // In to use the member function as callback, set the current class as the Window User Pointer
        ::glfwSetWindowUserPointer(_glctx.get_glfw_window(), this);

        // Set all callbacks
        ::glfwSetWindowCloseCallback(_glctx.get_glfw_window(),
            +[](GLFWwindow* window) {
                auto pThis = static_cast<offscreen_window*>(::glfwGetWindowUserPointer(window));
                //pThis->_handle_glfw_window_close_event(window);
            });

        ::glfwSetFramebufferSizeCallback(_glctx.get_glfw_window(),
            +[](GLFWwindow* window, int w, int h) {
                auto pThis = static_cast<offscreen_window*>(::glfwGetWindowUserPointer(window));
                pThis->_curr_window_width = w;
                pThis->_curr_window_height = h;
            });

        ::glfwGetWindowSize(_glctx.get_glfw_window(), &_curr_window_width, &_curr_window_height);
        _fb_sample_count = (multisample) ? 8 : 1;

        _scn_renderer.create(&_glctx);

        this->create_new_scene();

        TRIENGINE_TRACE("%s() LEAVE", __func__);
        _flag_initialized = true;
    }

    void offscreen_window::destroy_window()
    {
        if (_flag_initialized)
        {
            _flag_initialized = false;

            _scn_renderer.destroy();
            _glctx.destroy();
        }
    }

    bool offscreen_window::update_window()
    {
        ::glfwSwapBuffers(_glctx.get_glfw_window());
        ::glfwPollEvents();

        /**
         * https://www.glfw.org/docs/3.0/window.html
         *
         * When the user attempts to close the window,
         * for example by clicking the close widget or using a key chord like Alt+F4,
         * the close flag of the window is set.
         *
         * The window is however not actually destroyed and, unless you watch for this state change, nothing further happens.
         * The current state of the close flag is returned by glfwWindowShouldClose and can be set or cleared directly with glfwSetWindowShouldClose.
         */
        return !static_cast<bool>(::glfwWindowShouldClose(_glctx.get_glfw_window()));
    }

    void offscreen_window::render(
        image& frame_image)
    {
        const vec2_i32 scn_size{ _curr_window_width, _curr_window_height };

        this->_begin_frame();
        if (_curr_scn)
        {
            scene& scn = *_curr_scn;
            scn.get_camera()->set_view_port(view_port{ 0, 0, scn_size.x(),  scn_size.y() });
            _scn_renderer.render_scene(scn);

            //frame_image.prepare(W, H, image_format_type::bgr);
            //GLCall(::glReadPixels(
            //    0, 0,              /* GLint x, GLint y */
            //    W, H,              /* GLsizei width, GLsizei height */
            //    GL_BGR,            /* GLenum format */
            //    GL_UNSIGNED_BYTE,  /* GLenum type */
            //    frame_image.data() /* void* pixels */
            //));
        }
        this->_end_frame();

        const GLuint frame_image_texure_id = 
            [this]() -> GLuint {
                const bool msaa_enabled = _fb_sample_count > 1;
                if (msaa_enabled) {
                    _fb_main.blit_to(_fb_msaa_copy, true, false, false);
                    return _fb_msaa_copy.color_texture_id();
                } else {
                    return _fb_main.color_texture_id();
                }
            }();

        GLCall(::glBindTexture(GL_TEXTURE_2D, frame_image_texure_id));
        frame_image.prepare(scn_size.x(), scn_size.y(), image_format_type::bgra);
        GLCall(::glGetTexImage(
            GL_TEXTURE_2D,     /* GLenum target */
            0,                 /* GLint level */
            GL_BGRA,            /* GLenum format */
            GL_UNSIGNED_BYTE,  /* GLenum type */
            frame_image.data() /* void* pixels */
        ));
        GLCall(::glBindTexture(GL_TEXTURE_2D, 0));
    }

    void offscreen_window::_begin_frame()
    {
        if (_flag_invalidate_fbo)
        {
            // invalidate framebuffer

            const int32_t
                width_pixels = _curr_window_width,
                height_pixels = _curr_window_height;
            
            _fb_main.reserve(
                width_pixels,
                height_pixels,
                _fb_sample_count
            );

            _fb_msaa_copy.reserve(
                width_pixels,
                height_pixels,
                1
            );

            _flag_invalidate_fbo = false;
        }

        _fb_main.bind();
    }

    void offscreen_window::_end_frame()
    {
        _fb_main.unbind();
    }

} // namespace
