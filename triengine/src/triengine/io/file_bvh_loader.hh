#pragma once
#include <triengine/common.h>
#include <triengine/geometry/skeleton_object.hh>

#include <unordered_map>
#include <vector>
#include <filesystem>

namespace triengine::io
{
    using bvh_joint_id_t = size_t/* index */;

    struct bvh_joint_info_t
    {
        std::string name;
        double length{ 0.0 };
        std::string bvh_euler_axis_order;
    };

    struct bvh_joint_data_t
    {
        bvh_joint_id_t joint_id{};
        Eigen::Vector3d bvh_euler_angels{}; // Unit: [deg]
        Eigen::Vector3d world_position{};
        Eigen::Matrix3d world_rotation{};
    };

    struct bvh_motion_frame_t
    {
        std::unordered_map<bvh_joint_id_t, bvh_joint_data_t> skeleton;
    };

    struct bvh_file_t
    {
        std::vector<bvh_joint_info_t> joints; // index == bvh joint id
        std::unordered_map<bvh_joint_id_t/* child */, bvh_joint_id_t/* parent */> joints_parent_map;
        bvh_joint_id_t root_joint_id{};
        double frame_time{ 0.0 }; // FPS = 1 / frame_time
        std::vector<bvh_motion_frame_t> frames;
        //bvh_motion_frame_t t_pose_frame;
    };

    bool load_skeleton_from_bvh(
        const std::filesystem::path& file_path,
        bvh_file_t& bvh_file
    );

} // namespace