#include "geometry_object_base.hh"

#include <triengine/utility/string_format.hh>
#include <algorithm>
#include <numeric>
#include <atomic>
#include <string_view>

namespace triengine::geometry
{
    namespace {
        static geometry_object_id_t _create_unique_object_id() {
            static std::atomic<geometry_object_id_t> cnt_ = 0;
            return cnt_++; // TODO: overflow check?
        }
    } // namespace

    geometry_object_base::geometry_object_base(geometry_object_type type)
        : _id{ _create_unique_object_id() }
        , _type{ type }
    {
        const std::string type_str = [type]() -> std::string {
            switch (type) {
            case geometry_object_type::mesh: return "mesh";
            case geometry_object_type::pointcloud: return "pointcloud";
            case geometry_object_type::lineset: return "lineset";
            case geometry_object_type::skeleton: return "skeleton";
            default: return utility::string::c_format("unknown_%d", static_cast<int>(type));
            }
        }();

        this->set_name(utility::string::c_format(
            "object_%s_#%llX"
            , type_str.c_str()
            , this->get_id()
        ));
    }

    vec3_f32 geometry_object_base::compute_min_bound(
        const std::vector<vec3_f32>& points) const
    {
        if (points.empty()) {
            return math::vec3_all(0.0f);
        }

        vec3_f64 min_bound_f64 = std::accumulate(
            points.begin() + 1, // Start from the second element
            points.end(),
            points[0].cast<double>().eval(), // Initial value is first element (cast as f64 precision)
            [](vec3_f64 curr_min_f64, const vec3_f32& point_f32) {
                // Cast the float point to double and perform min comparison
                return curr_min_f64.array().min(point_f32.cast<double>().array()).matrix();
            });

        // Cast the final f64 precision result back to f32
        return min_bound_f64.cast<float>();
    }
    
    vec3_f32 geometry_object_base::compute_max_bound(
        const std::vector<vec3_f32>& points) const
    {
        if (points.empty()) {
            return math::vec3_all(0.0f);
        }
        
        vec3_f64 max_bound_d = std::accumulate(
            points.begin() + 1, // Start from the second element
            points.end(),
            points[0].cast<double>().eval(), // Initial value is first element (cast as f64 precision)
            [](vec3_f64 curr_max_f64, const vec3_f32& point_f32) {
                // Cast the float point to double and perform max comparison
                return curr_max_f64.array().max(point_f32.cast<double>().array()).matrix();
            });

        // Cast the final f64 precision result back to f32
        return max_bound_d.cast<float>();
    }
    
    vec3_f32 geometry_object_base::compute_center(
        const std::vector<vec3_f32>& points) const
    {
        if (points.empty()) {
            return math::vec3_all(0.0f);
        }

        // Use `std::accumulate` for summation, operating in f64 precision.
        vec3_f64 center_f64 = std::accumulate(
            points.begin(),
            points.end(),
            math::vec3_all(0.0), // Initial value is f64 zero vector
            [](vec3_f64 curr_sum_f64, const vec3_f32& point_f32) {
                return (curr_sum_f64 + point_f32.cast<double>()).eval();
            });

        center_f64 /= static_cast<double>(points.size());

        // Cast the final f64 precision result back to f32
        return center_f64.cast<float>();
    }

} // namespace