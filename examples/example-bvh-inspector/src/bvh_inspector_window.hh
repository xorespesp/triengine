#pragma once
#include <triengine/math/math3d.hh>
#include <triengine/io/file_bvh_loader.hh>
#include <triengine/geometry/skeleton_object.hh>
#include <triengine/gui/widgets/file_browser_widget.hh>
#include <triengine/gui/iwindow.hh>
#include <triengine/scene.hh>

#include <unordered_map>
#include <optional>
#include <memory>
#include <chrono>

namespace demo
{
    class bvh_inspector_window
        : public triengine::gui::iwindow
    {
        enum class playback_state_type {
            paused,
            playing,
        };

        struct ui_state_t {
            std::unique_ptr<triengine::io::bvh_file_t> bvh_data;
            std::optional<triengine::mat4_f32> offset_transform;
            std::unordered_map<
                triengine::io::bvh_joint_id_t/* parent */, 
                std::vector<triengine::io::bvh_joint_id_t>/* childs */
            > hierarchy_map_cache;

            triengine::color3_f32 joint_color{ 0.196f, 0.196f, 0.196f };
            triengine::color3_f32 selected_joint_color{ 0.980f, 0.196f, 0.196f };
            triengine::color3_f32 bone_color{ 0.098f, 0.098f, 0.098f };
            float joint_radius{ 0.020f };
            float bone_parent_cap_radius_ratio{ 0.001f };
            float bone_child_cap_radius_ratio{ 0.010f };
            float bone_middle_radius_ratio{ 0.080f };
            float max_bone_middle_radius{ 0.018f };
            float bone_height_ratio_parent{ 0.10f };
            int bone_resolution{ 4 };

            int32_t current_frame_index{ 0 };
            playback_state_type playback_state{ playback_state_type::paused };
            float playback_speed{ 1.0f }; // speed factor
            std::optional<triengine::io::bvh_joint_id_t> selected_joint_id;
            bool fl_rebuild_skeleton{ true }; // hierarchy/appearance changed -> recreate skeleton
            bool fl_pose_dirty{ false }; // only the pose changed -> in-place skeleton pose update

            ui_state_t() = default;
        };

        ui_state_t _state;
        triengine::gui::widgets::file_browser_widget _file_browser;

        std::shared_ptr<triengine::scene> _scene;
        std::shared_ptr<triengine::geometry::mesh_object> _origin_axis;
        std::shared_ptr<triengine::geometry::skeleton_object> _bvh_skeleton;

    public:
        bvh_inspector_window(std::shared_ptr<triengine::scene> target_scene);
        virtual ~bvh_inspector_window() = default;

        const char* get_window_name() const override;
        ImVec2 get_initial_window_size() const override;

        triengine::scene* get_scene() noexcept;
        const triengine::scene* get_scene() const noexcept;

        bool is_loaded() const noexcept;
        bool is_playing() const noexcept;

        void play() noexcept;
        void pause() noexcept;

        void move_to_next_frame() noexcept;
        void move_to_prev_frame() noexcept;

        void update_animation();
        void render(const triengine::gui::window_render_context& render_ctx) override;

    private:
        void _render_hierarchy_tree(triengine::io::bvh_joint_id_t bvh_jid);

        // Builds a fresh skeleton object whose hierarchy and appearance come from the
        // current ui state; `ref_frame` provides the initial pose and bone lengths.
        std::shared_ptr<triengine::geometry::skeleton_object> _build_skeleton_object(
            const triengine::io::bvh_file_t& bvh_file,
            const triengine::io::bvh_motion_frame_t& ref_frame
        ) const;

        // Converts a bvh motion frame into a skeleton pose (one entry per joint).
        triengine::geometry::skeleton_pose_t _make_skeleton_pose(
            const triengine::io::bvh_motion_frame_t& bvh_frame
        ) const;

    }; // class

} // namespace