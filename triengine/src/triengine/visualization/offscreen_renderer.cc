#include "offscreen_renderer.hh"
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

        _curr_frame_size = _glctx.get_window_size();

        _scn_renderer.create(&_glctx);

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

    std::shared_ptr<scene> offscreen_renderer::add_scene()
    {
        auto new_scn = std::make_shared<scene>(_glctx.get_gpu_resource_manager());
        if (_scn_id_map.count(new_scn->get_id())) {
            TRIENGINE_PANIC("Failed to add scene (id #%X already exists)", new_scn->get_id());
        }

        const bool is_first{ _scn_list.empty() };

        _scn_list.push_back(new_scn);
        _scn_id_map[new_scn->get_id()] = std::prev(_scn_list.end());

        if (is_first) {
            _curr_scn_it = std::prev(_scn_list.end());
        }

        return new_scn;
    }
    
    void offscreen_renderer::remove_scene(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        if (map_it != _scn_id_map.end()) {
            _scn_list.erase(map_it->second);
            _scn_id_map.erase(map_it);
            if ((*_curr_scn_it)->get_id() == scn_id) {
                _curr_scn_it = _scn_id_map.empty() 
                    ? _scn_list.end() 
                    : _scn_list.begin();
            }
        } else {
            TRIENGINE_WARN("Failed to remove scene #%X (not found)", scn_id);
        }
    }

    void offscreen_renderer::switch_scene(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        if (map_it == _scn_id_map.end()) {
            TRIENGINE_PANIC("Failed to change scene (invalid scene id #%X)", scn_id);
        }
        _curr_scn_it = map_it->second;
    }

    void offscreen_renderer::switch_to_previous_scene()
    {
        if (_curr_scn_it != _scn_list.end()) {
            _curr_scn_it = std::prev((_curr_scn_it != _scn_list.begin())
                ? _curr_scn_it
                : _scn_list.end()
            );
        }
    }

    void offscreen_renderer::switch_to_next_scene()
    {
        if (_curr_scn_it != _scn_list.end()) {
            const auto next_it = std::next(_curr_scn_it);
            _curr_scn_it = (next_it != _scn_list.end())
                ? next_it
                : _scn_list.begin();
        }
    }

    std::shared_ptr<const scene> offscreen_renderer::find_scene(scene_id_t scn_id) const
    {
        auto map_it = _scn_id_map.find(scn_id);
        return (map_it != _scn_id_map.end())
            ? *(map_it->second)
            : nullptr;
    }

    std::shared_ptr<scene> offscreen_renderer::find_scene(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        return (map_it != _scn_id_map.end())
            ? *(map_it->second)
            : nullptr;
    }

    std::shared_ptr<const scene> offscreen_renderer::get_current_scene() const
    {
        return (_curr_scn_it != _scn_list.end())
            ? *_curr_scn_it
            : nullptr;
    }

    std::shared_ptr<scene> offscreen_renderer::get_current_scene()
    {
        return (_curr_scn_it != _scn_list.end())
            ? *_curr_scn_it
            : nullptr;
    }

    vec2_i32 offscreen_renderer::get_frame_size() const noexcept
    {
        return _curr_frame_size;
    }

    void offscreen_renderer::resize_frame(int32_t width, int32_t height)
    {
        if (width <= 0 || height <= 0) {
            TRIENGINE_PANIC("Invalid frame size (%d, %d)", width, height);
        }
        _curr_frame_size = vec2_i32{ width, height };
        _flag_invalidate_fbo = true;
    }

    void offscreen_renderer::render(
        image_buffer& frame_image,
        const image_format_type frame_image_format)
    {
        if (_curr_scn_it == _scn_list.end()) {
            TRIENGINE_PANIC("No scenes added");
        }

        // Calculate frame delta time
        const double curr_frame_time = ::glfwGetTime();
        _frame_time_delta = curr_frame_time - _last_frame_time;
        _last_frame_time = curr_frame_time;
        const float frame_delta_f32 = static_cast<float>(_frame_time_delta);

        _glctx.swap_buffers();

        scene& target_scn = *(_curr_scn_it->get());
        abstract_camera& target_scn_camera = *target_scn.get_camera();
        target_scn_camera.set_viewport(view_port{ 0, 0, _curr_frame_size.x(), _curr_frame_size.y() });

        // Process camera input
        target_scn_camera.update_animation(frame_delta_f32);

        this->_begin_frame();
        _scn_renderer.render(
            _fb_main.fbo_id(),
            _fb_main.width_pixels(),
            _fb_main.height_pixels(),
            target_scn
        );
        this->_end_frame();

        GLCall(::glBindTexture(GL_TEXTURE_2D, _fb_main.color_attachment()->buffer_id));
        frame_image.prepare(_curr_frame_size.x(), _curr_frame_size.y(), frame_image_format);

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

    void offscreen_renderer::_begin_frame()
    {
        if (_flag_invalidate_fbo)
        {
            // invalidate framebuffer

            const int32_t
                frame_width_pixels = _curr_frame_size.x(),
                frame_height_pixels = _curr_frame_size.y();

            if (!_fb_main.is_valid())
            {
                _fb_main = core::frame_buffer::create_color_only_buffer(
                    GL_RGBA16F,
                    frame_width_pixels,
                    frame_height_pixels
                );
            }
            else
            {
                // It is okay to call reallocate every frame, 
                // as there is an internal reallocation-skip optimization implemented.
                _fb_main.reallocate(
                    frame_width_pixels,
                    frame_height_pixels
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
