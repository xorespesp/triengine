#pragma once
#include <filesystem>
#include <triengine/geometry/skeleton_object.hh>

namespace triengine::io
{
    bool load_skeleton_from_bvh(
        const std::filesystem::path& file_path,
        std::vector<geometry::skeleton_object>& frames /* out */
    );

} // namespace