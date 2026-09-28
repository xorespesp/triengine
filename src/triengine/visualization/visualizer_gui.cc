#include "visualizer_gui.hh"
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>

namespace triengine::visualization
{
    bool visualizer_gui::is_main_menu_enabled() const
    {
        if (!_gui_mgr) { TRIENGINE_PANIC("GUI access requires a created window"); }
        return _gui_mgr->is_main_menu_enabled();
    }

    void visualizer_gui::enable_main_menu(bool enable)
    {
        if (!_gui_mgr) { TRIENGINE_PANIC("GUI access requires a created window"); }
        _gui_mgr->enable_main_menu(enable);
    }

    void visualizer_gui::add_gui_window(
        std::shared_ptr<gui::iwindow> window,
        gui::dock_slot slot)
    {
        if (!_gui_mgr) { TRIENGINE_PANIC("GUI access requires a created window"); }
        _gui_mgr->add_window(std::move(window), slot);
    }

    void visualizer_gui::set_dock_split_ratios(const gui::dock_split_ratios& ratios)
    {
        if (!_gui_mgr) { TRIENGINE_PANIC("GUI access requires a created window"); }
        _gui_mgr->set_dock_split_ratios(ratios);
    }

    void visualizer_gui::_on_window_created()
    {
        _gui_mgr = std::make_unique<gui::gui_manager>();
        _gui_mgr->initialize(
            this,
            this->_get_gl_context(),
            this->get_window_dpi_scale().x()
        );

        _scene_window = _gui_mgr->get_scene_window();
    }

    void visualizer_gui::_on_dpi_changed(const vec2_f32 dpi_scale)
    {
        if (_gui_mgr) { _gui_mgr->change_dpi_scale(dpi_scale.x()); }
    }

    void visualizer_gui::_on_window_destroying()
    {
        _gui_mgr->deinitialize();
        _gui_mgr.reset();

        // release the scene framebuffer while the GL context is still current
        _scene_window.reset();
    }

    visualizer_gui::scene_render_target visualizer_gui::_begin_scene_frame()
    {
        // (re)allocates the framebuffer to the current scene window size
        _scene_window->bind_framebuffer();

        const core::frame_buffer& scene_fb = _scene_window->get_framebuffer();
        return scene_render_target{
            scene_fb.fbo_id(),
            vec2_i32{ scene_fb.width_pixels(), scene_fb.height_pixels() }
        };
    }

    void visualizer_gui::_end_scene_frame()
    {
        _scene_window->unbind_framebuffer();

        // The gui composites onto the backbuffer instead of covering it (translucent windows, pass-through central node), 
        // so clear it to keep the previous frame from showing through.
        GLCall(::glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
        GLCall(::glClear(GL_COLOR_BUFFER_BIT));

        // Render GUI (the scene framebuffer is drawn inside the scene window)
        _gui_mgr->render();
    }

    bool visualizer_gui::_is_scene_focused() const
    {
        return _scene_window->is_window_focused();
    }

    std::optional<vec2_f32> visualizer_gui::_try_convert_screen_pos_2_viewport_pos(const vec2_f32 screen_pos) const
    {
        return _scene_window->try_convert_screen_pos_2_viewport_pos(screen_pos);
    }

    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.

    bool visualizer_gui::_accepts_keyboard_input() const
    {
        return !(ImGui::GetIO().WantCaptureKeyboard && !_scene_window->is_window_focused());
    }

    bool visualizer_gui::_accepts_mouse_input(const vec2_f32 cursor_screen_pos) const
    {
        return !(ImGui::GetIO().WantCaptureMouse && !_scene_window->check_cursor_in_scene_viewport(cursor_screen_pos));
    }

} // namespace
