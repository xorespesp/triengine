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
            auto& scn_config = *_vis->get_current_scene()->get_render_config();

            if (ImGui::CollapsingHeader("Render Options", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Anti-Aliasing", &scn_config.enable_anti_aliasing);
                ImGui::Checkbox("Show Origin Axes", &scn_config.show_origin_axis);
                ImGui::Checkbox("Show Object Normals", &scn_config.show_object_normals);
                ImGui::Checkbox("Show Wireframe", &scn_config.show_wireframe);
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

                if (ImGui::CollapsingHeader("Lighting"))
                {
                    ImGui::Indent();

                    if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
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
                        ImGui::DragFloat("Ambient Intensity##DirLight", &dir_light_opts.ambientIntensity, 0.001f, 0.0f, 10.0f);
                        ImGui::DragFloat("Diffuse Intensity##DirLight", &dir_light_opts.diffuseIntensity, 0.001f, 0.0f, 10.0f);
                        ImGui::DragFloat("Specular Intensity##DirLight", &dir_light_opts.specularIntensity, 0.001f, 0.0f, 10.0f);
                        if (!dir_light_opts.enabled) { ImGui::EndDisabled(); }
                    }

                    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto& point_light_opts = scn_config.light_opts.point_light;
                        ImGui::Checkbox("Enable##PointLight", &point_light_opts.enabled);

                        if (!point_light_opts.enabled) { ImGui::BeginDisabled(); }
                        ImGui::Checkbox("Show Light Source##PointLight", &point_light_opts.show_light_source);
                        ImGui::DragFloat("Light Source Scale##Bloom", &point_light_opts.light_source_color_intensity, 0.1f, 1.0f, 200.0f);
                        ImGui::DragFloat3("Light Position##PointLight", point_light_opts.position.data(), 0.05f, -FLT_MAX / INT_MAX, FLT_MAX / INT_MAX);
                        ImGui::ColorEdit3("Light Color##PointLight", point_light_opts.color.data());
                        ImGui::DragFloat("Ambient Intensity##PointLight", &point_light_opts.ambientIntensity, 0.001f, 0.0f, 10.0f);
                        ImGui::DragFloat("Diffuse Intensity##PointLight", &point_light_opts.diffuseIntensity, 0.001f, 0.0f, 10.0f);
                        ImGui::DragFloat("Specular Intensity##PointLight", &point_light_opts.specularIntensity, 0.001f, 0.0f, 10.0f);
                        if (!point_light_opts.enabled) { ImGui::EndDisabled(); }
                    }

                    if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto& bloom_opts = scn_config.light_opts.bloom;
                        ImGui::Checkbox("Enable##Bloom", &bloom_opts.enabled);

                        if (!bloom_opts.enabled) { ImGui::BeginDisabled(); }
                        ImGui::DragFloat("Strength##Bloom", &bloom_opts.strength, 0.001f, 0.0f, 1.0f);
                        ImGui::DragFloat("Filter Radius##Bloom", &bloom_opts.upsample_filter_radius, 0.001f, 0.001f, 0.1f);
                        if (!bloom_opts.enabled) { ImGui::EndDisabled(); }
                    }

                    if (ImGui::CollapsingHeader("HDR", ImGuiTreeNodeFlags_DefaultOpen))
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
                        "Transparent Grid",
                        "Box-Filtered Grid",
                        "Box-Filtered Chess"
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
                        case 0: scn_config.inf_plane_opts.plane_option = transparent_grid_plane_option_t{}; break;
                        case 1: scn_config.inf_plane_opts.plane_option = box_filtered_grid_plane_option_t{}; break;
                        case 2: scn_config.inf_plane_opts.plane_option = box_filtered_chess_plane_option_t{}; break;
                        default: TRIENGINE_ASSERT(false); break;
                        }
                    }

                    ImGui::DragFloat("Max View Distance##InfPlane", &scn_config.inf_plane_opts.max_view_distance, 0.1f, 20.0f, 100.0f);
                    ImGui::DragFloat("Grid Cell Size##InfPlane", &scn_config.inf_plane_opts.grid_cell_size, 0.001f, 0.025f, FLT_MAX);

                    std::visit([](auto& pattern_opt) {
                        using T = std::decay_t<decltype(pattern_opt)>;
                        if constexpr (std::is_same_v<T, transparent_grid_plane_option_t>) {
                            ImGui::ColorEdit3("Grid Line Color##InfPlane", pattern_opt.grid_line_color.data());
                        } else if constexpr (std::is_same_v<T, box_filtered_grid_plane_option_t>) {
                            ImGui::ColorEdit3("Grid Line Color##InfPlane", pattern_opt.grid_line_color.data());
                            ImGui::ColorEdit3("Grid Cell Color##InfPlane", pattern_opt.grid_cell_color.data());
                        } else if constexpr (std::is_same_v<T, box_filtered_chess_plane_option_t>) {
                            ImGui::ColorEdit3("Grid Cell Color1##InfPlane", pattern_opt.grid_cell_color1.data());
                            ImGui::ColorEdit3("Grid Cell Color2##InfPlane", pattern_opt.grid_cell_color2.data());
                        } else {
                            TRIENGINE_ASSERT(false);
                        }
                    }, scn_config.inf_plane_opts.plane_option);

                    if (disabled) { ImGui::EndDisabled(); }

                    ImGui::Unindent();
                } // Infinite Grid Options

            } // Render Options

        }
        else
        {
            // ...
        }
    }

} // namespace