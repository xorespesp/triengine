#pragma once
#include "../scene_wrapper.hh"

#include <triengine/math/math3d.hh>
#include <triengine/io/file_bvh_loader.hh>
#include <triengine/geometry/skeleton_object.hh>
#include <triengine/gui/widgets/file_browser_widget.hh>
#include <triengine/utility/bit_cast.hh>

#include <cxlib/utils/debug_panic.hh>
#include <cxlib/utils/debug_assert.hh>

#include <chrono>

namespace demo::scene
{
    using namespace triengine;

    class bvh_scene
        : public scene_wrapper
    {
        enum class playback_state_type
        {
            paused,
            playing,
        };

        struct ui_state_t
        {
            std::unique_ptr<io::bvh_file_t> bvh_data;
            std::optional<mat4_f32> offset_transform;
            std::unordered_map<io::bvh_joint_id_t/* parent */, std::vector<io::bvh_joint_id_t>/* childs */> hierarchy_map_cache;
            
            triengine::color3_f32 joint_color{ 0.196f, 0.196f, 0.196f };
            triengine::color3_f32 bone_color{ 0.098f, 0.098f, 0.098f };
            float joint_radius{ 0.020f };
            float bone_parent_cap_radius_ratio{ 0.001f };
            float bone_child_cap_radius_ratio{ 0.010f };
            float bone_middle_radius_ratio{ 0.080f };
            float max_bone_middle_radius{ 0.018f };
            float bone_height_ratio_parent{ 0.10f };
            int bone_resolution{ 4 };

            int current_frame_index{ 0 };
            playback_state_type playback_state{ playback_state_type::paused };
            float playback_speed{ 1.0f }; // speed factor
            std::optional<io::bvh_joint_id_t> selected_joint_id;
            bool fl_update_scene{ true };

            ui_state_t() = default;
        };

        ui_state_t _state;
        gui::widgets::file_browser_widget _file_browser;

        std::shared_ptr<geometry::mesh_object> _origin_axis;
        std::shared_ptr<geometry::skeleton_object> _bvh_skeleton;

    public:
        bvh_scene(
            visualization::visualizer& vis)
            : scene_wrapper(vis.add_scene())
        {
            const auto rsrc_dir_path = global_options::instance()->get_resource_directory();
            auto scn = this->get_scene();
            scn->set_name("bvh playback");

            scn->get_render_config()->bg_color = triengine::color4_f32::all(1.0f);
            scn->get_render_config()->show_origin_xz_grid = true;
            scn->get_render_config()->light_opts.point_light.position = vec3_f32{ 0.0f, 1.5f, -1.5f };
            scn->get_render_config()->light_opts.point_light.ambient_intensity = 0.0f;
            scn->get_render_config()->light_opts.point_light.diffuse_intensity = 2.8f;
            scn->get_render_config()->light_opts.point_light.specular_intensity = 1.5f;
            scn->get_render_config()->light_opts.dir_light.ambient_intensity = 0.2f;
            scn->get_render_config()->light_opts.dir_light.diffuse_intensity = 0.2f;
            scn->get_render_config()->light_opts.dir_light.specular_intensity = 0.5f;
            scn->get_render_config()->light_opts.bloom.strength = 0.05f;
            scn->get_render_config()->light_opts.hdr.exposure = 0.3f;
            scn->get_render_config()->inf_plane_opts.max_view_distance = 35.0f;

            {
                scn->get_render_config()->show_origin_xz_grid = true;
                scn->get_render_config()->inf_plane_opts.max_view_distance = 80.0f;
                scn->get_render_config()->inf_plane_opts.grid_cell_size = 0.5f;

                box_filtered_grid_plane_option_t inf_plane_opt{};
                inf_plane_opt.grid_line_color = math::vec3_all(80.0f / 255.0f);
                inf_plane_opt.grid_cell_color = math::vec3_all(40.0f / 255.0f);
                scn->get_render_config()->inf_plane_opts.plane_option = inf_plane_opt;
            }

            _origin_axis = geometry::mesh_object::create_coordinate_frame(0.5f);
            scn->add_geometry(_origin_axis);

            _file_browser.register_file_filters({ "*.bvh", "*.*" });
            _file_browser.set_active_file_filter(0);
            _file_browser.set_cwd(rsrc_dir_path / "bvh");
        }

        void update_animation() override
        {
            // Playback handling
            if (_state.bvh_data &&
                _state.playback_state == playback_state_type::playing)
            {
                const io::bvh_file_t& bvh_data = *_state.bvh_data;

                using clock_type = std::chrono::high_resolution_clock;
                using duration_type = std::chrono::microseconds;

                thread_local clock_type::time_point tp_next_update{};

                const float default_fps = static_cast<float>(1.0 / bvh_data.frame_time);
                const float scaled_fps = std::max(1.0f, default_fps * _state.playback_speed);

                const auto update_step = std::chrono::milliseconds{ static_cast<int64_t>(1000.0f / scaled_fps) };

                if (const auto tp_delta = clock_type::now() - tp_next_update;
                    tp_delta >= update_step)
                {
                    tp_next_update += (tp_delta + update_step);

                    // move to next frame
                    _state.current_frame_index = (_state.current_frame_index + 1) % bvh_data.frames.size();
                    _state.fl_update_scene = true;
                }
            }

            // Update scene
            if (_state.fl_update_scene)
            {
                if (_state.bvh_data)
                {
                    if (_bvh_skeleton) {
                        this->get_scene()->remove_geometry(_bvh_skeleton);
                    }

                    const io::bvh_file_t& bvh_data = *_state.bvh_data;
                    _bvh_skeleton = this->_create_skeleton_object_from_bvh(
                        bvh_data,
                        bvh_data.frames[_state.current_frame_index]
                    );

                    if (_state.offset_transform) {
                        _bvh_skeleton->transform(_state.offset_transform.value(), true);
                        _origin_axis->transform(_state.offset_transform.value(), false);
                    }

                    this->get_scene()->add_geometry(_bvh_skeleton);
                }
                else
                {
                    if (_bvh_skeleton) {
                        this->get_scene()->remove_geometry(_bvh_skeleton);
                    }
                    _bvh_skeleton.reset();
                }

                _state.fl_update_scene = false;
            }
        }

        void render_gui(
            [[maybe_unused]] const gui::window_render_context& render_ctx) override
        {
            if (_state.bvh_data)
            {
                const io::bvh_file_t& bvh_data = *_state.bvh_data;

                ImGui::Text("File: %s", bvh_data.name.c_str());
                ImGui::SameLine();
                if (ImGui::Button("close")) {
                    _state = ui_state_t{};
                    return;
                }

                ImGui::Text("Frame Time: %.4f seconds (%.1f FPS)"
                    , bvh_data.frame_time
                    , (bvh_data.frame_time > 0.0) ? 1.0 / bvh_data.frame_time : 0.0
                );

                ImGui::Separator();

                if (bvh_data.frames.empty()) {
                    ImGui::Text("No motion frames found.");
                    _state.current_frame_index = 0;
                    return;
                }

                if (ImGui::Button("|<")) {
                    _state.current_frame_index = 0;
                    _state.fl_update_scene = true;
                }

                ImGui::SameLine();

                if (_state.playback_state == playback_state_type::playing) {
                    if (ImGui::Button("||")) {
                        _state.playback_state = playback_state_type::paused;
                    }
                } else {
                    if (ImGui::Button("> ")) {
                        _state.playback_state = playback_state_type::playing;
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button(">|")) {
                    _state.current_frame_index = static_cast<int>(_state.bvh_data->frames.size() - 1);
                    _state.fl_update_scene = true;
                }

                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
                if (ImGui::SliderInt("##FrameSlider"
                    , &_state.current_frame_index
                    , 0
                    , static_cast<int>(bvh_data.frames.size()) - 1
                    , "Frame: %d"))
                {
                    _state.playback_state = playback_state_type::paused;
                    _state.fl_update_scene = true;
                }
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::Text("/ %zu frames", bvh_data.frames.size());

                if (ImGui::DragFloat("Playback Speed", &_state.playback_speed, 0.01f, 0.01f, 4.0f, "%.2fx")) {
                    _state.fl_update_scene = true;
                }

                if (ImGui::CollapsingHeader("Skeleton Render Options", ImGuiTreeNodeFlags_None))
                {
                    ImGui::Indent();

                    if (ImGui::ColorEdit3("Joint Color", _state.joint_color.data(), ImGuiColorEditFlags_NoAlpha)) {
                        _state.fl_update_scene = true;
                    }

                    if (ImGui::ColorEdit3("Bone Color", _state.bone_color.data(), ImGuiColorEditFlags_NoAlpha)) {
                        _state.fl_update_scene = true;
                    }

                    if (ImGui::DragFloat("Joint Radius", &_state.joint_radius, 0.001f, 0.001f, 0.1f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        _state.fl_update_scene = true;
                    }

                    if (ImGui::DragFloat("Bone Parent Cap Radius Ratio", &_state.bone_parent_cap_radius_ratio, 0.001f, 0.001f, 0.5f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        _state.fl_update_scene = true;
                    }

                    if (ImGui::DragFloat("Bone Child Cap Radius Ratio", &_state.bone_child_cap_radius_ratio, 0.001f, 0.001f, 0.5f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        _state.fl_update_scene = true;
                    }

                    if (ImGui::DragFloat("Bone Middle Radius Ratio", &_state.bone_middle_radius_ratio, 0.001f, 0.001f, 0.5f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        _state.fl_update_scene = true;
                    }

                    if (ImGui::DragFloat("Max Bone Middle Radius", &_state.max_bone_middle_radius, 0.001f, 0.001f, 0.2f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        _state.fl_update_scene = true;
                    }

                    if (ImGui::DragFloat("Bone Height Ratio", &_state.bone_height_ratio_parent, 0.001f, 0.001f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                        _state.fl_update_scene = true;
                    }

                    if (ImGui::SliderInt("Bone Resolution", &_state.bone_resolution, 3, 40, "%d")) {
                        _state.fl_update_scene = true;
                    }

                    ImGui::Unindent();
                }

                ImGui::Spacing();

                if (ImGui::CollapsingHeader("BVH File Inspector", ImGuiTreeNodeFlags_None))
                {
                    // Left panel
                    const float left_panel_width = std::max(200.0f * render_ctx.dpi_scale, ImGui::GetContentRegionAvail().x * 0.45f);
                    ImGui::BeginChild("JointHierarchyPanel", ImVec2(left_panel_width, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
                    {
                        ImGui::Text("Joint Hierarchy");
                        ImGui::Separator();

                        const float indent_spacing = 8.0f * render_ctx.dpi_scale;
                        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, indent_spacing);
                        this->_render_hierarchy_tree(_state.bvh_data->root_joint_id);
                        ImGui::PopStyleVar();
                    }
                    ImGui::EndChild();

                    ImGui::SameLine();

                    // Right panel
                    ImGui::BeginChild("JointDetailsPanel", ImVec2(0, 0), true);
                    {
                        ImGui::Text("Selected Joint Details");
                        ImGui::Separator();

                        if (!_state.selected_joint_id) {
                            ImGui::Text("Select a joint from the hierarchy tree.");
                            ImGui::EndChild();
                            return;
                        }

                        CXLIB_ASSERT(_state.selected_joint_id.value() < bvh_data.joints.size());

                        const io::bvh_joint_id_t sel_bvh_jid = _state.selected_joint_id.value();
                        const io::bvh_joint_info_t& sel_bvh_jinfo = bvh_data.joints[sel_bvh_jid];
                        const io::bvh_joint_data_t& sel_bvh_jdata = bvh_data.frames[_state.current_frame_index].skeleton.at(sel_bvh_jid);

                        ImGui::Text("Joint Name: %s (#%zu)", sel_bvh_jinfo.name.c_str(), sel_bvh_jid);
                        ImGui::Text("Joint Length: %.6f", sel_bvh_jinfo.length);

                        ImGui::Spacing();
                        ImGui::Text("BVH Euler Angles (%s, deg):", sel_bvh_jinfo.bvh_euler_axis_order.c_str()); {
                            ImGui::Indent();
                            const auto& angles = sel_bvh_jdata.bvh_euler_angels;
                            ImGui::Text("[%.6f, %.6f, %.6f]", angles.x(), angles.y(), angles.z());
                            ImGui::Unindent();
                        }

                        ImGui::Spacing();
                        ImGui::Text("World Position:"); {
                            ImGui::Indent();
                            const auto& pos = sel_bvh_jdata.world_position;
                            ImGui::Text("[%.6f, %.6f, %.6f]", pos.x(), pos.y(), pos.z());
                            ImGui::Unindent();
                        }

                        ImGui::Spacing();
                        ImGui::Text("World Rotation:"); {
                            ImGui::Indent();
                            const auto& R = sel_bvh_jdata.world_rotation;
                            ImGui::Text("[%.6f, %.6f, %.6f]", R(0, 0), R(0, 1), R(0, 2));
                            ImGui::Text("[%.6f, %.6f, %.6f]", R(1, 0), R(1, 1), R(1, 2));
                            ImGui::Text("[%.6f, %.6f, %.6f]", R(2, 0), R(2, 1), R(2, 2));
                            ImGui::Unindent();
                        }
                    }
                    ImGui::EndChild();
                } // collapsing header
            }
            else //if (!_state.bvh_data)
            {
                ImGui::Text("Select BVH file to play...");

                if (_file_browser.show())
                {
                    _state.bvh_data = std::make_unique<io::bvh_file_t>();
                    if (!io::load_skeleton_from_bvh(
                        _file_browser.get_selected_path(),
                        *_state.bvh_data
                    )) {
                        CXLIB_PANIC("failed to load skeletons from bvh file");
                    }

                    // Build(rebuild) hierarchy cache
                    _state.hierarchy_map_cache.clear();
                    for (const auto [child_bvh_jid, parent_bvh_jid] : _state.bvh_data->joints_parent_map) {
                        if (child_bvh_jid == parent_bvh_jid) { continue; }
                        _state.hierarchy_map_cache[parent_bvh_jid].push_back(child_bvh_jid);
                    }

                    //Eigen::Matrix3f R; // Z-Y-X (Yaw-Pitch-Roll) Order
                    //R = Eigen::AngleAxisf(math::deg2rad(180.0f), Eigen::Vector3f::UnitZ())
                    //    * Eigen::AngleAxisf(math::deg2rad(0.0f), Eigen::Vector3f::UnitY())
                    //    * Eigen::AngleAxisf(math::deg2rad(0.0f), Eigen::Vector3f::UnitX());
                    //Eigen::Matrix4f Tr{ Eigen::Matrix4f::Identity() };
                    //Tr.block<3, 3>(0, 0) = R;
                    //_state.offset_transform = Tr;

                    _state.fl_update_scene = true;
                }
            }
        }

    private:
        void _render_hierarchy_tree(
            const io::bvh_joint_id_t bvh_jid)
        {
            CXLIB_ASSERT(bvh_jid < _state.bvh_data->joints.size());

            const bool has_children = _state.hierarchy_map_cache.count(bvh_jid) && _state.hierarchy_map_cache.at(bvh_jid).size() > 0;
            const auto& bvh_jinfo = _state.bvh_data->joints[bvh_jid];

            ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (_state.selected_joint_id == bvh_jid) { tree_node_flags |= ImGuiTreeNodeFlags_Selected; }
            if (!has_children) { tree_node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; }

            bool open_tree_node = ImGui::TreeNodeEx(utility::bit_cast<void*>(bvh_jid), tree_node_flags,
                "%s (#%zu)"
                , bvh_jinfo.name.c_str()
                , bvh_jid
            );

            if (!has_children) {
                open_tree_node = false; // If there are no children (Leaf), TreeNodeEx only displays the contents and does not open.
            }

            if (ImGui::IsItemClicked()) {
                _state.selected_joint_id = bvh_jid; // update selected jid
            }

            if (open_tree_node && has_children) {
                for (const auto child_bvh_jid : _state.hierarchy_map_cache.at(bvh_jid)) {
                    this->_render_hierarchy_tree(child_bvh_jid);
                }
                ImGui::TreePop();
            }
        }

        std::shared_ptr<geometry::skeleton_object> _create_skeleton_object_from_bvh(
            const io::bvh_file_t& bvh_file,
            const io::bvh_motion_frame_t& bvh_frame) const
        {
            constexpr double kScaleCM2M = 0.01;

            std::vector<geometry::skeleton_joint_info_t> skeleton_joints;
            skeleton_joints.resize(bvh_frame.skeleton.size());
            for (const auto& [bvh_jid, bvh_jdata] : bvh_frame.skeleton)
            {
                skeleton_joints.at(static_cast<size_t>(bvh_jid)) = geometry::skeleton_joint_info_t{
                    (bvh_jdata.world_position * kScaleCM2M).cast<float>().eval(),
                    bvh_jdata.world_rotation.cast<float>().eval(),
                    _state.joint_color,
                    _state.joint_radius
                };
            }

            std::vector<geometry::skeleton_bone_info_t> skeleton_bones;
            skeleton_bones.reserve(bvh_frame.skeleton.size());
            for (const auto [child_bvh_jid, parent_bvh_jid] : bvh_file.joints_parent_map)
            {
                if (child_bvh_jid == parent_bvh_jid) { continue; } // root joint has no parent
                
                const geometry::skeleton_joint_info_t* const from_joint = &skeleton_joints.at(static_cast<size_t>(child_bvh_jid));
                const geometry::skeleton_joint_info_t* const to_joint = &skeleton_joints.at(static_cast<size_t>(parent_bvh_jid));
                const float bone_length = triengine::math::vec3_distance(from_joint->position, to_joint->position);

                skeleton_bones.emplace_back(
                    from_joint/* from_joint */,
                    to_joint/* to_joint */,
                    _state.bone_color/* color */,
                    bone_length * _state.bone_parent_cap_radius_ratio/* parent_cap_radius */,
                    bone_length * _state.bone_child_cap_radius_ratio/* child_cap_radius */,
                    std::max(bone_length * _state.bone_middle_radius_ratio, _state.max_bone_middle_radius)/* middle_radius */,
                    _state.bone_height_ratio_parent/* height_ratio_parent */,
                    _state.bone_resolution/* resolution */
                );
            }

            return geometry::skeleton_object::create(
                skeleton_joints,
                skeleton_bones
            );
        }

    }; // class

} // namespace