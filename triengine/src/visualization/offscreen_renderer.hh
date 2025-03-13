#pragma once
#include "../common.h"
#include "../gl_context.hh"
#include "../scene_renderer.hh"
#include "../image.hh"

#include <functional>
#include <unordered_map>
#include <array>

namespace triengine::visualization
{
    class offscreen_renderer
    {
    public:
        offscreen_renderer();
        virtual ~offscreen_renderer() = default;

        offscreen_renderer(const offscreen_renderer&) = delete;
        offscreen_renderer& operator=(const offscreen_renderer&) = delete;

        const gl_context* get_gl_context() const noexcept { return &_glctx; }
        gl_context* get_gl_context() noexcept { return &_glctx; }

        void create_renderer(
            int32_t width,
            int32_t height
        );

        void destroy_renderer();

        std::shared_ptr<const scene> get_current_scene() const { return _curr_scn; }
        std::shared_ptr<scene> get_current_scene() { return _curr_scn; }

        bool add_scene(std::shared_ptr<scene> scn = std::make_shared<triengine::scene>());
        void remove_scene(std::shared_ptr<scene> scn);
        void change_scene(std::shared_ptr<scene> scn);

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

        frame_buffer _fb_main;

    }; // class

} // namespace