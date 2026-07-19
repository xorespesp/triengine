#include "color_map.hh"

#include <algorithm>
#include <cstdint>

namespace triengine::utility
{
    namespace
    {
        /// Converts an 8-bit per-channel RGB triple to a normalized `color3_f32`.
        inline color3_f32 rgb8(uint8_t r, uint8_t g, uint8_t b)
        {
            constexpr float kInv255{ 1.0f / 255.0f };
            return color3_f32{
                static_cast<float>(r) * kInv255,
                static_cast<float>(g) * kInv255,
                static_cast<float>(b) * kInv255,
            };
        }

        /// Interpolates `control_colors` at normalized position `t` using
        /// piecewise linear interpolation between evenly spaced control colors.
        inline color3_f32 interpolate_control_colors(const std::vector<color3_f32>& control_colors, float t)
        {
            if (control_colors.empty()) { return color3_f32::zero(); }
            if (control_colors.size() == 1) { return control_colors.front(); }
            t = std::clamp(t, 0.0f, 1.0f);

            const float pos = t * static_cast<float>(control_colors.size() - 1);
            const int lower = static_cast<int>(pos);
            const int upper = std::min(lower + 1, static_cast<int>(control_colors.size()) - 1);
            const float frac = pos - static_cast<float>(lower);

            const color3_f32& a = control_colors[static_cast<size_t>(lower)];
            const color3_f32& b = control_colors[static_cast<size_t>(upper)];

            return color3_f32{
                a.r() + (b.r() - a.r()) * frac,
                a.g() + (b.g() - a.g()) * frac,
                a.b() + (b.b() - a.b()) * frac,
            };
        }

    } // namespace

    color_map::color_map(const std::vector<color3_f32>& control_colors, size_t lut_size)
    {
        if (lut_size == 0) { lut_size = 1; }

        _lut.resize(lut_size);

        if (lut_size == 1) {
            _lut[0] = control_colors.empty() ? color3_f32::zero() : control_colors.front();
            return;
        }

        for (size_t i = 0; i < lut_size; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(lut_size - 1);
            _lut[i] = interpolate_control_colors(control_colors, t);
        }
    }

    color3_f32 color_map::at(float t) const
    {
        if (_lut.empty()) { return color3_f32::zero(); }

        t = std::clamp(t, 0.0f, 1.0f);
        const size_t idx = static_cast<size_t>(t * static_cast<float>(_lut.size() - 1));
        return _lut[idx];
    }

    namespace color_map_presets
    {
        const color_map& jet()
        {
            static const color_map map{ {
                rgb8(0, 0, 255), rgb8(0, 255, 255), rgb8(255, 255, 0),
                rgb8(255, 0, 0), rgb8(50, 0, 0),
            } };
            return map;
        }

        const color_map& classic()
        {
            static const color_map map{ {
                rgb8(30, 77, 203), rgb8(25, 60, 192), rgb8(45, 117, 220),
                rgb8(204, 108, 191), rgb8(196, 57, 178), rgb8(198, 33, 24),
            } };
            return map;
        }

        const color_map& hue()
        {
            static const color_map map{ {
                rgb8(255, 0, 0), rgb8(255, 255, 0), rgb8(0, 255, 0),
                rgb8(0, 255, 255), rgb8(0, 0, 255), rgb8(255, 0, 255),
                rgb8(255, 0, 0),
            } };
            return map;
        }

        const color_map& grayscale()
        {
            static const color_map map{ {
                rgb8(255, 255, 255), rgb8(0, 0, 0),
            } };
            return map;
        }

        const color_map& inv_grayscale()
        {
            static const color_map map{ {
                rgb8(0, 0, 0), rgb8(255, 255, 255),
            } };
            return map;
        }

        const color_map& biomes()
        {
            static const color_map map{ {
                rgb8(0, 0, 204), rgb8(204, 230, 255), rgb8(255, 255, 153),
                rgb8(170, 255, 128), rgb8(0, 153, 0), rgb8(230, 242, 255),
            } };
            return map;
        }

        const color_map& cold()
        {
            static const color_map map{ {
                rgb8(230, 247, 255), rgb8(0, 92, 230), rgb8(0, 179, 179),
                rgb8(0, 51, 153), rgb8(0, 5, 15),
            } };
            return map;
        }

        const color_map& warm()
        {
            static const color_map map{ {
                rgb8(255, 255, 230), rgb8(255, 204, 0), rgb8(255, 136, 77),
                rgb8(255, 51, 0), rgb8(128, 0, 0), rgb8(10, 0, 0),
            } };
            return map;
        }

    } // namespace color_map_presets

} // namespace triengine::utility
