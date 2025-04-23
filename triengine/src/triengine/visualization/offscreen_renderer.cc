#include "offscreen_renderer.hh"
#include <triengine/misc/string_utils.hh>
#include <triengine/misc/debug_utils.hh>
#include <triengine/misc/gl_utils.hh>

#include <iostream>
#include <memory>

/**
 * Offscreen rendering in GLFW
 * https://github.com/glfw/glfw/blob/master/examples/offscreen.c
 */

namespace triengine::visualization
{
    offscreen_renderer::offscreen_renderer()
    {
    }

    void offscreen_renderer::create_renderer(
        const int32_t window_width,
        const int32_t window_height)
    {
        if (_flag_initialized) {
            TRIENGINE_PANIC("already created");
        }

        _glctx.create(
            "",
            false,
            window_width,
            window_height,
            false
        );

        ::glfwGetWindowSize(_glctx.get_glfw_window(), &_curr_window_width, &_curr_window_height);

        _scn_renderer.create(&_glctx);

        // Create main scene
        this->add_scene();

        TRIENGINE_TRACE("%s() LEAVE", __func__);
        _flag_initialized = true;
    }

    void offscreen_renderer::destroy_renderer()
    {
        if (_flag_initialized)
        {
            _flag_initialized = false;

            _scn_renderer.destroy();
            _glctx.destroy();
        }
    }

    bool offscreen_renderer::add_scene(std::shared_ptr<scene> scn)
    {
        if (_scn_map.empty()) { _curr_scn = scn; }
        const auto [it, success] = _scn_map.insert({ scn->id(), scn });
        return success;
    }
    
    void offscreen_renderer::remove_scene(std::shared_ptr<scene> scn)
    {
        if (scn) {
            auto it = _scn_map.find(scn->id());
            if (it != _scn_map.end()) {
                _scn_map.erase(it);
                if (_curr_scn->id() == scn->id()) {
                    _curr_scn = _scn_map.empty() ? nullptr : _scn_map.begin()->second;
                }
            }
        }
    }

    void offscreen_renderer::change_scene(std::shared_ptr<scene> scn) {
        _curr_scn = scn;
    }

    void offscreen_renderer::render(
        image& frame_image)
    {
        ::glfwSwapBuffers(_glctx.get_glfw_window());

        const vec2_i32 scn_size{ _curr_window_width, _curr_window_height };

        this->_begin_frame();
        if (_curr_scn)
        {
            scene& scn = *_curr_scn;
            scn.get_camera()->set_view_port(view_port{ 0, 0, scn_size.x(),  scn_size.y() });
            _scn_renderer.render(_fb_main, scn);

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

        GLCall(::glBindTexture(GL_TEXTURE_2D, _fb_main.color_attachment()->buffer_id));
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

    void offscreen_renderer::_begin_frame()
    {
        if (_flag_invalidate_fbo)
        {
            // invalidate framebuffer

            const int32_t
                width_pixels = _curr_window_width,
                height_pixels = _curr_window_height;

            if (!_fb_main.is_valid())
            {
                _fb_main = frame_buffer::create_color_depth_stencil_buffer(
                    GL_RGBA16F,
                    GL_DEPTH_COMPONENT24,
                    GL_STENCIL_INDEX8,
                    _curr_window_width,
                    _curr_window_height
                );
            }
            else
            {
                // It is okay to call reallocate every frame, 
                // as there is an internal reallocation-skip optimization implemented.
                _fb_main.reallocate(
                    width_pixels,
                    height_pixels
                );
            }

            _flag_invalidate_fbo = false;
        }

        _fb_main.bind();
    }

    void offscreen_renderer::_end_frame()
    {
        _fb_main.unbind();
    }

} // namespace
