#pragma once
#include <triengine/common.h>
#include <triengine/image_buffer.hh>
#include <triengine/core/gl_context.hh>
#include <triengine/core/scene_renderer.hh>
#include <triengine/utility/noncopyable.hh>

#include <functional>
#include <unordered_map>
#include <array>

namespace triengine::visualization
{
    class offscreen_renderer
        : utility::noncopyable
    {
    public:
        offscreen_renderer();
        virtual ~offscreen_renderer() = default;

        const core::gl_context* get_gl_context() const noexcept { return &_glctx; }
        core::gl_context* get_gl_context() noexcept { return &_glctx; }

        void create_renderer(vec2_i32 initial_frame_size);

        void destroy_renderer();

        std::shared_ptr<scene> add_scene();
        void remove_scene(scene_id_t scn_id);
        
        void switch_scene(scene_id_t scn_id);
        void switch_to_previous_scene();
        void switch_to_next_scene();

        std::shared_ptr<const scene> find_scene(scene_id_t scn_id) const;
        std::shared_ptr<scene> find_scene(scene_id_t scn_id);

        std::shared_ptr<const scene> get_current_scene() const;
        std::shared_ptr<scene> get_current_scene();

        vec2_i32 get_frame_size() const noexcept;
        void resize_frame(vec2_i32 new_frame_size);

        void render(
            image_buffer& frame_image/* out */,
            image_format_type frame_image_format = image_format_type::bgra
        );

    private:
        void _begin_frame();
        void _end_frame();
        
    private:
        bool _flag_initialized{ false };
        bool _flag_invalidate_fbo{ true };

        core::gl_context _glctx;
        vec2_i32 _curr_frame_size{};

        core::scene_renderer _scn_renderer;
        
        std::list<std::shared_ptr<scene>> _scn_list;
        std::list<std::shared_ptr<scene>>::iterator _curr_scn_it{ _scn_list.end() };
        std::unordered_map<
            scene_id_t, 
            std::list<std::shared_ptr<scene>>::iterator
        > _scn_id_map;

        core::frame_buffer _fb_main;

        // frame time calculation
        double _frame_time_delta{ 0.0 }, _last_frame_time{ 0.0 };

    }; // class

} // namespace