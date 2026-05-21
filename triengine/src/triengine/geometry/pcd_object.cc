#include "pcd_object.hh"

#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/hash_utils.hh>
#include <unordered_map>
#include <algorithm>
#include <limits>
#include <cmath>
#include <cstdint>

namespace triengine::geometry
{
    namespace
    {
        /// Maps a per-point scalar field to RGB colors through `cmap`, using the
        /// normalization strategy in `options`, writing the result into
        /// `out_colors` (resized to match `scalars`).
        void colorize_from_scalars(
            std::vector<color3_f32>& out_colors,
            const std::vector<float>& scalars,
            const utility::color_map& cmap,
            const colorize_options& options)
        {
            if (scalars.empty()) {
                out_colors.clear();
                return;
            }

            // Resolve the scalar range: use the explicit range from `options`.
            // if present, otherwise auto-derive it from the finite scalar values.
            const colorize_range value_range = [&options, &scalars]() -> colorize_range {
                if (options.range.has_value()) {
                    if (options.range->min > options.range->max) {
                        TRIENGINE_PANIC("colorize_options::range requires min <= max");
                    }
                    return *options.range;
                }

                // auto-derive it from the finite scalar values.
                float derived_min = std::numeric_limits<float>::max();
                float derived_max = std::numeric_limits<float>::lowest();
                bool any_finite = false;
                for (const float s : scalars) {
                    if (!std::isfinite(s)) { continue; }
                    derived_min = std::min(derived_min, s);
                    derived_max = std::max(derived_max, s);
                    any_finite = true;
                }
                if (!any_finite) { return { 0.0f, 0.0f }; }
                return { derived_min, derived_max };
            }();

            out_colors.resize(scalars.size());

            const float range_span = value_range.max - value_range.min;

            if (options.mapping == colorize_mapping::dynamic &&
                range_span > 0.0f)
            {
                const int bin_count = std::max(options.histogram_bins, 1);

                // Histogram equalization makes the color distribution adapt to the
                // data: value ranges where points are densely packed receive more
                // of the colormap, while sparse ranges receive less.
                //
                // The scalar values are arbitrary floats, so they cannot index a
                // histogram directly. Instead the value range is divided into
                // `bin_count` equal-width bins, and `bin_of()`
                // quantizes a scalar value to its bin index. The per-bin counts are
                // then accumulated into a cumulative distribution (CDF); each
                // point's normalized value `t` is the CDF evaluated at its bin
                // (`cdf[bin] / total`), which spreads `t` according to data density.
                std::vector<uint64_t> cdf(static_cast<size_t>(bin_count), 0);
                auto bin_of = [&](float s) -> int {
                    // Quantize the scalar `s` to a bin index within `[0, bin_count)`.
                    const float t = (s - value_range.min) / range_span;
                    const int bin = static_cast<int>(t * static_cast<float>(bin_count));
                    return std::clamp(bin, 0, bin_count - 1);
                };

                uint64_t total = 0;
                for (const float s : scalars) {
                    if (!std::isfinite(s)) { continue; }
                    ++cdf[static_cast<size_t>(bin_of(s))];
                    ++total;
                }
                for (size_t i = 1; i < cdf.size(); ++i) { cdf[i] += cdf[i - 1]; }

                for (size_t i = 0; i < scalars.size(); ++i) {
                    float t = 0.0f;
                    if (total > 0 && std::isfinite(scalars[i])) {
                        t = static_cast<float>(cdf[static_cast<size_t>(bin_of(scalars[i]))]) /
                            static_cast<float>(total);
                    }
                    out_colors[i] = cmap.at(t);
                }
            }
            else
            {
                // Linear min-max mapping (also the fallback when the range is empty).
                for (size_t i = 0; i < scalars.size(); ++i) {
                    float t = 0.0f;
                    if (range_span > 0.0f && std::isfinite(scalars[i])) {
                        t = std::clamp((scalars[i] - value_range.min) / range_span, 0.0f, 1.0f);
                    }
                    out_colors[i] = cmap.at(t);
                }
            }
        }

    } // namespace

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

    void pcd_object::colorize_by_axis(
        const colorize_axis axis,
        const utility::color_map& color_map,
        const colorize_options& options)
    {
        // `colorize_axis` enumerators map directly to coordinate indices (x=0, y=1, z=2).
        const int axis_index = static_cast<int>(axis);

        std::vector<float> scalars(points.size());
        for (size_t i = 0; i < points.size(); ++i) {
            scalars[i] = points[i][axis_index];
        }

        colorize_from_scalars(colors, scalars, color_map, options);
        this->mark_dirty();
    }

    void pcd_object::colorize_by_distance(
        const vec3_f32& reference_point,
        const utility::color_map& color_map,
        const colorize_options& options)
    {
        std::vector<float> scalars(points.size());
        for (size_t i = 0; i < points.size(); ++i) {
            scalars[i] = (points[i] - reference_point).norm();
        }

        colorize_from_scalars(colors, scalars, color_map, options);
        this->mark_dirty();
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