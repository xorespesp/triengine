#include "scene_ctrl_window.hh"
#include "../../misc/debug_utils.hh"
#include "../../visualizer_window.hh"

#include <unordered_map>
#include <string>

namespace triengine::gui
{
    scene_control_window::scene_control_window(
        visualizer_window* vis_window)
        : _vis_window{ vis_window }
    { }

    void scene_control_window::render(
        [[maybe_unused]] const window_render_context& render_ctx)
    {
        if (_vis_window)
        {
            auto& render_config = _vis_window->get_render_config();

            if (ImGui::CollapsingHeader("Render Options", ImGuiTreeNodeFlags_DefaultOpen))
            {
                {
                    using enum_type = triengine::visualizer_window::view_layout_mode;
                    static const std::unordered_map<enum_type, std::string> item_names = {
                        { enum_type::one_view, "one_view" },
                        { enum_type::two_views, "two_view" },
                        { enum_type::three_views, "three_views" }
                    };

                    const enum_type selected_item = render_config.view_layout;
                    if (ImGui::BeginCombo("View Layout", item_names.find(selected_item)->second.c_str())) {
                        for (int32_t curr_item_value = 0; curr_item_value < static_cast<int32_t>(item_names.size()); ++curr_item_value) {
                            const enum_type curr_item = static_cast<enum_type>(curr_item_value);
                            const bool is_selected = (selected_item == curr_item);
                            if (ImGui::Selectable(item_names.find(curr_item)->second.c_str(), is_selected)) {
                                // Selection changed
                                render_config.view_layout = static_cast<enum_type>(curr_item_value);
                            }
                
                            if (is_selected) {
                                // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
                                ImGui::SetItemDefaultFocus();
                            }
                        } // for
                        ImGui::EndCombo();
                    }
                }

                ImGui::ColorEdit3("BG Color", render_config.bg_color.data());
                ImGui::Checkbox("Show Origin Axes", &render_config.show_origin_axis);
                ImGui::Checkbox("Show Origin XZ Plane", &render_config.show_origin_xz_plane);
                ImGui::Checkbox("Show Object Normals", &render_config.show_object_normals);
                ImGui::Checkbox("Show Wireframe", &render_config.show_wireframe);

                {
                    using enum_type = triengine::visualizer_window::skeleton_render_mode;
                    static const std::unordered_map<enum_type, std::string> item_names = {
                        { enum_type::skeleton_default, "skeleton_default" },
                        { enum_type::skeleton_overlay, "skeleton_overlay" },
                        { enum_type::overlay_with_joint_axis, "overlay_with_joint_axis" }
                    };

                    const enum_type selected_item = render_config.skeleton_mode;
                    if (ImGui::BeginCombo("Skeleton Render Mode", item_names.find(selected_item)->second.c_str())) {
                        for (int32_t curr_item_value = 0; curr_item_value < static_cast<int32_t>(item_names.size()); ++curr_item_value) {
                            const enum_type curr_item = static_cast<enum_type>(curr_item_value);
                            const bool is_selected = (selected_item == curr_item);
                            if (ImGui::Selectable(item_names.find(curr_item)->second.c_str(), is_selected)) {
                                // Selection changed
                                render_config.skeleton_mode = static_cast<triengine::visualizer_window::skeleton_render_mode>(curr_item_value);
                            }

                            if (is_selected) {
                                // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
                                ImGui::SetItemDefaultFocus();
                            }
                        } // for
                        ImGui::EndCombo();
                    }
                }

                if (ImGui::CollapsingHeader("Lighting Options"))
                {
                    if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto& dir_light_opts = render_config.light_opts.dir_light;
                        ImGui::Checkbox("Enabled##DirLight", &dir_light_opts.enabled);
                        ImGui::Checkbox("Use Blinn##DirLight", &dir_light_opts.use_blinn);
                        ImGui::Checkbox("Follow Camera##DirLight", &dir_light_opts.follow_camera);
                        ImGui::DragFloat3("Light Direction##DirLight",
                            dir_light_opts.direction.data(),
                            0.01f,
                            -1.0f, 1.0f,
                            "%.3f",
                            dir_light_opts.follow_camera ? ImGuiSliderFlags_ReadOnly : ImGuiSliderFlags_None
                        );
                        ImGui::ColorEdit3("Light Color##DirLight", dir_light_opts.color.data());
                        ImGui::SliderFloat("Ambient Intensity##DirLight", &dir_light_opts.ambientIntensity, 0.0f, 1.0f);
                        ImGui::SliderFloat("Diffuse Intensity##DirLight", &dir_light_opts.diffuseIntensity, 0.0f, 1.0f);
                        ImGui::SliderFloat("Specular Intensity##DirLight", &dir_light_opts.specularIntensity, 0.0f, 1.0f);
                    }

                    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto& point_light_opts = render_config.light_opts.point_light;
                        ImGui::Checkbox("Enabled##PointLight", &point_light_opts.enabled);
                        ImGui::Checkbox("Use Blinn##PointLight", &point_light_opts.use_blinn);
                        ImGui::Checkbox("Show Light Source##PointLight", &point_light_opts.show_light_source);
                        ImGui::DragFloat3("Light Position##PointLight", point_light_opts.position.data(), 0.05f, -FLT_MAX / INT_MAX, FLT_MAX / INT_MAX);
                        ImGui::ColorEdit3("Light Color##PointLight", point_light_opts.color.data());
                        ImGui::SliderFloat("Ambient Intensity##PointLight", &point_light_opts.ambientIntensity, 0.0f, 1.0f);
                        ImGui::SliderFloat("Diffuse Intensity##PointLight", &point_light_opts.diffuseIntensity, 0.0f, 1.0f);
                        ImGui::SliderFloat("Specular Intensity##PointLight", &point_light_opts.specularIntensity, 0.0f, 1.0f);
                    }
                }
            }

        }
        else
        {
            // ...
        }
    }

} // namespace