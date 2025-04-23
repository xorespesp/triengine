#pragma once
#include <filesystem>
#include <triengine/geometry/triangle_mesh_object.hh>

namespace triengine::io
{
    bool load_obj_file(
        const std::filesystem::path& file_path,
        geometry::triangle_mesh_object& mesh/* out */
    );

} // namespace