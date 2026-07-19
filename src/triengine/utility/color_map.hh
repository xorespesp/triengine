#pragma once
#include <triengine/basic_types.hh>

#include <vector>

namespace triengine::utility
{
    /// 1D color lookup table(LUT). Maps a normalized scalar in `[0,1]` to an RGB color
    /// by interpolating between evenly spaced control colors.
    class color_map {
    public:
        /// Builds a color map from a list of control colors.
        ///
        /// `control_colors` are the key colors that define the gradient. They are
        /// placed at evenly spaced positions over `[0,1]` (the first at `0`, the
        /// last at `1`), and the colors in between are produced by piecewise
        /// linear interpolation.
        ///
        /// The constructor samples this gradient at `lut_size` evenly spaced
        /// positions and stores the resulting colors in an internal lookup table.
        /// `at()` then only indexes this precomputed table instead of
        /// interpolating on every call. A larger `lut_size` yields a finer,
        /// smoother gradient at the cost of memory; the default is sufficient
        /// for typical visualization use.
        ///
        /// At least one control color is required; an empty list yields an
        /// all-black map.
        explicit color_map(
            const std::vector<color3_f32>& control_colors,
            size_t lut_size = 4000
        );

        /// Number of entries in the precomputed lookup table.
        size_t lut_size() const noexcept { return _lut.size(); }

        /// Maps a normalized value `t` (clamped to `[0,1]`) to an RGB color by
        /// selecting the nearest LUT entry.
        color3_f32 at(float t) const;

    private:
        std::vector<color3_f32> _lut;
    };

    /// Predefined color map presets.
    namespace color_map_presets
    {
        const color_map& jet();
        const color_map& classic();
        const color_map& hue();
        const color_map& grayscale();
        const color_map& inv_grayscale();
        const color_map& biomes();
        const color_map& cold();
        const color_map& warm();

    } // namespace color_map_presets

} // namespace triengine::utility
