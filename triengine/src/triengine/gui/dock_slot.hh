#pragma once

namespace triengine::gui
{
    enum class dock_slot {
        floating,
        left,
        right,
        top,
        bottom,
    };

    // Ratio of each side dock node relative to the parent region at the moment it is carved
    // out (see gui_manager::setup_dock_space for the sequential splitting order). Each value
    // must be in the open range (0, 1); out-of-range values are clamped on apply.
    struct dock_split_ratios {
        float left   = 0.22f;
        float right  = 0.25f;
        float top    = 0.20f;
        float bottom = 0.25f;
    };

} // namespace
