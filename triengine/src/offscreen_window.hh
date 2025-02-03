#pragma once
#include "common.h"
#include "gl_context.hh"
#include "scene_renderer.hh"
#include "image.hh"

#include <functional>
#include <unordered_map>
#include <array>

namespace triengine
{
    class offscreen_window
    {
    public:
        offscreen_window();
        virtual ~offscreen_window() = default;

        offscreen_window(const offscreen_window&) = delete;
        offscreen_window& operator=(const offscreen_window&) = delete;

        const gl_context* get_gl_context() const noexcept { return &_glctx; }
        gl_context* get_gl_context() noexcept { return &_glctx; }

        std::shared_ptr<const scene> get_current_scene() const { return _curr_scn; }
        std::shared_ptr<scene> get_current_scene() { return _curr_scn; }

        void create_window(
            int32_t width,
            int32_t height,
            bool multisample = true
        );

        void destroy_window();

        bool update_window();

        std::shared_ptr<scene> create_new_scene() {
            auto new_scn = std::make_shared<scene>();
            if (_scn_map.empty()) { _curr_scn = new_scn; }
            _scn_map.insert({ new_scn->id(), new_scn });
            return new_scn;
        }

        void remove_scene(std::shared_ptr<scene> scn) {
            if (scn) {
                auto it = _scn_map.find(scn->id());
                if (it != _scn_map.end()) {
                    _scn_map.erase(it);
                    if (_curr_scn->id() == scn->id()) {
                        _curr_scn = _scn_map.empty() ? nullptr : _scn_map.begin()->second;
                    }
                }
            }
        }

        void change_scene(std::shared_ptr<scene> scn) {
            _curr_scn = scn;
        }

        void render(
            image& frame_image/* out */
        );

    private:
        void _begin_frame();
        void _end_frame();
        
    private:
        bool _flag_initialized{ false };
        bool _flag_invalidate_fbo{ true };

        gl_context _glctx;
        int32_t _curr_window_width{};
        int32_t _curr_window_height{};

        scene_renderer _scn_renderer;
        std::unordered_map<uint32_t/* scene id */, std::shared_ptr<scene>> _scn_map;
        std::shared_ptr<scene> _curr_scn;

        int32_t _fb_sample_count{ 1 };
        frame_buffer _fb_main;
        frame_buffer _fb_msaa_copy; // only used in msaa rendering

    }; // class

} // namespace