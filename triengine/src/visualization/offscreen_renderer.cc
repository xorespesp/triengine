#include "offscreen_renderer.hh"
#include "../misc/string_utils.hh"
#include "../misc/debug_utils.hh"
#include "../misc/gl_utils.hh"

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
        const int32_t window_height,
        const bool multisample)
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
        _fb_sample_count = (multisample) ? 8 : 1;

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

    void offscreen_renderer::_begin_frame()
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

    void offscreen_renderer::_end_frame()
    {
        _fb_main.unbind();
    }

} // namespace
