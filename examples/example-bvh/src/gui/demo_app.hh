#pragma once
#include <utils/logger.hh>
#include <triengine/visualizer_window.hh>
#include <triengine/frame_buffer.hh>

#include <memory>
#include <array>
#include <magic_enum.hpp>

namespace gui
{
    class demo_app
    {
    private:
        std::unique_ptr<triengine::visualizer_window> _off_window;
        std::shared_ptr<triengine::scene> _scene;

        std::shared_ptr<triengine::gui::log_window> _log_window;
        std::shared_ptr<triengine::gui::render_stats_window> _render_stats_window;

        std::shared_ptr<triengine::geometry::triangle_mesh_object> _obj_texcolor_mesh;

    public:
        demo_app() = default;
        ~demo_app() = default;

        void set_close_callback(
            triengine::visualizer_window::close_callback cb
        );

        void set_key_callback(
            triengine::visualizer_window::key_callback cb
        );

        void create(
            const std::filesystem::path& triengine_resource_dir
        );

        void destroy();

        void run();

    private:
        void _render();

    }; // class

} // namespace