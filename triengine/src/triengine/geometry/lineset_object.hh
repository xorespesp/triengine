#pragma once
#include <triengine/geometry/geometry_object_base.hh>

#include <vector>
#include <memory>

namespace triengine::geometry
{
    // Refs:
    // https://github.com/isl-org/Open3D/blob/main/cpp/open3d/geometry/LineSet.h

    class lineset_object
        : public geometry_object_base
    {
    public:
        std::vector<vec3_f32> line_points; /// Points coordinates. (line vertices)
        std::vector<vec2_i32> line_indices;  /// Lines denoted by the index of points forming the line. (line indices)
        std::vector<color3_f32> line_colors; /// RGB colors of lines.

    private:
        std::shared_ptr<geometry_object_base> clone_impl() const override {
            auto cloned = std::make_shared<lineset_object>();
            cloned->line_points = this->line_points;
            cloned->line_indices = this->line_indices;
            cloned->line_colors = this->line_colors;
            return cloned;
        }

    public:
        lineset_object()
            : geometry_object_base{ geometry_object_type::lineset }
        {}

        std::shared_ptr<lineset_object> clone() const {
            return std::static_pointer_cast<lineset_object>(this->clone_impl());
        }

        void paint_uniform_color(const color3_f32& color) {
            line_colors.clear();
            line_colors.resize(line_points.size(), color);
        }

        void apply_model_in_place() override {
            TRIENGINE_PANIC("not implemented");
        }

        vec3_f32 get_min_bound() const override {
            return this->compute_min_bound(line_points);
        }

        vec3_f32 get_max_bound() const override {
            return this->compute_max_bound(line_points);
        }
        
        vec3_f32 get_center() const override {
            return this->compute_center(line_points);
        }

    public:
        static void create_xz_plane(
            lineset_object& object/* out */,
            const float plane_size/* width and height; Unit: [m] */,
            const int num_of_grids
        );

        static std::shared_ptr<lineset_object> create_xz_plane(
            const float plane_size/* width and height; Unit: [m] */,
            const int num_of_grids)
        {
            auto new_object = std::make_shared<lineset_object>();
            create_xz_plane(*new_object, plane_size, num_of_grids);
            return new_object;
        }

        static void create_cube(
            lineset_object& object/* out */,
            const vec3_f32& cube_center,
            const vec3_f32& cube_size
        );

        static std::shared_ptr<lineset_object> create_cube(
            const vec3_f32& cube_center,
            const vec3_f32& cube_size)
        {
            auto new_object = std::make_shared<lineset_object>();
            create_cube(*new_object, cube_center, cube_size);
            return new_object;
        }

    }; // class

} // namespace