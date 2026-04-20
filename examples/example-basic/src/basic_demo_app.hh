#pragma once
#include <cxlib/utils/logger.hh>
#include <cxlib/utils/path_utils.hh>

#include "scene_wrapper.hh"

#include <triengine/gui/windows/scene_ctrl_window.hh>
#include <filesystem>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

namespace demo
{
    class demo_scene_control_window;

    class basic_demo_app
    {
    public:
        basic_demo_app();
        ~basic_demo_app();

        void create();
        void destroy();
        void run();

    private:
        std::unique_ptr<triengine::visualization::visualizer> _vis;
        std::shared_ptr<triengine::gui::log_window> _log_window;
        std::shared_ptr<triengine::gui::render_stats_window> _render_stats_window;
        std::shared_ptr<triengine::gui::scene_control_window> _scene_ctrl_window;
        std::shared_ptr<demo_scene_control_window> _demo_scene_ctrl_window;
        bool _flag_animation{ true };
    }; // class

} // namespace