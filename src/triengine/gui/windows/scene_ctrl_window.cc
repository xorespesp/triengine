#include "scene_ctrl_window.hh"
#include <triengine/utility/debug_utils.hh>
#include <triengine/visualization/visualizer.hh>

#include <unordered_map>
#include <string>

namespace triengine::gui
{
    scene_control_window::scene_control_window(
        visualization::visualizer* const vis)
        : _vis{ vis }
    { }

    void scene_control_window::render(
        [[maybe_unused]] const window_render_context& render_ctx)
    {
        if (_vis)
        {
            auto& scn = *_vis->get_current_scene();
            auto& scn_config = *scn.get_render_config();

            if (ImGui::CollapsingHeader("Render Options", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Anti-Aliasing", &scn_config.enable_anti_aliasing);
                ImGui::Checkbox("Show Object Normals", &scn_config.show_object_normals);
                ImGui::ColorEdit4("BG Color", scn_config.bg_color.data());

                {
                    using enum_type = triengine::skeleton_render_mode;
                    static const std::unordered_map<enum_type, std::string> item_names = {
                        { enum_type::skeleton_default, "skeleton_default" },
                        { enum_type::skeleton_overlay, "skeleton_overlay" },
                        { enum_type::overlay_with_joint_axis, "overlay_with_joint_axis" }
                    };

                    if (const enum_type selected_item{ scn_config.skeleton_mode };
                        ImGui::BeginCombo("Skeleton Render Mode", item_names.find(selected_item)->second.c_str())) {
                        for (int32_t curr_item_value{ 0 }; curr_item_value < static_cast<int32_t>(item_names.size()); ++curr_item_value) {
                            const enum_type curr_item{ static_cast<enum_type>(curr_item_value) };
                            const bool is_selected{ (selected_item == curr_item) };
                            if (ImGui::Selectable(item_names.find(curr_item)->second.c_str(), is_selected)) {
                                // Selection changed
                                scn_config.skeleton_mode = static_cast<enum_type>(curr_item_value);
                            }

                            if (is_selected) {
                                // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
                                ImGui::SetItemDefaultFocus();
                            }
                        } // for
                        ImGui::EndCombo();
                    }
                }

                if (ImGui::CollapsingHeader("Camera"))
                {
                    ImGui::Indent();

                    static constexpr std::array<const char*, 4> kCameraTypeNamesMap = {
                        "Fly",
                        "Arcball",
                        "Ortho",
                        "Pinhole"
                    };

                    if (int cam_type_idx = static_cast<int>(scn.get_camera()->get_type());
                        ImGui::Combo(
                            "Camera Type##CameraType",
                            &cam_type_idx,
                            kCameraTypeNamesMap.data(),
                            static_cast<int>(kCameraTypeNamesMap.size())
                        )) {
                        scn.switch_camera_type(static_cast<triengine::camera_type>(cam_type_idx));
                    }

                    ImGui::SeparatorText("Camera Control");

                    thread_local bool fl_smooth_update{ true };
                    ImGui::Checkbox("Smooth Update", &fl_smooth_update);

                    const auto cam_type = scn.get_camera()->get_type();
                    if (cam_type == camera_type::fly)
                    {
                        auto* fly_cam = scn.get_camera()->as<fly_camera>();
                        TRIENGINE_ASSERT(fly_cam != nullptr);
                        auto& fly_cam_opts = fly_cam->get_options();

                        ImGui::DragFloat("Movement Speed", &fly_cam_opts.movement_speed,
                            0.1f, 0.1f, 100.0f,
                            "%.2f",
                            ImGuiSliderFlags_AlwaysClamp
                        );

                        ImGui::DragFloat("Mouse Sensitivity", &fly_cam_opts.mouse_sensitivity,
                            0.01f, 0.01f, 10.0f,
                            "%.2f",
                            ImGuiSliderFlags_AlwaysClamp
                        );

                        ImGui::DragFloat("Damping Factor", &fly_cam_opts.damping_factor,
                            0.1f, 1.0f, 30.0f,
                            "%.2f",
                            ImGuiSliderFlags_AlwaysClamp
                        );

                        if (auto position = fly_cam->get_position();
                            ImGui::DragFloat3("Position", position.data(),
                                0.1f, -100.0f, 100.0f)) {
                            fly_cam->set_position(position, fl_smooth_update);
                        }

                        if (auto direction = fly_cam->get_direction();
                            ImGui::DragFloat3("Direction", direction.data(),
                                0.01f, -1.0f, 1.0f,
                                "%.3f",
                                ImGuiSliderFlags_AlwaysClamp)) {
                            fly_cam->set_direction(direction, fl_smooth_update);
                        }

                        if (float yaw = fly_cam->get_yaw();
                            ImGui::DragFloat("Yaw", &yaw,
                                1.0f, -180.0f, 180.0f,
                                "%.2f",
                                ImGuiSliderFlags_AlwaysClamp)) {
                            fly_cam->set_yaw(yaw, fl_smooth_update);
                        }

                        if (float pitch = fly_cam->get_pitch();
                            ImGui::DragFloat("Pitch", &pitch,
                                1.0f, camera_constants::kMinPitch, camera_constants::kMaxPitch,
                                "%.2f",
                                ImGuiSliderFlags_AlwaysClamp)) {
                            fly_cam->set_pitch(pitch, fl_smooth_update);
                        }

                        if (float fovy = fly_cam->get_fovy();
                            ImGui::DragFloat("Fovy", &fovy,
                                0.1f, camera_constants::kMinFovy, camera_constants::kMaxFovy,
                                "%.2f",
                                ImGuiSliderFlags_AlwaysClamp)) {
                            fly_cam->set_fovy(fovy, fl_smooth_update);
                        }
                    }
                    else if (cam_type == camera_type::arcball)
                    {
                        auto* arcball_cam = scn.get_camera()->as<arcball_camera>();
                        TRIENGINE_ASSERT(arcball_cam != nullptr);
                        auto& arcball_cam_opts = arcball_cam->get_options();

                        ImGui::DragFloat("Mouse Sensitivity", &arcball_cam_opts.mouse_sensitivity,
                            0.01f, 0.01f, 10.0f, 
                            "%.2f",
                            ImGuiSliderFlags_AlwaysClamp
                        );

                        ImGui::DragFloat("Damping Factor", &arcball_cam_opts.damping_factor,
                            0.1f, 1.0f, 30.0f, 
                            "%.2f",
                            ImGuiSliderFlags_AlwaysClamp
                        );

                        if (auto pivot_point = arcball_cam->get_pivot_point();
                            ImGui::DragFloat3("Pivot Point", pivot_point.data(),
                                0.1f, -100.0f, 100.0f)) {
                            arcball_cam->set_pivot_point(pivot_point, fl_smooth_update);
                        }

                        if (float zoom_distance = arcball_cam->get_zoom_distance();
                            ImGui::DragFloat("Zoom Distance", &zoom_distance,
                                0.1f, camera_constants::kMinArcballZoomDistance, camera_constants::kMaxArcballZoomDistance)) {
                            arcball_cam->set_zoom_distance(zoom_distance, fl_smooth_update);
                        }

                        if (float yaw = arcball_cam->get_yaw();
                            ImGui::DragFloat("Yaw", &yaw,
                                1.0f, -180.0f, 180.0f, 
                                "%.2f",
                                ImGuiSliderFlags_AlwaysClamp)) {
                            arcball_cam->set_yaw(yaw, fl_smooth_update);
                        }

                        if (float pitch = arcball_cam->get_pitch();
                            ImGui::DragFloat("Pitch", &pitch,
                                1.0f, camera_constants::kMinPitch, camera_constants::kMaxPitch,
                                "%.2f",
                                ImGuiSliderFlags_AlwaysClamp)) {
                            arcball_cam->set_pitch(pitch, fl_smooth_update);
                        }

                        if (float fovy = arcball_cam->get_fovy();
                            ImGui::DragFloat("Fovy", &fovy,
                                0.1f, camera_constants::kMinFovy, camera_constants::kMaxFovy,
                                "%.2f",
                                ImGuiSliderFlags_AlwaysClamp)) {
                            arcball_cam->set_fovy(fovy, fl_smooth_update);
                        }
                    }
                    else if (cam_type == camera_type::ortho)
                    {
                        auto* ortho_cam = scn.get_camera()->as<ortho_camera>();
                        TRIENGINE_ASSERT(ortho_cam != nullptr);
                        auto& ortho_cam_opts = ortho_cam->get_options();

                        ImGui::DragFloat("Mouse Sensitivity", &ortho_cam_opts.mouse_sensitivity,
                            0.01f, 0.01f, 10.0f,
                            "%.2f",
                            ImGuiSliderFlags_AlwaysClamp
                        );

                        ImGui::DragFloat("Damping Factor", &ortho_cam_opts.damping_factor,
                            0.1f, 1.0f, 30.0f,
                            "%.2f",
                            ImGuiSliderFlags_AlwaysClamp
                        );

                        if (auto pivot_point = ortho_cam->get_pivot_point();
                            ImGui::DragFloat3("Pivot Point", pivot_point.data(),
                                0.1f, -100.0f, 100.0f)) {
                            ortho_cam->set_pivot_point(pivot_point, fl_smooth_update);
                        }

                        if (float zoom_distance = ortho_cam->get_zoom_distance();
                            ImGui::DragFloat("Zoom Distance", &zoom_distance,
                                0.1f, camera_constants::kMinArcballZoomDistance, camera_constants::kMaxArcballZoomDistance)) {
                            ortho_cam->set_zoom_distance(zoom_distance, fl_smooth_update);
                        }

                        if (float yaw = ortho_cam->get_yaw();
                            ImGui::DragFloat("Yaw", &yaw,
                                1.0f, -180.0f, 180.0f,
                                "%.2f",
                                ImGuiSliderFlags_AlwaysClamp)) {
                            ortho_cam->set_yaw(yaw, fl_smooth_update);
                        }

                        if (float pitch = ortho_cam->get_pitch();
                            ImGui::DragFloat("Pitch", &pitch,
                                1.0f, camera_constants::kMinPitch, camera_constants::kMaxPitch,
                                "%.2f",
                                ImGuiSliderFlags_AlwaysClamp)) {
                            ortho_cam->set_pitch(pitch, fl_smooth_update);
                        }

                        if (float view_height = ortho_cam->get_ortho_view_height();
                            ImGui::DragFloat("View Height", &view_height,
                                0.1f, camera_constants::kMinOrthoViewHeight, camera_constants::kMaxOrthoViewHeight,
                                "%.2f",
                                ImGuiSliderFlags_AlwaysClamp)) {
                            ortho_cam->set_ortho_view_height(view_height, fl_smooth_update);
                        }
                    }
                    else if (cam_type == camera_type::pinhole)
                    {
                        auto* pinhole_cam = scn.get_camera()->as<pinhole_camera>();
                        TRIENGINE_ASSERT(pinhole_cam != nullptr);

                        // The pinhole camera is driven by calibration data (intrinsics + extrinsics)
                        // rather than mouse interaction, so only its intrinsics are exposed here.
                        // fx/fy, cx/cy, and image width/height are each declared as adjacent members,
                        // so their addresses can be passed to the 2-component widgets below.
                        auto intrinsics = pinhole_cam->get_intrinsics();
                        bool intrinsics_changed = false;

                        constexpr double kMinFocalLength{ 1.0 }, kMaxFocalLength{ 10000.0 };
                        constexpr double kMinPrincipalPoint{ 0.0 }, kMaxPrincipalPoint{ 10000.0 };
                        intrinsics_changed |= ImGui::DragScalarN("Focal Length (fx, fy)", ImGuiDataType_Double, &intrinsics.fx, 2,
                            1.0f, &kMinFocalLength, &kMaxFocalLength, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                        intrinsics_changed |= ImGui::DragScalarN("Principal Point (cx, cy)", ImGuiDataType_Double, &intrinsics.cx, 2,
                            1.0f, &kMinPrincipalPoint, &kMaxPrincipalPoint, "%.1f");
                        intrinsics_changed |= ImGui::DragInt2("Image Size (w, h)", &intrinsics.image_width,
                            1.0f, 1, 8192);

                        if (intrinsics_changed) {
                            pinhole_cam->set_intrinsics(intrinsics);
                        }
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ERROR: Unknown Camera Type");
                    }

                    ImGui::Unindent();
                } // Camera Options

                if (ImGui::CollapsingHeader("Lighting"))
                {
                    ImGui::Indent();

                    ImGui::SeparatorText("Directional Light");
                    {
                        auto& dir_light_opts = scn_config.light_opts.dir_light;
                        ImGui::Checkbox("Enable##DirLight", &dir_light_opts.enabled);

                        if (!dir_light_opts.enabled) { ImGui::BeginDisabled(); }
                        ImGui::Checkbox("Follow Camera##DirLight", &dir_light_opts.follow_camera);
                        ImGui::DragFloat3("Light Direction##DirLight",
                            dir_light_opts.direction.data(),
                            0.01f,
                            -1.0f, 1.0f,
                            "%.3f",
                            dir_light_opts.follow_camera ? ImGuiSliderFlags_ReadOnly : ImGuiSliderFlags_None
                        );
                        ImGui::ColorEdit3("Light Color##DirLight", dir_light_opts.color.data());
                        ImGui::DragFloat("Ambient Intensity##DirLight", &dir_light_opts.ambient_intensity, 0.001f, 0.0f, 10.0f);
                        ImGui::DragFloat("Diffuse Intensity##DirLight", &dir_light_opts.diffuse_intensity, 0.001f, 0.0f, 10.0f);
                        ImGui::DragFloat("Specular Intensity##DirLight", &dir_light_opts.specular_intensity, 0.001f, 0.0f, 10.0f);
                        if (!dir_light_opts.enabled) { ImGui::EndDisabled(); }
                    }

                    ImGui::SeparatorText("Point Light");
                    {
                        auto& point_light_opts = scn_config.light_opts.point_light;
                        ImGui::Checkbox("Enable##PointLight", &point_light_opts.enabled);

                        if (!point_light_opts.enabled) { ImGui::BeginDisabled(); }
                        ImGui::Checkbox("Show Light Source##PointLight", &point_light_opts.show_light_source);
                        ImGui::DragFloat("Light Source Scale##Bloom", &point_light_opts.light_source_color_intensity, 0.1f, 1.0f, 200.0f);
                        ImGui::DragFloat3("Light Position##PointLight", point_light_opts.position.data(), 0.05f, -FLT_MAX / INT_MAX, FLT_MAX / INT_MAX);
                        ImGui::ColorEdit3("Light Color##PointLight", point_light_opts.color.data());
                        ImGui::DragFloat("Ambient Intensity##PointLight", &point_light_opts.ambient_intensity, 0.001f, 0.0f, 10.0f);
                        ImGui::DragFloat("Diffuse Intensity##PointLight", &point_light_opts.diffuse_intensity, 0.001f, 0.0f, 10.0f);
                        ImGui::DragFloat("Specular Intensity##PointLight", &point_light_opts.specular_intensity, 0.001f, 0.0f, 10.0f);
                        if (!point_light_opts.enabled) { ImGui::EndDisabled(); }
                    }

                    // Simple Fog Options
                    ImGui::SeparatorText("Fog");
                    {
                        auto& simple_fog_opts = scn_config.light_opts.simple_fog;

                        ImGui::Checkbox("Enabled", &simple_fog_opts.enabled);
                        ImGui::DragFloat("Fog Density", &simple_fog_opts.fog_density,
                            0.001f, 0.0f, 100.0f,
                            "%.3f",
                            ImGuiSliderFlags_AlwaysClamp
                        );
                        ImGui::DragFloat("Fog Start Distance", &simple_fog_opts.fog_start_dist,
                            0.1f, 0.0f, 100.0f,
                            "%.2f",
                            ImGuiSliderFlags_AlwaysClamp
                        );
                    }

                    ImGui::SeparatorText("Bloom");
                    {
                        auto& bloom_opts = scn_config.light_opts.bloom;
                        ImGui::Checkbox("Enable##Bloom", &bloom_opts.enabled);

                        if (!bloom_opts.enabled) { ImGui::BeginDisabled(); }
                        ImGui::DragFloat("Strength##Bloom", &bloom_opts.strength, 0.001f, 0.0f, 1.0f);
                        ImGui::DragFloat("Filter Radius##Bloom", &bloom_opts.upsample_filter_radius, 0.001f, 0.001f, 0.1f);
                        if (!bloom_opts.enabled) { ImGui::EndDisabled(); }
                    }

                    ImGui::SeparatorText("HDR");
                    {
                        using enum_type = triengine::tone_mapping_curve_type;
                        static const std::unordered_map<enum_type, std::string> item_names = {
                            { enum_type::reinherd, "reinherd" },
                            { enum_type::uncharted2_filmic, "uncharted2_filmic" },
                            { enum_type::aces_filmic, "aces_filmic" }
                        };

                        auto& hdr_opts = scn_config.light_opts.hdr;
                        ImGui::Checkbox("Enable##HDR", &hdr_opts.enabled);

                        if (!hdr_opts.enabled) { ImGui::BeginDisabled(); }
                        ImGui::DragFloat("Exposure##HDR", &hdr_opts.exposure, 0.001f, 0.0f, 1.0f);
                        if (const enum_type selected_item{ hdr_opts.tone_mapping_curve };
                            ImGui::BeginCombo("Tone-Mapping Curve##HDR", item_names.find(selected_item)->second.c_str())) {
                            for (int32_t curr_item_value{ 0 }; curr_item_value < static_cast<int32_t>(item_names.size()); ++curr_item_value) {
                                const enum_type curr_item{ static_cast<enum_type>(curr_item_value) };
                                const bool is_selected{ (selected_item == curr_item) };
                                if (ImGui::Selectable(item_names.find(curr_item)->second.c_str(), is_selected)) {
                                    // Selection changed
                                    hdr_opts.tone_mapping_curve = static_cast<enum_type>(curr_item_value);
                                }

                                if (is_selected) {
                                    // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
                                    ImGui::SetItemDefaultFocus();
                                }
                            } // for
                            ImGui::EndCombo();
                        }
                        if (!hdr_opts.enabled) { ImGui::EndDisabled(); }
                    }

                    ImGui::Unindent();
                } // Lighting Options

                if (ImGui::CollapsingHeader("Infinite Plane"))
                {
                    ImGui::Indent();

                    ImGui::Checkbox("Enable##InfPlane", &scn_config.show_origin_xz_grid);
                    const bool disabled = !scn_config.show_origin_xz_grid;

                    if (disabled) { ImGui::BeginDisabled(); }

                    static constexpr std::array<const char*, 3> kInfPlanePatternTypeNamesMap = {
                        "Box-Filtered Chess", // 0
                        "Box-Filtered Grid", // 1
                        "Transparent Grid" // 2
                    };

                    if (int combo_idx = static_cast<int>(scn_config.inf_plane_opts.plane_option.index());
                        ImGui::Combo(
                            "Plane Type",
                            &combo_idx,
                            kInfPlanePatternTypeNamesMap.data(),
                            static_cast<int>(kInfPlanePatternTypeNamesMap.size())
                        ))
                    {
                        switch (combo_idx) {
                        case 0: scn_config.inf_plane_opts.plane_option = box_filtered_chess_plane_option_t{}; break;
                        case 1: scn_config.inf_plane_opts.plane_option = box_filtered_grid_plane_option_t{}; break;
                        case 2: scn_config.inf_plane_opts.plane_option = transparent_grid_plane_option_t{}; break;
                        default: TRIENGINE_ASSERT(false); break;
                        }
                    }

                    ImGui::DragFloat("Max View Distance##InfPlane", &scn_config.inf_plane_opts.max_view_distance, 0.1f, 20.0f, 100.0f);
                    ImGui::DragFloat("Grid Cell Size##InfPlane", &scn_config.inf_plane_opts.grid_cell_size, 0.001f, 0.025f, FLT_MAX);

                    std::visit([](auto& pattern_opt) {
                        constexpr uint16_t kMinShininess{ 1 }, kMaxShininess{ 256 };
                        using T = std::decay_t<decltype(pattern_opt)>;
                        if constexpr (std::is_same_v<T, transparent_grid_plane_option_t>) {
                            ImGui::ColorEdit3("Grid Line Color##InfPlane", pattern_opt.grid_line_color.data());
                        } else if constexpr (std::is_same_v<T, box_filtered_grid_plane_option_t>) {
                            ImGui::ColorEdit3("Grid Line Color##InfPlane", pattern_opt.grid_line_color.data());
                            ImGui::ColorEdit3("Grid Cell Color##InfPlane", pattern_opt.grid_cell_color.data());
                            ImGui::DragFloat("Ambient Intensity##InfPlane", &pattern_opt.material.ambient_intensity, 0.001f, 0.0f, 10.0f);
                            ImGui::DragFloat("Diffuse Intensity##InfPlane", &pattern_opt.material.diffuse_intensity, 0.001f, 0.0f, 10.0f);
                            ImGui::DragFloat("Specular Intensity##InfPlane", &pattern_opt.material.specular_intensity, 0.001f, 0.0f, 10.0f);
                            ImGui::DragScalar("Shininess##InfPlane", ImGuiDataType_U16, &pattern_opt.material.shininess, 
                                1.0f, &kMinShininess, &kMaxShininess);
                        } else if constexpr (std::is_same_v<T, box_filtered_chess_plane_option_t>) {
                            ImGui::ColorEdit3("Grid Cell Color1##InfPlane", pattern_opt.grid_cell_color1.data());
                            ImGui::ColorEdit3("Grid Cell Color2##InfPlane", pattern_opt.grid_cell_color2.data());
                            ImGui::DragFloat("Ambient Intensity##InfPlane", &pattern_opt.material.ambient_intensity, 0.001f, 0.0f, 10.0f);
                            ImGui::DragFloat("Diffuse Intensity##InfPlane", &pattern_opt.material.diffuse_intensity, 0.001f, 0.0f, 10.0f);
                            ImGui::DragFloat("Specular Intensity##InfPlane", &pattern_opt.material.specular_intensity, 0.001f, 0.0f, 10.0f);
                            ImGui::DragScalar("Shininess##InfPlane", ImGuiDataType_U16, &pattern_opt.material.shininess,
                                1.0f, &kMinShininess, &kMaxShininess);
                        } else {
                            TRIENGINE_ASSERT(false);
                        }
                    }, scn_config.inf_plane_opts.plane_option);

                    if (disabled) { ImGui::EndDisabled(); }

                    ImGui::Unindent();
                } // Infinite Grid Options

                if (ImGui::CollapsingHeader("Text Render"))
                {
                    ImGui::Indent();
                    ImGui::Checkbox("Enable##TextRenderOption", &scn_config.text_render_opts.enabled);

	                {
		                ImGui::SeparatorText("Distance Scaling Options");

                        auto& scale_opts = scn_config.text_render_opts.dist_scale_opts;

		                // Enable/Disable distance scaling
		                ImGui::Checkbox("Enable##TextDistanceScalingOption", &scale_opts.enabled);

		                if (scale_opts.enabled)
		                {
			                ImGui::Separator();

			                // Scaling type selection
			                const char* scaling_type_names[] = { "Fade Out", "Perspective" };
			                int current_type = static_cast<int>(scale_opts.scale_mode);
			
			                if (ImGui::Combo("Scaling Type", &current_type, scaling_type_names, IM_ARRAYSIZE(scaling_type_names))) {
				                scale_opts.scale_mode = static_cast<text_render_options::dist_scale_mode_type>(current_type);
			                }

			                // Show description based on scaling type
			                if (scale_opts.scale_mode == text_render_options::dist_scale_mode_type::fade_out) {
				                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
					                "Fade Out: Text maintains original size, fades with distance");
			                } else {
				                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
					                "Perspective: Text scales inversely with distance (3D effect)");
			                }

			                ImGui::Separator();

			                // Reference distance
			                if (ImGui::DragFloat("Reference Distance", &scale_opts.ref_distance, 
				                0.1f, 0.1f, 100.0f, "%.2f")) {
			                }
			                ImGui::SameLine();
			                ImGui::TextDisabled("(?)");
			                if (ImGui::IsItemHovered()) {
				                ImGui::SetTooltip("Distance where text appears at scale 1.0");
			                }

			                // Max distance
			                if (ImGui::DragFloat("Max Distance", &scale_opts.max_distance, 
				                0.1f, scale_opts.ref_distance + 0.1f, 200.0f, "%.2f")) {
			                }
			                ImGui::SameLine();
			                ImGui::TextDisabled("(?)");
			                if (ImGui::IsItemHovered()) {
				                ImGui::SetTooltip("Distance where text completely disappears");
			                }

			                // Ensure max_distance is always greater than ref_distance
			                if (scale_opts.max_distance <= scale_opts.ref_distance) {
				                scale_opts.max_distance = scale_opts.ref_distance + 0.1f;
			                }

			                ImGui::Separator();
		                }
	                }

                    {
                        ImGui::SeparatorText("Depth Testing Options");

                        auto& depth_test_opts = scn_config.text_render_opts.depth_test_opts;

                        // Enable/Disable depth testing
                        ImGui::Checkbox("Enable##TextDepthTestOption", &depth_test_opts.enabled);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(?)");
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Enable occlusion testing for 3D labels using scene depth buffer");
                        }

                        if (depth_test_opts.enabled)
                        {
                            ImGui::Separator();

                            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                                "3D labels will be made semi-transparent when occluded by scene geometry");

                            ImGui::Separator();

                            // Depth bias
                            if (ImGui::DragFloat("Depth Bias", &depth_test_opts.depth_bias,
                                0.0001f, 0.0f, 0.1f, "%.4f")) {
                            }
                            ImGui::SameLine();
                            ImGui::TextDisabled("(?)");
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Small offset to prevent z-fighting artifacts");
                            }

                            // Occlusion alpha
                            if (ImGui::DragFloat("Occlusion Alpha", &depth_test_opts.occlusion_alpha,
                                0.01f, 0.0f, 1.0f, "%.2f")) {
                            }
                            ImGui::SameLine();
                            ImGui::TextDisabled("(?)");
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Alpha multiplier when text is occluded (0.0 = invisible, 1.0 = no change)");
                            }

                            ImGui::Separator();

                            // Debug info
                            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                                "Camera: Near=%.2f, Far=%.2f",
                                triengine::camera_constants::kNearPlane,
                                triengine::camera_constants::kFarPlane);
                        }
                    }

                    ImGui::Unindent();
                } // Text Render Options
            } // Render Options

        }
        else //if (!_vis)
        {
            // ...
        }
    }

} // namespace