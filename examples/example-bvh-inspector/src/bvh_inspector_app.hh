#pragma once
#include <cxlib/utils/logger.hh>
#include <cxlib/utils/path_utils.hh>

#include <triengine/visualization/visualizer.hh>
#include <triengine/gui/windows/scene_ctrl_window.hh>

#include "bvh_inspector_window.hh"

#include <memory>

namespace demo
{
    class bvh_inspector_app
    {
    public:
        bvh_inspector_app();
        ~bvh_inspector_app();

        void create();
        void destroy();
        void run();

    private:
        std::unique_ptr<triengine::visualization::visualizer> _vis;
        std::shared_ptr<triengine::gui::log_window> _log_window;
        std::shared_ptr<triengine::gui::render_stats_window> _render_stats_window;
        std::shared_ptr<triengine::gui::scene_control_window> _scene_ctrl_window;
        std::shared_ptr<bvh_inspector_window> _inspector_window;
    }; // class

} // namespace