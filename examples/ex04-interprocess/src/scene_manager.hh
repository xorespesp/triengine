#pragma once
#include "scene_wrapper.hh"

#include <triengine/visualization/offscreen_renderer_dx.hh>
#include <triengine/utility/noncopyable.hh>

#include <memory>
#include <vector>

// Owns the demo's scenes and tracks the active one, keeping the renderer's current scene in
// sync. Adding a scene is the only extension point: new scene types just derive from
// scene_wrapper and get registered via add(). Modeled on ex01-basic's scene controller,
// minus the ImGui window.
//
// Not thread-safe: the owner (renderer_process::impl) serializes all access under its lock.
class scene_manager
    : triengine::utility::noncopyable
{
public:
    explicit scene_manager(triengine::visualization::offscreen_renderer_dx& renderer)
        : _renderer{ renderer }
    { }

    // Register a scene. The first one added becomes the active scene.
    void add(std::unique_ptr<scene_wrapper> scene)
    {
        const bool is_first = _scenes.empty();
        _scenes.push_back(std::move(scene));
        if (is_first) {
            _current = 0;
            _renderer.switch_scene(_scenes[_current]->get_scene_id());
        }
    }

    bool empty() const noexcept { return _scenes.empty(); }

    scene_wrapper* current() noexcept {
        return _scenes.empty() ? nullptr : _scenes[_current].get();
    }
    const scene_wrapper* current() const noexcept {
        return _scenes.empty() ? nullptr : _scenes[_current].get();
    }

    void switch_to_prev()
    {
        if (_scenes.empty()) { return; }
        _current = (_current == 0) ? _scenes.size() - 1 : _current - 1;
        _renderer.switch_scene(_scenes[_current]->get_scene_id());
    }

    void switch_to_next()
    {
        if (_scenes.empty()) { return; }
        _current = (_current + 1) % _scenes.size();
        _renderer.switch_scene(_scenes[_current]->get_scene_id());
    }

private:
    triengine::visualization::offscreen_renderer_dx& _renderer;
    std::vector<std::unique_ptr<scene_wrapper>> _scenes;
    size_t _current{ 0 };

}; // class
