#pragma once
#include <triengine/geometry/geometry_object_base.hh>

#include <vector>
#include <memory>

namespace triengine::geometry
{
    class pcd_object
        : public geometry_object_base
    {
    public:
        struct material_t
        {
            float ambient{ 1.0f }; /// ambient intensity; [0.0...1.0]
            float diffuse{ 1.0f }; /// diffuse intensity; [0.0...1.0]
            float specular{ 0.3f }; /// specular intensity; [0.0...1.0]
            uint16_t shininess{ 128 }; /// surface shininess scalar (must be `> 0`)
            float alpha{ 1.0f }; /// object transparency (WBOIT); [0.0...1.0]

            material_t() = default;

            bool is_valid() const noexcept {
                return shininess > 0;
            }
        };

    public:
        std::vector<vec3_f32> points; /// The position of the point cloud vertices (Unit: [m])
        std::vector<vec3_f32> normals; /// normal vectors
        std::vector<color3_f32> colors; /// normalized RGB
        material_t material;

    public:
        pcd_object()
            : geometry_object_base{ geometry_object_type::pointcloud }
        {}

        bool is_opaque() const noexcept {
            return std::fabs(1.0f - std::clamp(material.alpha, 0.0f, 1.0f)) < std::numeric_limits<float>::epsilon();
        }

        /// Returns `true` if the point cloud contains point normals.
        bool has_normals() const noexcept {
            return !points.empty() && points.size() == normals.size();
        }

        /// Returns `true` if the point cloud contains point colors.
        bool has_colors() const noexcept {
            return !points.empty() && points.size() == colors.size();
        }

        void clear();

        void paint_uniform_color(const color3_f32& color);

        pcd_object& remove_duplicated_points();

        pcd_object& remove_non_finite_points(
            bool remove_nan_points = true,
            bool remove_inf_points = true
        );

    public:
        static std::shared_ptr<pcd_object> create() {
            return std::make_shared<pcd_object>();
        }

    }; // class

} // namespace