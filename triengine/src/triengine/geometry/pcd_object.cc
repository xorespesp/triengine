#include "pcd_object.hh"

#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/hash_utils.hh>
#include <unordered_map>

namespace triengine::geometry
{
    void pcd_object::clear()
    {
        points.clear();
        normals.clear();
        colors.clear();
    }

    void pcd_object::paint_uniform_color(
        const color3_f32& color) 
    {
        colors.clear();
        colors.resize(points.size(), color);
    }

    pcd_object& pcd_object::remove_duplicated_points()
    {
        const bool has_normals = this->has_normals();
        const bool has_colors = this->has_colors();

        const size_t old_points_size = points.size();
        size_t new_points_size = 0;

        std::unordered_map<
            Eigen::Vector3f/*point*/, 
            size_t/*index*/, 
            utility::hash_eigen<Eigen::Vector3f>
        > point_to_old_index;

        for (size_t i = 0; i < old_points_size; ++i)
        {
            if (const auto [_, success] = point_to_old_index.insert(
                    { points[i], i }
                ); success)
            {
                points[new_points_size] = points[i];
                if (has_normals) { normals[new_points_size] = normals[i]; }
                if (has_colors) { colors[new_points_size] = colors[i]; }
                ++new_points_size;
            }
        }
        
        points.resize(new_points_size);
        if (has_normals) { normals.resize(new_points_size); }
        if (has_colors) { colors.resize(new_points_size); }
    
        TRIENGINE_DEBUG("%lld points have been removed"
            , static_cast<int64_t>(old_points_size) - static_cast<int64_t>(new_points_size)
        );
    
        return *this;
    }

    pcd_object& pcd_object::remove_non_finite_points(
        const bool remove_nan_points,
        const bool remove_inf_points)
    {
        if (!remove_nan_points && !remove_inf_points) {
            TRIENGINE_WARN("Skipping removal of non-finite points: both NaN and Inf filtering are disabled.");
            return *this;
        }

        const bool has_normals = this->has_normals();
        const bool has_colors = this->has_colors();
        
        const size_t old_points_size = points.size();
        size_t new_points_size = 0;

        for (size_t i = 0; i < old_points_size; ++i)
        {
            bool is_valid{ false };
            if (remove_nan_points && remove_inf_points) {
                is_valid = points[i].array().isFinite().all();
            } else {
                is_valid = 
                    !(remove_nan_points && points[i].array().isNaN().any()) &&
                    !(remove_inf_points && points[i].array().isInf().any());
            }
            if (is_valid) {
                points[new_points_size] = points[i];
                if (has_normals) { normals[new_points_size] = normals[i]; }
                if (has_colors) { colors[new_points_size] = colors[i]; }
                ++new_points_size;
            }
        }
    
        points.resize(new_points_size);
        if (has_normals) { normals.resize(new_points_size); }
        if (has_colors) { colors.resize(new_points_size); }

        TRIENGINE_DEBUG("%lld non-finite points have been removed."
            , static_cast<int64_t>(old_points_size) - static_cast<int64_t>(new_points_size)
        );

        return *this;
    }

} // namespace