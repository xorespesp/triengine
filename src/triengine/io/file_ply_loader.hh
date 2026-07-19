#pragma once
#include <filesystem>
#include <functional>
#include <triengine/geometry/pcd_object.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/io/pointcloud_load_options.hh>

namespace triengine::io
{
    bool load_pointcloud_from_ply(
        const std::filesystem::path& file_path,
        geometry::pcd_object& pcd/* out */,
        const pointcloud_load_options& opts = {}
    );
    
} // namespace