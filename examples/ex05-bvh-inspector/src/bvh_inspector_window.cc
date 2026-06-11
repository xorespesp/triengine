#include "bvh_inspector_window.hh"

#include <triengine/utility/bit_cast.hh>
#include <xutl/string/path_utils.hh>
#include <xutl/debug/panic.hh>
#include <xutl/debug/assert.hh>

namespace demo
{
    using namespace triengine;

    bvh_inspector_window::bvh_inspector_window(
        std::shared_ptr<scene> target_scene)
        : _scene{ target_scene }
    {
        auto scn = _scene;
        scn->set_name("bvh scene");

        scn->get_render_config()->bg_color = color4_f32::all(1.0f);
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
        _file_browser.set_max_visible_items(25);

        const std::filesystem::path curr_image_dir_path{ _XUTL string::get_current_module_image_path().parent_path() };
        _file_browser.set_cwd(curr_image_dir_path);
    }

    const char* bvh_inspector_window::get_window_name() const {
        return "BVH Inspector Window";
    }

    ImVec2 bvh_inspector_window::get_initial_window_size() const {
        return ImVec2{ 540.0f, 650.0f };
    }

    scene* bvh_inspector_window::get_scene() noexcept {
        return _scene.get();
    }

    const scene* bvh_inspector_window::get_scene() const noexcept {
        return _scene.get();
    }

    bool bvh_inspector_window::is_loaded() const noexcept {
        return _state.bvh_data != nullptr;
    }

    bool bvh_inspector_window::is_playing() const noexcept {
        return this->is_loaded() && _state.playback_state == playback_state_type::playing;
    }

    void bvh_inspector_window::play() noexcept {
        if (this->is_loaded()) {
            _state.playback_state = playback_state_type::playing;
        }
    }

    void bvh_inspector_window::pause() noexcept {
        if (this->is_loaded()) {
            _state.playback_state = playback_state_type::paused;
        }
    }

    void bvh_inspector_window::move_to_next_frame() noexcept {
        if (this->is_loaded()) {
            _state.playback_state = playback_state_type::paused;
            _state.current_frame_index = static_cast<int32_t>(static_cast<size_t>(_state.current_frame_index + 1) % _state.bvh_data->frames.size());
            _state.fl_pose_dirty = true;
        }
    }

    void bvh_inspector_window::move_to_prev_frame() noexcept {
        if (this->is_loaded()) {
            _state.playback_state = playback_state_type::paused;
            _state.current_frame_index = static_cast<int32_t>((static_cast<size_t>(_state.current_frame_index - 1) + _state.bvh_data->frames.size()) % _state.bvh_data->frames.size());
            _state.fl_pose_dirty = true;
        }
    }

