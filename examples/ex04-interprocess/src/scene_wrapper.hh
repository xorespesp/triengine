#pragma once
#include <triengine/scene.hh>
#include <triengine/utility/noncopyable.hh>

#include <memory>

// A self-contained scene: it builds its own triengine::scene in the constructor and owns
// every geometry/resource that belongs to it. The scene_manager keeps a list of these and
// drives the active one.
//
// Threading: the update()/post_render() hooks run on the render thread (around render()).
// Construction/destruction also happen on the render thread.
class scene_wrapper
    : triengine::utility::noncopyable
{
public:
    explicit scene_wrapper(std::shared_ptr<triengine::scene> scene)
        : _scene{ std::move(scene) }
    { }

    virtual ~scene_wrapper() = default;

    triengine::scene_id_t get_scene_id() const { return _scene->get_id(); }
    std::shared_ptr<const triengine::scene> get_scene() const noexcept { return _scene; }
    std::shared_ptr<triengine::scene> get_scene() noexcept { return _scene; }

    // Per-frame update, called on the render thread before render() while this scene is the
    // active one. `frame_size` is the renderer's current frame size.
    virtual void update([[maybe_unused]] triengine::vec2_i32 frame_size) {}

    // Called on the render thread after render() returns (e.g. to recycle a buffer whose
    // upload the render just consumed). Default: nothing to do.
    virtual void post_render() {}

private:
    std::shared_ptr<triengine::scene> _scene;

}; // class
