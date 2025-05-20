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

        std::unordered_map<
            Eigen::Vector3f/*point*/, 
            size_t/*old index*/, 
            utility::hash_eigen<Eigen::Vector3f>
        > point_2_old_idx_map;

        size_t new_idx = 0;
        for (size_t old_idx = 0; old_idx < old_points_size; ++old_idx)
        {
            const auto [
                _, 
                inserted
            ] = point_2_old_idx_map.insert({ points[old_idx], old_idx });

            if (inserted) {
                points[new_idx] = points[old_idx];
                if (has_normals) { normals[new_idx] = normals[old_idx]; }
                if (has_colors) { colors[new_idx] = colors[old_idx]; }
                ++new_idx;
            }
        }

        const size_t new_points_size = new_idx;
        
        points.resize(new_points_size);
        if (has_normals) { normals.resize(new_points_size); }
        if (has_colors) { colors.resize(new_points_size); }
    
        TRIENGINE_DEBUG("%s() : %lld points have been removed"
            , __func__
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

        size_t new_idx = 0;
        for (size_t old_idx = 0; old_idx < old_points_size; ++old_idx)
        {
            bool is_valid{ false };
            if (remove_nan_points && remove_inf_points) {
                is_valid = points[old_idx].array().isFinite().all();
            } else {
                is_valid = !(remove_nan_points && points[old_idx].array().isNaN().any()) && 
                           !(remove_inf_points && points[old_idx].array().isInf().any());
            }
            if (is_valid) {
                points[new_idx] = points[old_idx];
                if (has_normals) { normals[new_idx] = normals[old_idx]; }
                if (has_colors) { colors[new_idx] = colors[old_idx]; }
                ++new_idx;
            }
        }

        const size_t new_points_size = new_idx;

        points.resize(new_points_size);
        if (has_normals) { normals.resize(new_points_size); }
        if (has_colors) { colors.resize(new_points_size); }

        TRIENGINE_DEBUG("%s() : %lld non-finite points have been removed."
            , __func__
            , static_cast<int64_t>(old_points_size) - static_cast<int64_t>(new_points_size)
        );

        return *this;
    }

} // namespace