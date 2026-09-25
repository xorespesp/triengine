#include "visualizer.hh"
#include <triengine/utility/string_format.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

#include <iostream>
#include <memory>

namespace triengine::visualization
{
    visualizer::visualizer()
    { }

    const core::gl_context* visualizer::get_gl_context() const noexcept {
        return &_glctx;
    }

    core::gl_context* visualizer::get_gl_context() noexcept {
        return &_glctx;
    }

    vec2_i32 visualizer::get_window_size() const {
        return _glctx.get_window_size();
    }

    vec2_i32 visualizer::get_frame_size() const {
        return _frame_size;
    }

    vec2_f32 visualizer::get_window_dpi_scale() const {
        return _glctx.get_window_dpi_scale();
    }

    void visualizer::set_close_callback(close_callback_type cb) {
        _cb_close = std::move(cb);
    }

    void visualizer::set_dpi_change_callback(dpi_change_callback_type cb) {
        _cb_dpi_change = std::move(cb);
    }

    void visualizer::set_key_callback(key_callback_type cb) {
        _cb_key = std::move(cb);
    }

    void visualizer::set_mouse_button_callback(mouse_button_callback_type cb) {
        _cb_mouse_button = std::move(cb);
    }

    void visualizer::set_mouse_move_callback(mouse_move_callback_type cb) {
        _cb_mouse_move = std::move(cb);
    }

    void visualizer::set_mouse_scroll_callback(mouse_scroll_callback_type cb) {
        _cb_mouse_scroll = std::move(cb);
    }

