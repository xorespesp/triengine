#pragma once
#include <functional>

namespace triengine::io
{
    struct pointcloud_load_options
    {
        pointcloud_load_options(
            bool remove_nan_points_ = false,
            bool remove_inf_points_ = false,
            bool print_progress_ = false,
            std::function<bool(float)> progress_cb_ = {})
            : remove_nan_points{ remove_nan_points_ }
            , remove_inf_points{ remove_inf_points_ }
            , print_progress{ print_progress_ }
            , progress_cb{ std::move(progress_cb_) }
        {}

        pointcloud_load_options(
            std::function<bool(float)> progress_cb_)
            : pointcloud_load_options()
        {
            progress_cb = std::move(progress_cb_);
        }

        /// Whether to remove all points that have nan
        bool remove_nan_points;

        /// Whether to remove all points that have +-inf
        bool remove_inf_points;

        /// Print progress to stdout about loading progress.
        bool print_progress;

        /// Callback to invoke as reading is progressing. 
        /// parameter is percentage completion (0.0~100.0). 
        /// return `true` indicates to continue loading,
        /// `false` means to try to stop loading and cleanup
        std::function<bool(float)> progress_cb;
    };

} // namespace