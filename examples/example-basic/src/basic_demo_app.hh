#pragma once
#include <utils/logger.hh>
#include <triengine/visualization/visualizer.hh>
#include <triengine/frame_buffer.hh>

#include <memory>
#include <array>
#include <magic_enum/magic_enum.hpp>

namespace gui
{
    class basic_demo_app
    {
    public:
        basic_demo_app() = default;
        ~basic_demo_app() = default;

        void create(
            const std::filesystem::path& triengine_resource_dir
        );

        void destroy();

        void run();

    private:
        void _render();

    private:
        std::unique_ptr<triengine::visualization::visualizer> _vis;
        std::shared_ptr<triengine::scene> _scene;

        std::shared_ptr<triengine::gui::log_window> _log_window;
        std::shared_ptr<triengine::gui::render_stats_window> _render_stats_window;

        std::shared_ptr<triengine::geometry::triangle_mesh_object> _obj_texcolor_mesh;
        std::shared_ptr<triengine::geometry::triangle_mesh_object> _obj_texcolor_mesh2;

    }; // class

} // namespace