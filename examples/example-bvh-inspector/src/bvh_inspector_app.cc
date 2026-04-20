#pragma once
#include "bvh_inspector_app.hh"

namespace demo
{
    bvh_inspector_app::bvh_inspector_app()
    {}

    bvh_inspector_app::~bvh_inspector_app()
    {}

    void bvh_inspector_app::create()
    {
        CXLIB_TRACE("{}() ENTER", __func__);

        _vis = std::make_unique<triengine::visualization::visualizer>();
        _vis->create_window("Triengine Demo - BVH Inspector"
            " (Build: " __DATE__ ", " __TIME__
#if defined (_DEBUG)
            " DBG"
#else  // ^^^ _DEBUG ^^^ / vvv !_DEBUG vvv
            " REL"
#endif // ^^^ !_DEBUG ^^^
            ")"
        );

        _log_window = std::make_shared<triengine::gui::log_window>();
        _log_window->set_visible(false);
        _vis->add_gui_window(_log_window);

        _render_stats_window = std::make_shared<triengine::gui::render_stats_window>();
        _render_stats_window->set_visible(false);
        _vis->add_gui_window(_render_stats_window);

        _scene_ctrl_window = std::make_shared<triengine::gui::scene_control_window>(_vis.get());
        _vis->add_gui_window(_scene_ctrl_window, triengine::gui::dock_slot::left);

        auto scn = _vis->add_scene();
        _inspector_window = std::make_shared<bvh_inspector_window>(scn);
        _vis->add_gui_window(_inspector_window, triengine::gui::dock_slot::left);

        _vis->set_key_callback(
            [this](
                [[maybe_unused]] const int32_t key,
                [[maybe_unused]] const int32_t scancode,
                [[maybe_unused]] const int32_t action,
                [[maybe_unused]] const int32_t mods,
                [[maybe_unused]] bool& handled)
        {
            if (action != GLFW_RELEASE)
            {
                CXLIB_TRACE("key: {}", key);

                switch (key) {
                case GLFW_KEY_ESCAPE:
                    break;
                case GLFW_KEY_F12:
                    _vis->enable_main_menu(!_vis->is_main_menu_enabled());
                    break;
                case GLFW_KEY_SPACE:
                    if (_inspector_window->is_playing()) {
                        _inspector_window->pause();
                    } else {
                        _inspector_window->play();
                    }
                    break;
                case GLFW_KEY_LEFT:
                    _inspector_window->move_to_prev_frame();
                    break;
                case GLFW_KEY_RIGHT:
                    _inspector_window->move_to_next_frame();
                    break;
                }
            }
        });

    }

    void bvh_inspector_app::destroy()
    {
        CXLIB_TRACE("{}() ENTER", __func__);

        _log_window.reset();
        _render_stats_window.reset();
        _scene_ctrl_window.reset();
        _inspector_window.reset();

        _vis->destroy_window();
        _vis.reset();

        CXLIB_TRACE("{}() LEAVE", __func__);
    }

    void bvh_inspector_app::run()
    {
        CXLIB_TRACE("{}() ENTER", __func__);

        CXLIB_DEBUG("polling start..");
        while (_vis->update_window())
        {
            _vis->render();
            _inspector_window->update_animation();
        } // while

        CXLIB_TRACE("{}() LEAVE", __func__);
    }

} // namespace