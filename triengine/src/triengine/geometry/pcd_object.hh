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
            float ambient_intensity{ 1.0f }; /// ambient intensity; must be `>= 0`
            float diffuse_intensity{ 1.0f }; /// diffuse intensity; must be `>= 0`
            float specular_intensity{ 0.25f }; /// specular intensity; must be `>= 0`
            uint16_t shininess{ 128 }; /// object surface shininess scalar (must be `> 0`)
            float alpha{ 1.0f }; /// object transparency (WBOIT); must be `[0.0...1.0]`

            material_t() = default;

            bool is_valid() const noexcept {
                return
                    0.0f <= ambient_intensity &&
                    0.0f <= diffuse_intensity &&
                    0.0f <= specular_intensity &&
                    0 < shininess &&
                    0.0f <= alpha && alpha <= 1.0f;
            }
        };

    public:
        std::vector<vec3_f32> points; /// The position of the point cloud vertices (Unit: [m])
        std::vector<vec3_f32> normals; /// normal vectors
        std::vector<color3_f32> colors; /// normalized RGB
        material_t material;

    private:
        std::shared_ptr<geometry_object_base> clone_impl() const override {
            auto cloned = std::make_shared<pcd_object>();
            cloned->points = this->points;
            cloned->normals = this->normals;
            cloned->colors = this->colors;
            cloned->material = this->material;
            return cloned;
        }

    public:
        pcd_object()
            : geometry_object_base{ geometry_object_type::pointcloud }
        {}

        std::shared_ptr<pcd_object> clone() const {
            return std::static_pointer_cast<pcd_object>(this->clone_impl());
        }

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

        void apply_model_in_place() override {
            TRIENGINE_PANIC("not implemented");
        }

        vec3_f32 get_min_bound() const override {
            return this->compute_min_bound(points);
        }

        vec3_f32 get_max_bound() const override {
            return this->compute_max_bound(points);
        }
        
        vec3_f32 get_center() const override {
            return this->compute_center(points);
        }

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