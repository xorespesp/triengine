#pragma once
#include <filesystem>
#include "../geometry/skeleton_object.hh"

namespace triengine::io
{
    bool load_obj_file(
        const std::filesystem::path& file_path,
        std::vector<geometry::skeleton_object>& frames /* out */
    );

} // namespace