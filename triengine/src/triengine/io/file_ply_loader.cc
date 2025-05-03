#include "file_ply_loader.hh"

#include <triengine/utility/bit_cast.hh>
#include <triengine/utility/progress_reporter.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/extern/rply/rply.h>

namespace triengine::io
{
    bool load_pointcloud_from_ply(
        const std::filesystem::path& file_path,
        geometry::pcd_object& pcd/* out */,
        const pointcloud_load_options& opts)
    {
        std::shared_ptr<std::remove_pointer_t<p_ply>> ply_file{
            ::ply_open(file_path.string().c_str(), nullptr, 0, nullptr),
            ::ply_close
        };
        
        if (!ply_file) {
            TRIENGINE_TRACE("Failed to open file: %s", file_path.string().c_str());
            return false;
        }

        if (!::ply_read_header(ply_file.get())) {
            TRIENGINE_TRACE("Failed to parse PLY header");
            return false;
        }

        struct read_state_t {
            utility::progress_reporter* reporter{ nullptr };
            geometry::pcd_object* out_pcd{ nullptr };
            long vertex_index{}, vertex_num{};
            long normal_index{}, normal_num{};
            long color_index{},  color_num{};
        };
        
        static const auto read_vertex_cb_ = 
            +[](p_ply_argument argument) -> int
            {
                read_state_t* pstate{ nullptr };
                long rd_index{};
                ::ply_get_argument_user_data(argument, utility::bit_cast<void**>(&pstate), &rd_index);
                if (pstate->vertex_index >= pstate->vertex_num) {
                    return 0;  // some sanity check
                }
                
                double value = ::ply_get_argument_value(argument);
                pstate->out_pcd->points[pstate->vertex_index](rd_index) = value;
                if (rd_index == 2) { // reading 'z' ?
                    pstate->vertex_index++;
                    if (pstate->vertex_index % 1000 == 0) {
                        pstate->reporter->update(pstate->vertex_index);
                    }
                }
                return 1;
            };
        
        static const auto read_normal_cb_ = 
            +[](p_ply_argument argument) -> int
            {
                read_state_t* pstate{ nullptr };
                long rd_index{};
                ::ply_get_argument_user_data(argument, utility::bit_cast<void**>(&pstate), &rd_index);
                if (pstate->normal_index >= pstate->normal_num) {
                    return 0;
                }
                
                double value = ::ply_get_argument_value(argument);
                pstate->out_pcd->normals[pstate->normal_index](rd_index) = value;
                if (rd_index == 2) { // reading 'nz' ?
                    pstate->normal_index++;
                }
                return 1;
            };
        
        static const auto read_color_cb_ = 
            +[](p_ply_argument argument) -> int
            {
                read_state_t* pstate{ nullptr };
                long rd_index{};
                ::ply_get_argument_user_data(argument, utility::bit_cast<void **>(&pstate), &rd_index);
                if (pstate->color_index >= pstate->color_num) {
                    return 0;
                }
                
                double value = ::ply_get_argument_value(argument);
                pstate->out_pcd->colors[pstate->color_index](rd_index) = value / 255.0;
                if (rd_index == 2) { // reading 'blue' ?
                    pstate->color_index++;
                }
                return 1;
            };
        
        read_state_t rd_state{};
        rd_state.out_pcd = &pcd;

        rd_state.vertex_num = ::ply_set_read_cb(ply_file.get(), "vertex", "x", read_vertex_cb_, &rd_state, 0/*rd_index*/);
        ::ply_set_read_cb(ply_file.get(), "vertex", "y", read_vertex_cb_, &rd_state, 1/*rd_index*/);
        ::ply_set_read_cb(ply_file.get(), "vertex", "z", read_vertex_cb_, &rd_state, 2/*rd_index*/);

        rd_state.normal_num = ::ply_set_read_cb(ply_file.get(), "vertex", "nx", read_normal_cb_, &rd_state, 0/*rd_index*/);
        ::ply_set_read_cb(ply_file.get(), "vertex", "ny", read_normal_cb_, &rd_state, 1/*rd_index*/);
        ::ply_set_read_cb(ply_file.get(), "vertex", "nz", read_normal_cb_, &rd_state, 2/*rd_index*/);

        rd_state.color_num = ::ply_set_read_cb(ply_file.get(), "vertex", "red", read_color_cb_, &rd_state, 0/*rd_index*/);
        ::ply_set_read_cb(ply_file.get(), "vertex", "green", read_color_cb_, &rd_state, 1/*rd_index*/);
        ::ply_set_read_cb(ply_file.get(), "vertex", "blue", read_color_cb_, &rd_state, 2/*rd_index*/);

        if (rd_state.vertex_num <= 0) {
            TRIENGINE_TRACE("Unexpected vertex number: %ld", rd_state.vertex_num);
            return false;
        }

        pcd.clear();
        pcd.points.resize(rd_state.vertex_num);
        pcd.normals.resize(rd_state.normal_num);
        pcd.colors.resize(rd_state.color_num);

        utility::progress_reporter reporter{ static_cast<uint64_t>(rd_state.vertex_num) };
        reporter.set_progress_cb(opts.progress_cb);
        rd_state.reporter = &reporter;

        if (!::ply_read(ply_file.get())) {
            TRIENGINE_TRACE("Failed to read file: %s", file_path.string().c_str());
            return false;
        }

        TRIENGINE_TRACE("Read {} pointcloud vertices.", pcd.points.size());

        if (opts.remove_nan_points || opts.remove_inf_points) {
            pcd.remove_non_finite_points(
                opts.remove_nan_points, 
                opts.remove_inf_points
            );
        }

        reporter.finish();
        return true;
    }

} // namespace