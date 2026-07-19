#pragma once
#include <filesystem>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/scene.hh>

namespace triengine::io
{
    bool load_mesh_from_obj(
        const std::filesystem::path& file_path,
        bool apply_gamma_correction,
        scene& scn,
        geometry::mesh_object& mesh/* out */
    );

} // namespace