#pragma once
#include <triengine/global_options.hh>
#include <triengine/visualization/visualizer.hh>
#include <triengine/utility/noncopyable.hh>

#include <filesystem>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

namespace gui
{
    class scene_control_window;
    
    class scene_wrapper
        : triengine::utility::noncopyable
    {
        std::shared_ptr<triengine::scene> _scene;

    public:
        scene_wrapper(std::shared_ptr<triengine::scene> scene)
            : _scene{ std::move(scene) }
        {}

        virtual ~scene_wrapper() = default;

        triengine::scene_id_t get_scene_id() const { return _scene->get_id(); }
        std::shared_ptr<const triengine::scene> get_scene() const noexcept { return _scene; }
        std::shared_ptr<triengine::scene> get_scene() noexcept { return _scene; }

        virtual void update_animation() = 0;
        virtual void render_gui(const triengine::gui::window_render_context& render_ctx) = 0;
    };

    class basic_demo_app
    {
    public:
        basic_demo_app();
        ~basic_demo_app();

        void create();
        void destroy();
        void run();

    private:
        void _add_main_scene();
        void _add_engine_scene();
        void _add_pointcloud_scene();
        void _add_bvh_scene();
        
    private:
        std::unique_ptr<triengine::visualization::visualizer> _vis;
        std::shared_ptr<triengine::gui::log_window> _log_window;
        std::shared_ptr<triengine::gui::render_stats_window> _render_stats_window;
        std::shared_ptr<scene_control_window> _scene_ctrl_window;
        bool _flag_animation{ true };
    }; // class

} // namespace