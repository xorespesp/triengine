#pragma once
#include <triengine/geometry/geometry_object_base.hh>

#include <vector>
#include <memory>

namespace triengine::geometry
{
    class light_source_object
        : public geometry_object_base
    {
    public:
        std::vector<vec3_f32> vertex_positions; /// List of triangle vertex positions (3 * num_triangles)
        std::vector<vec3_f32> vertex_normals;   /// List of triangle vertex normals (3 * num_triangles)
        std::vector<vec3_i32> triangle_indices; /// List of triangles denoted by the index of points forming the triangle. (num_triangles)
		color3_f32 color; /// uniform color of object

    public:
        light_source_object()
            : geometry_object_base{ geometry_object_type::light_source }
        {}

        void clear() {
            vertex_positions.clear();
            vertex_normals.clear();
            triangle_indices.clear();
        }

    public:
        static std::shared_ptr<light_source_object> create(
            float radius = 1.0f
        );

    }; // class

} // namespace