#pragma once
#include <filesystem>
#include <triengine/geometry/triangle_mesh_object.hh>

namespace triengine::io
{
    bool load_triangle_mesh_from_obj(
        const std::filesystem::path& file_path,
        bool apply_gamma_correction,
        geometry::triangle_mesh_object& mesh/* out */
    );

} // namespace