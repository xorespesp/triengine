#pragma once
#include <triengine/global_options.hh>
#include <triengine/visualization/visualizer_gui.hh>
#include <triengine/utility/noncopyable.hh>

#include <memory>

namespace demo
{
    class scene_wrapper
        : triengine::utility::noncopyable
    {
        std::shared_ptr<triengine::scene> _scene;

    public:
        scene_wrapper(std::shared_ptr<triengine::scene> scene)
            : _scene{ std::move(scene) }
        { }

        virtual ~scene_wrapper() = default;

        triengine::scene_id_t get_scene_id() const { return _scene->get_id(); }
        std::shared_ptr<const triengine::scene> get_scene() const noexcept { return _scene; }
        std::shared_ptr<triengine::scene> get_scene() noexcept { return _scene; }

        virtual void update_animation() = 0;
        virtual void render_gui(const triengine::gui::window_render_context& render_ctx) = 0;
    };

} // namespace