    void visualizer::create_window(
        const std::string& window_name,
        const bool show_window,
        const int32_t window_width,
        const int32_t window_height,
        const bool fullscreen,
        const bool enable_vsync)
    {
        if (_flag_initialized) {
            TRIENGINE_PANIC("already created");
        }

        _glctx.create_window(
            window_name,
            show_window,
            window_width,
            window_height,
            fullscreen
        );

        _glctx.init_context(enable_vsync);

        _glctx.set_close_callback(std::bind(&visualizer::_handle_close_event, this,
            std::placeholders::_1));
        _glctx.set_frame_resize_callback(std::bind(&visualizer::_handle_frame_resize_event, this,
            std::placeholders::_1));
        _glctx.set_dpi_change_callback(std::bind(&visualizer::_handle_dpi_change_event, this,
            std::placeholders::_1));
        _glctx.set_key_callback(std::bind(&visualizer::_handle_key_event, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        _glctx.set_mouse_button_callback(std::bind(&visualizer::_handle_mouse_button_event, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        _glctx.set_mouse_move_callback(std::bind(&visualizer::_handle_mouse_move_event, this,
            std::placeholders::_1));
        _glctx.set_mouse_scroll_callback(std::bind(&visualizer::_handle_mouse_scroll_event, this,
            std::placeholders::_1));

        _scn_renderer.create(&_glctx);

        this->_on_window_created();

        _flag_initialized = true;
    }

    void visualizer::close_window()
    {
        _glctx.set_window_close_flag(true);
    }

    void visualizer::destroy_window()
    {
        if (_flag_initialized)
        {
            _flag_initialized = false;

            this->_on_window_destroying();

            _scene_fb.destroy();
            _scn_renderer.destroy();
            _glctx.reset_context();
            _glctx.destroy_window();
        }
    }

    void visualizer::set_window_position(int32_t xpos, int32_t ypos)
    {
        _glctx.set_window_position(xpos, ypos);
    }

    void visualizer::set_window_visible(bool visible)
    {
        _glctx.set_window_visible(visible);
    }

    std::shared_ptr<scene> visualizer::add_scene()
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

    void visualizer::remove_scene(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        if (map_it != _scn_id_map.end()) {
            // erase invalidates iterator, so we must check it before the erase.
            const bool removing_curr_scn = (_curr_scn_it == map_it->second);
            _scn_list.erase(map_it->second);
            _scn_id_map.erase(map_it);
            if (removing_curr_scn) {
                _curr_scn_it = _scn_id_map.empty()
                    ? _scn_list.end()
                    : _scn_list.begin();
            }
        } else {
            TRIENGINE_WARN("Failed to remove scene #%X (not found)", scn_id);
        }
    }

    void visualizer::switch_scene(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        if (map_it == _scn_id_map.end()) {
            TRIENGINE_PANIC("Failed to change scene (invalid scene id #%X)", scn_id);
        }
        _curr_scn_it = map_it->second;
    }

    void visualizer::switch_to_previous_scene()
    {
        if (_curr_scn_it != _scn_list.end()) {
            _curr_scn_it = std::prev((_curr_scn_it != _scn_list.begin())
                ? _curr_scn_it
                : _scn_list.end()
            );
        }
    }

    void visualizer::switch_to_next_scene()
    {
        if (_curr_scn_it != _scn_list.end()) {
            const auto next_it = std::next(_curr_scn_it);
            _curr_scn_it = (next_it != _scn_list.end())
                ? next_it
                : _scn_list.begin();
        }
    }

    std::shared_ptr<const scene> visualizer::find_scene(scene_id_t scn_id) const
    {
        auto map_it = _scn_id_map.find(scn_id);
        return (map_it != _scn_id_map.end())
            ? *(map_it->second)
            : nullptr;
    }

    std::shared_ptr<scene> visualizer::find_scene(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        return (map_it != _scn_id_map.end())
            ? *(map_it->second)
            : nullptr;
    }

    std::shared_ptr<const scene> visualizer::get_current_scene() const
    {
        return (_curr_scn_it != _scn_list.end())
            ? *_curr_scn_it
            : nullptr;
    }

    std::shared_ptr<scene> visualizer::get_current_scene()
    {
        return (_curr_scn_it != _scn_list.end())
            ? *_curr_scn_it
            : nullptr;
    }

    void visualizer::render()
    {
        if (_curr_scn_it == _scn_list.end()) {
            TRIENGINE_PANIC("No scenes added");
        }

        // Calculate frame delta time
        const double curr_frame_time = ::glfwGetTime();
        _frame_time_delta = curr_frame_time - _last_frame_time;
        _last_frame_time = curr_frame_time;
        const float frame_delta_f32 = static_cast<float>(_frame_time_delta);

        const scene_render_target render_target = this->_begin_scene_frame();
        _frame_size = render_target.frame_size;

        // Render Scene (skipped while the frame has no area, e.g. minimized window)
        if (render_target.frame_size.x() > 0 && render_target.frame_size.y() > 0)
        {
            scene& target_scn = *(_curr_scn_it->get());
            abstract_camera& target_scn_camera = *target_scn.get_camera();
            target_scn_camera.set_viewport(view_port{ 0, 0, render_target.frame_size.x(), render_target.frame_size.y() });

            // Process camera input
            target_scn_camera.update_animation(frame_delta_f32);
            if (_flag_camera_interaction && this->_is_scene_focused()) {
                if (_glctx.get_key_state(GLFW_KEY_W) == GLFW_PRESS) { target_scn_camera.process_keyboard_translation(camera_movement_type::forward, frame_delta_f32); }
                if (_glctx.get_key_state(GLFW_KEY_S) == GLFW_PRESS) { target_scn_camera.process_keyboard_translation(camera_movement_type::backward, frame_delta_f32); }
                if (_glctx.get_key_state(GLFW_KEY_A) == GLFW_PRESS) { target_scn_camera.process_keyboard_translation(camera_movement_type::left, frame_delta_f32); }
                if (_glctx.get_key_state(GLFW_KEY_D) == GLFW_PRESS) { target_scn_camera.process_keyboard_translation(camera_movement_type::right, frame_delta_f32); }
                if (_glctx.get_key_state(GLFW_KEY_UP) == GLFW_PRESS) { target_scn_camera.process_keyboard_translation(camera_movement_type::up, frame_delta_f32); }
                if (_glctx.get_key_state(GLFW_KEY_DOWN) == GLFW_PRESS) { target_scn_camera.process_keyboard_translation(camera_movement_type::down, frame_delta_f32); }
            }

            _scn_renderer.render(
                render_target.fbo_id,
                render_target.frame_size.x(),
                render_target.frame_size.y(),
                target_scn
            );
        }

        this->_end_scene_frame();
    }

    bool visualizer::update_window()
    {
        _glctx.swap_buffers();
        _glctx.poll_window_events();
        return !_glctx.get_window_close_flag();
    }

    visualizer::scene_render_target visualizer::_begin_scene_frame()
    {
        const vec2_i32 framebuffer_size = _glctx.get_framebuffer_size();
        if (framebuffer_size.x() <= 0 || framebuffer_size.y() <= 0) {
            return scene_render_target{ 0, framebuffer_size };
        }

        if (!_scene_fb.is_valid())
        {
            _scene_fb = core::frame_buffer::create_color_only_buffer(
                GL_RGBA16F,
                framebuffer_size.x(),
                framebuffer_size.y()
            );
        }
        else
        {
            // It is okay to call reallocate every frame,
            // as there is an internal reallocation-skip optimization implemented.
            _scene_fb.reallocate(
                framebuffer_size.x(),
                framebuffer_size.y()
            );
        }

        _scene_fb.bind();

        return scene_render_target{ _scene_fb.fbo_id(), framebuffer_size };
    }

    void visualizer::_end_scene_frame()
    {
        if (!_scene_fb.is_valid()) {
            return;
        }

        _scene_fb.unbind();

        // only the color buffer is presented;
        // the window backbuffer's depth/stencil is never read back
        _scene_fb.blit_to_default_framebuffer(
            _scene_fb.width_pixels(),
            _scene_fb.height_pixels(),
            true /* blit_color */,
            false /* blit_depth */,
            false /* blit_stencil */
        );
    }

    bool visualizer::_is_scene_focused() const
    {
        return _glctx.is_window_focused();
    }

    std::optional<vec2_f32> visualizer::_try_convert_screen_pos_2_viewport_pos(const vec2_f32 screen_pos) const
    {
        // the whole window is the scene viewport here
        const vec2_i32 window_size = _glctx.get_window_size();
        const bool cursor_in_window =
            _glctx.is_window_focused() &&
            screen_pos.x() >= 0.0f && screen_pos.x() < static_cast<float>(window_size.x()) &&
            screen_pos.y() >= 0.0f && screen_pos.y() < static_cast<float>(window_size.y());

        if (!cursor_in_window) {
            return std::nullopt;
        }

        // window screen coordinates to framebuffer pixels (they differ on scaled displays)
        const vec2_i32 framebuffer_size = _glctx.get_framebuffer_size();
        const vec2_f32 framebuffer_pos{
            screen_pos.x() * static_cast<float>(framebuffer_size.x()) / static_cast<float>(window_size.x()),
            screen_pos.y() * static_cast<float>(framebuffer_size.y()) / static_cast<float>(window_size.y())
        };

        return win32_screen_pos_2_gl_viewport_pos(
            framebuffer_pos,
            framebuffer_size,
            view_port{ vec2_i32{ 0, 0 }, framebuffer_size }
        );
    }

    void visualizer::_handle_close_event(
        bool& cancel)
    {
        if (_cb_close) {
            _cb_close(cancel);
        }
    }

    void visualizer::_handle_frame_resize_event([[maybe_unused]] const vec2_i32 new_frame_size)
    {
        // ...
    }

    void visualizer::_handle_key_event(
        const int32_t key,
        [[maybe_unused]] const int32_t scancode,
        [[maybe_unused]] const int32_t action,
        [[maybe_unused]] const int32_t mods)
    {
        if (!this->_accepts_keyboard_input()) {
            return;
        }

        if (_cb_key) {
            bool handled = false;
            _cb_key(key, scancode, action, mods, handled);
            if (handled) { return; }
        }

        // https://www.glfw.org/docs/latest/group__keys.html
        if (action == GLFW_RELEASE) { return; }

        switch (key) {
        case GLFW_KEY_HOME:
            //TODO: reset camera state
            break;
        }
    }

    void visualizer::_handle_mouse_button_event(
        const int32_t button,
        [[maybe_unused]] const int32_t action,
        [[maybe_unused]] const int32_t mods)
    {
        const vec2_f32 curr_cursor_screen_pos = _glctx.get_cursor_screen_pos();
        if (!this->_accepts_mouse_input(curr_cursor_screen_pos)) {
            return;
        }

        if (_cb_mouse_button) {
            bool handled = false;
            _cb_mouse_button(button, action, mods, handled);
            if (handled) { return; }
        }
    }

    void visualizer::_handle_mouse_move_event(const vec2_f64 cursor_pos)
    {
        const vec2_f32 cursor_screen_pos = cursor_pos.cast<float>();
        if (!this->_accepts_mouse_input(cursor_screen_pos)) {
            return;
        }

        if (_cb_mouse_move) {
            bool handled = false;
            _cb_mouse_move(cursor_pos, handled);
            if (handled) { return; }
        }

        // Drop any drag in progress, so that re-enabling does not jump the camera.
        if (!_flag_camera_interaction) {
            _begin_click_cursor_screen_pos.reset();
            return;
        }

        // If the cursor is not hovered over the scene viewport, skip scene interaction control.
        if (!this->_is_cursor_in_scene_viewport(cursor_screen_pos)) {
            return;
        }

        //
        // Scene interaction control start
        //

        const bool
            fl_l_mouse_pressed = GLFW_PRESS == ::glfwGetMouseButton(_glctx.get_glfw_window(), GLFW_MOUSE_BUTTON_LEFT),
            fl_r_mouse_pressed = GLFW_PRESS == ::glfwGetMouseButton(_glctx.get_glfw_window(), GLFW_MOUSE_BUTTON_RIGHT),
            fl_m_mouse_pressed = GLFW_PRESS == ::glfwGetMouseButton(_glctx.get_glfw_window(), GLFW_MOUSE_BUTTON_MIDDLE);

        if (fl_l_mouse_pressed || fl_r_mouse_pressed || fl_m_mouse_pressed)
        {
            const vec2_f32 move_offset{
                cursor_screen_pos.x() - _begin_click_cursor_screen_pos.value_or(cursor_screen_pos).x(),
                _begin_click_cursor_screen_pos.value_or(cursor_screen_pos).y() - cursor_screen_pos.y() // reversed since y-coordinates go from bottom to top
            };

            abstract_camera* const scn_camera = this->get_current_scene()->get_camera();

            if (fl_l_mouse_pressed)
            {
                scn_camera->process_mouse_rotation(move_offset);
            }
            else if (fl_m_mouse_pressed)
            {
                const vec2_f32 start_viewport_pos = this->_try_convert_screen_pos_2_viewport_pos(
                    _begin_click_cursor_screen_pos.value_or(cursor_screen_pos)
                ).value();

                const vec2_f32 end_viewport_pos = this->_try_convert_screen_pos_2_viewport_pos(
                    cursor_screen_pos
                ).value();

                scn_camera->process_mouse_translation(
                    start_viewport_pos,
                    end_viewport_pos
                );
            }

            _begin_click_cursor_screen_pos = cursor_screen_pos;
        }
        else
        {
            if (_begin_click_cursor_screen_pos) {
                _begin_click_cursor_screen_pos.reset();
            }
        }
    }

    void visualizer::_handle_mouse_scroll_event(const vec2_f64 scroll_offset)
    {
        const vec2_f32 curr_cursor_screen_pos = _glctx.get_cursor_screen_pos();
        if (!this->_accepts_mouse_input(curr_cursor_screen_pos)) {
            return;
        }

        if (_cb_mouse_scroll) {
            bool handled = false;
            _cb_mouse_scroll(scroll_offset, handled);
            if (handled) { return; }
        }

        if (_flag_camera_interaction && this->_is_cursor_in_scene_viewport(curr_cursor_screen_pos))
        {
            abstract_camera* const scn_camera = this->get_current_scene()->get_camera();

            const bool shift_pressed =
                GLFW_PRESS == _glctx.get_key_state(GLFW_KEY_LEFT_SHIFT) ||
                GLFW_PRESS == _glctx.get_key_state(GLFW_KEY_RIGHT_SHIFT);

            const float zoom_offset = static_cast<float>(scroll_offset.y());
            if (!shift_pressed) {
                scn_camera->process_mouse_zoom(zoom_offset);
            } else {
                scn_camera->process_mouse_perspective_zoom(zoom_offset);
            }
        }
    }

    void visualizer::_handle_dpi_change_event(const vec2_f32 dpi_scale)
    {
        if (_cb_dpi_change) {
            _cb_dpi_change(dpi_scale);
        }
    }

} // namespace
