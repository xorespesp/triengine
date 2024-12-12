#pragma once
#include "geometry_object_base.hh"

#include <vector>
#include <memory>

namespace triengine::geometry
{
    // Refs: 
    // https://github.com/isl-org/Open3D/blob/db00e339c1645440dea6951c2971ffa759934112/cpp/open3d/geometry/PointCloud.h

    class pcd_object
        : public geometry_object_base
    {
    public:
        std::vector<vec3_f32> points; /// The position of the point cloud vertices (Unit: [m])
        std::vector<vec3_f32> normals; /// normal vectors
        std::vector<color3_f32> colors; /// normalized RGB

    public:
        pcd_object()
            : geometry_object_base{ geometry_object_type::pointcloud }
        {}

        void paint_uniform_color(const color3_f32& color) {
            colors.resize(points.size(), color);
        }

        void clear() {
            points.clear();
            normals.clear();
            colors.clear();
        }

    public:
        static std::shared_ptr<pcd_object> create() {
            return std::make_shared<pcd_object>();
        }

    }; // class

} // namespace