    void bvh_inspector_window::update_animation()
    {
        // Playback handling
        if (this->is_loaded() &&
            this->is_playing())
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
                _state.current_frame_index = static_cast<int32_t>(static_cast<size_t>(_state.current_frame_index + 1) % bvh_data.frames.size());
                _state.fl_pose_dirty = true;
            }
        }

        // Rebuild the skeleton when its hierarchy or appearance changed.
        if (_state.fl_rebuild_skeleton)
        {
            if (_bvh_skeleton) {
                this->get_scene()->remove_geometry(_bvh_skeleton);
                _bvh_skeleton.reset();
            }

            if (this->is_loaded())
            {
                const io::bvh_file_t& bvh_data = *_state.bvh_data;
                _bvh_skeleton = this->_build_skeleton_object(
                    bvh_data,
                    bvh_data.frames[_state.current_frame_index]
                );

                if (_state.offset_transform) {
                    _bvh_skeleton->transform(_state.offset_transform.value(), true);
                    _origin_axis->transform(_state.offset_transform.value(), false);
                }

                this->get_scene()->add_geometry(_bvh_skeleton);
            }

            _state.fl_rebuild_skeleton = false;
            _state.fl_pose_dirty = false;
        }
        // Otherwise, apply an in-place pose update for the current frame (hot path).
        else if (_state.fl_pose_dirty && _bvh_skeleton)
        {
            const io::bvh_file_t& bvh_data = *_state.bvh_data;
            _bvh_skeleton->set_pose(
                this->_make_skeleton_pose(bvh_data.frames[_state.current_frame_index])
            );
            _state.fl_pose_dirty = false;
        }
    }

    void bvh_inspector_window::render(
        [[maybe_unused]] const gui::window_render_context& render_ctx)
    {
        if (this->is_loaded())
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
                _state.fl_pose_dirty = true;
            }

            ImGui::SameLine();

            if (_state.playback_state == playback_state_type::playing) {
                if (ImGui::Button("||")) {
                    _state.playback_state = playback_state_type::paused;
                }
            }
            else {
                if (ImGui::Button("> ")) {
                    _state.playback_state = playback_state_type::playing;
                }
            }

            ImGui::SameLine();

            if (ImGui::Button(">|")) {
                _state.current_frame_index = static_cast<int>(_state.bvh_data->frames.size() - 1);
                _state.fl_pose_dirty = true;
            }

            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);
            if (ImGui::SliderInt("##FrameSlider"
                , &_state.current_frame_index
                , 0
                , static_cast<int>(bvh_data.frames.size()) - 1
                , "Frame: %d"))
            {
                _state.playback_state = playback_state_type::paused;
                _state.fl_pose_dirty = true;
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::Text("/ %zu frames", bvh_data.frames.size());

            // Playback speed affects only frame timing, not geometry or pose.
            ImGui::DragFloat("Playback Speed", &_state.playback_speed, 0.01f, 0.01f, 4.0f, "%.2fx");

            if (ImGui::CollapsingHeader("Skeleton Render Options", ImGuiTreeNodeFlags_None))
            {
                ImGui::Indent();

                if (ImGui::ColorEdit3("Joint Color", _state.joint_color.data(), ImGuiColorEditFlags_NoAlpha)) {
                    _state.fl_rebuild_skeleton = true;
                }

                if (ImGui::ColorEdit3("Selected Joint Color", _state.selected_joint_color.data(), ImGuiColorEditFlags_NoAlpha)) {
                    _state.fl_rebuild_skeleton = true;
                }

                if (ImGui::ColorEdit3("Bone Color", _state.bone_color.data(), ImGuiColorEditFlags_NoAlpha)) {
                    _state.fl_rebuild_skeleton = true;
                }

                if (ImGui::DragFloat("Joint Radius", &_state.joint_radius, 0.001f, 0.001f, 0.1f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                    _state.fl_rebuild_skeleton = true;
                }

                if (ImGui::DragFloat("Bone Parent Cap Radius Ratio", &_state.bone_parent_cap_radius_ratio, 0.001f, 0.001f, 0.5f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                    _state.fl_rebuild_skeleton = true;
                }

                if (ImGui::DragFloat("Bone Child Cap Radius Ratio", &_state.bone_child_cap_radius_ratio, 0.001f, 0.001f, 0.5f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                    _state.fl_rebuild_skeleton = true;
                }

                if (ImGui::DragFloat("Bone Middle Radius Ratio", &_state.bone_middle_radius_ratio, 0.001f, 0.001f, 0.5f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                    _state.fl_rebuild_skeleton = true;
                }

                if (ImGui::DragFloat("Max Bone Middle Radius", &_state.max_bone_middle_radius, 0.001f, 0.001f, 0.2f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                    _state.fl_rebuild_skeleton = true;
                }

                if (ImGui::DragFloat("Bone Height Ratio", &_state.bone_height_ratio_parent, 0.001f, 0.001f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                    _state.fl_rebuild_skeleton = true;
                }

                if (ImGui::SliderInt("Bone Resolution", &_state.bone_resolution, 3, 40, "%d")) {
                    _state.fl_rebuild_skeleton = true;
                }

                ImGui::Unindent();
            }

            ImGui::Spacing();

            if (ImGui::CollapsingHeader("BVH File Data", ImGuiTreeNodeFlags_DefaultOpen))
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

                    XUTL_ASSERT(_state.selected_joint_id.value() < bvh_data.joints.size());

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
                    XUTL_PANIC("failed to load skeletons from bvh file");
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

                _state.fl_rebuild_skeleton = true;
            }
        }
    }

    void bvh_inspector_window::_render_hierarchy_tree(
        const io::bvh_joint_id_t bvh_jid)
    {
        XUTL_ASSERT(bvh_jid < _state.bvh_data->joints.size());

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
            _state.fl_rebuild_skeleton = true; // for update selected joint color in skeleton geometry
        }

        if (open_tree_node && has_children) {
            for (const auto child_bvh_jid : _state.hierarchy_map_cache.at(bvh_jid)) {
                this->_render_hierarchy_tree(child_bvh_jid);
            }
            ImGui::TreePop();
        }
    }

    geometry::skeleton_pose_t bvh_inspector_window::_make_skeleton_pose(
        const io::bvh_motion_frame_t& bvh_frame) const
    {
        constexpr double kScaleCM2M = 0.01;

        geometry::skeleton_pose_t pose;
        pose.resize(bvh_frame.skeleton.size());
        for (const auto& [bvh_jid, bvh_jdata] : bvh_frame.skeleton)
        {
            geometry::skeleton_joint_pose_t& jp = pose.at(static_cast<size_t>(bvh_jid));
            jp.position = (bvh_jdata.world_position * kScaleCM2M).cast<float>().eval();
            jp.rotation = bvh_jdata.world_rotation.cast<float>().eval();
        }

        return pose;
    }

    std::shared_ptr<geometry::skeleton_object> bvh_inspector_window::_build_skeleton_object(
        const io::bvh_file_t& bvh_file,
        const io::bvh_motion_frame_t& ref_frame) const
    {
        // Reference pose for the initial frame; also used to derive per-bone
        // lengths for radius scaling.
        const geometry::skeleton_pose_t ref_pose = this->_make_skeleton_pose(ref_frame);

        geometry::skeleton_object::builder builder;

        // Add joints in bvh joint-id order, so each skeleton joint id equals its
        // bvh joint id. Appearance comes from the ui state; the selected joint is
        // highlighted with a distinct color.
        for (io::bvh_joint_id_t bvh_jid = 0; bvh_jid < bvh_file.joints.size(); ++bvh_jid)
        {
            const bool is_selected =
                _state.selected_joint_id.has_value() &&
                _state.selected_joint_id.value() == bvh_jid;

            geometry::skeleton_joint_desc_t jdesc;
            jdesc.color = is_selected ? _state.selected_joint_color : _state.joint_color;
            jdesc.radius = _state.joint_radius;

            builder.add_joint(jdesc, ref_pose.at(bvh_jid));
        }

        // Connect bones; each bone's radii are scaled by its reference length.
        for (const auto [child_bvh_jid, parent_bvh_jid] : bvh_file.joints_parent_map)
        {
            if (child_bvh_jid == parent_bvh_jid) { continue; } // root joint has no parent

            const float bone_length = math::vec3_distance(
                ref_pose.at(child_bvh_jid).position,
                ref_pose.at(parent_bvh_jid).position
            );

            geometry::skeleton_bone_desc_t bdesc;
            bdesc.color = _state.bone_color;
            bdesc.parent_cap_radius = bone_length * _state.bone_parent_cap_radius_ratio;
            bdesc.child_cap_radius = bone_length * _state.bone_child_cap_radius_ratio;
            bdesc.middle_radius = std::max(bone_length * _state.bone_middle_radius_ratio, _state.max_bone_middle_radius);
            bdesc.height_ratio_parent = _state.bone_height_ratio_parent;
            bdesc.resolution = _state.bone_resolution;

            builder.add_bone(parent_bvh_jid, child_bvh_jid, bdesc);
        }

        return builder.build();
    }


} // namespace