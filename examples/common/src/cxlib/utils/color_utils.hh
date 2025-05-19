#pragma once
#include <cxlib/cxlib_defs.h>
#include <cxlib/utils/string_utils.hh>

#include <type_traits>
#include <thread>
#include <chrono>
#include <random>
#include <cmath>
#include <string>
#include <stdexcept>

_CXLIB_NAMESPACE_BEGIN
namespace utils
{
    namespace {
        namespace detail
        {
            constexpr double fmap(
                const double x,
                const double in_min, const double in_max,
                const double out_min, const double out_max)
            {
                return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
            }

            constexpr float fmapf(
                const float x,
                const float in_min, const float in_max,
                const float out_min, const float out_max)
            {
                return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
            }

            struct random_engine {
            private:
                std::mt19937_64 _rd;
                std::uniform_real_distribution<double> _gen;

            public:
                random_engine(double min, double max)
                    : _rd{ this->gen_seed() }
                    , _gen{ min, max }
                {}

                double get_value() { return _gen(_rd); }

            private:
                uint64_t gen_seed() const {
                    const auto time_since_epoch = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()
                        ).count());
                    const auto tid_hash = static_cast<uint64_t>(
                        std::hash<std::thread::id>()(std::this_thread::get_id()));
                    return (tid_hash ^ time_since_epoch);
                }
            };

            // std::type_identity (since C++20)
            // https://en.cppreference.com/w/cpp/types/type_identity
            template<class T> struct type_identity { using type = T; };
            template<class T> using type_identity_t = typename type_identity<T>::type;

        } // namespace detail
    } // namespace

    class rgb_color_t final
    {
        double 
            _r_val, // ∈ [0, 1]
            _g_val, // ∈ [0, 1]
            _b_val; // ∈ [0, 1]

    public:
        constexpr rgb_color_t() noexcept
            : _r_val{ 0.0 }, _g_val{ 0.0 }, _b_val{ 0.0 }
        { }

        rgb_color_t(double r_, double g_, double b_) {
            this->set_r(r_); this->set_g(g_); this->set_b(b_);
        }

        inline void set_r(double r_) {
            if (!(0.0 <= r_ && r_ <= 1.0)) { throw std::invalid_argument{ "invalid R value" }; }
            _r_val = r_;
        }

        inline void set_g(double g_) {
            if (!(0.0 <= g_ && g_ <= 1.0)) { throw std::invalid_argument{ "invalid G value" }; }
            _g_val = g_;
        }

        inline void set_b(double b_) {
            if (!(0.0 <= b_ && b_ <= 1.0)) { throw std::invalid_argument{ "invalid B value" }; }
            _b_val = b_;
        }

        constexpr double r() const noexcept { return _r_val; }
        constexpr double g() const noexcept { return _g_val; }
        constexpr double b() const noexcept { return _b_val; }

        template<typename _Ty>
        constexpr _Ty cast() const {
            // https://stackoverflow.com/a/5513109
            return this->_cast_impl(detail::type_identity<_Ty>{});
        }

    private:
        inline std::string _cast_impl(detail::type_identity<std::string>) const {
            return _CXLIB utils::string::c_format("rgb(%3d,%3d,%3d)"
                , static_cast<int>(detail::fmap(_r_val, 0.0, 1.0, static_cast<double>(0x00), static_cast<double>(0xFF)))
                , static_cast<int>(detail::fmap(_g_val, 0.0, 1.0, static_cast<double>(0x00), static_cast<double>(0xFF)))
                , static_cast<int>(detail::fmap(_b_val, 0.0, 1.0, static_cast<double>(0x00), static_cast<double>(0xFF)))
            );
        }

#if defined (_WINGDI_)
        constexpr ::COLORREF _cast_impl(detail::type_identity<::COLORREF>) const {
            return RGB(
                static_cast<uint8_t>(detail::fmap(_r_val, 0.0, 1.0, static_cast<double>(0x00), static_cast<double>(0xFF))),
                static_cast<uint8_t>(detail::fmap(_g_val, 0.0, 1.0, static_cast<double>(0x00), static_cast<double>(0xFF))),
                static_cast<uint8_t>(detail::fmap(_b_val, 0.0, 1.0, static_cast<double>(0x00), static_cast<double>(0xFF)))
            );
        }
#endif // ^^^ _WINGDI_ ^^^

    }; // class

    class hsv_color_t final
    {
        double
            _h_val, // ∈ [0, 360]
            _s_val, // ∈ [0, 1]
            _v_val; // ∈ [0, 1]

    public:
        constexpr hsv_color_t() noexcept
            : _h_val{ 0.0 }, _s_val{ 0.0 }, _v_val{ 0.0 }
        { }

        hsv_color_t(double h_, double s_, double v_) {
            this->set_h(h_); this->set_s(s_); this->set_v(v_);
        }

        inline void set_h(double h_) {
            if (!(0.0 <= h_ && h_ <= 360.0)) { throw std::invalid_argument{ "invalid H value" }; }
            _h_val = h_;
        }

        inline void set_s(double s_) {
            if (!(0.0 <= s_ && s_ <= 1.0)) { throw std::invalid_argument{ "invalid S value" }; }
            _s_val = s_;
        }

        inline void set_v(double v_) {
            if (!(0.0 <= v_ && v_ <= 1.0)) { throw std::invalid_argument{ "invalid V value" }; }
            _v_val = v_;
        }

        constexpr double h() const noexcept { return _h_val; }
        constexpr double s() const noexcept { return _s_val; }
        constexpr double v() const noexcept { return _v_val; }

        template<typename _Ty>
        constexpr _Ty cast() const {
            // https://stackoverflow.com/a/5513109
            return this->_cast_impl(detail::type_identity<_Ty>{});
        }

    private:
        inline std::string _cast_impl(detail::type_identity<std::string>) const {
            return _CXLIB utils::string::c_format("hsv(%3d,%3d%%,%3d%%)"
                , static_cast<int>(std::round(_h_val))
                , static_cast<int>(std::round(_s_val * 100.0))
                , static_cast<int>(std::round(_v_val * 100.0))
            );
        }

    }; // class

    // Ref: https://stackoverflow.com/a/6930407
    static hsv_color_t rgb2hsv(
        const rgb_color_t& in)
    {
        hsv_color_t out_;
        double min_, max_, delta_;

        min_ = (in.r() < in.g()) ? in.r() : in.g();
        min_ = (min_ < in.b()) ? min_ : in.b();
        max_ = (in.r() > in.g()) ? in.r() : in.g();
        max_ = (max_ > in.b()) ? max_ : in.b();

        out_.set_v(max_);
        delta_ = max_ - min_;

        if (delta_ < 0.00001) {
            out_.set_s(0.0);
            out_.set_h(0.0); // undefined, maybe nan?
            return out_;
        }

        if (max_ > 0.0) { // NOTE: if Max is == 0, this divide would cause a crash
            out_.set_s(delta_ / max_);
        } else {
            // if max is 0, then r = g = b = 0              
            // s = 0, h is undefined
            out_.set_s(0.0);
            out_.set_h(0.0/*NAN*/); // its now undefined
            return out_;
        }

        if (in.r() >= max_) { // > is bogus, just keeps compilor happy
            out_.set_h((in.g() - in.b()) / delta_); // between yellow & magenta
        } else {
            if (in.g() >= max_) {
                out_.set_h(2.0 + (in.b() - in.r()) / delta_); // between cyan & yellow
            } else {
                out_.set_h(4.0 + (in.r() - in.g()) / delta_); // between magenta & cyan
            }
        }

        out_.set_h(out_.h() * 60.0); // degrees
        if (out_.h() < 0.0) {
            out_.set_h(out_.h() + 360.0);
        }

        return out_;
    }

    // Ref: https://stackoverflow.com/a/6930407
    static rgb_color_t hsv2rgb(
        const hsv_color_t& in)
    {
        double hh_, p_, q_, t_, ff_;
        int32_t i_;
        rgb_color_t out_;
            
        if (in.s() <= 0.0) { // < is bogus, just shuts up warnings
            out_.set_r(in.v());
            out_.set_g(in.v());
            out_.set_b(in.v());
            return out_;
        }

        hh_ = in.h();
        if (hh_ >= 360.0) { hh_ = 0.0; }
        hh_ /= 60.0;
        i_ = static_cast<int32_t>(hh_);
        ff_ = hh_ - i_;
        p_ = in.v() * (1.0 - in.s());
        q_ = in.v() * (1.0 - (in.s() * ff_));
        t_ = in.v() * (1.0 - (in.s() * (1.0 - ff_)));

        switch (i_) {
        case 0:
            out_.set_r(in.v());
            out_.set_g(t_); 
            out_.set_b(p_);
            break;
        case 1:
            out_.set_r(q_);
            out_.set_g(in.v());
            out_.set_b(p_);
            break;
        case 2:
            out_.set_r(p_);
            out_.set_g(in.v());
            out_.set_b(t_);
            break;
        case 3:
            out_.set_r(p_);
            out_.set_g(q_);
            out_.set_b(in.v());
            break;
        case 4:
            out_.set_r(t_);
            out_.set_g(p_);
            out_.set_b(in.v());
            break;
        case 5:
        default:
            out_.set_r(in.v());
            out_.set_g(p_);
            out_.set_b(q_);
            break;
        }

        return out_;
    }

    /**
     * Refs:
     * https://en.wikipedia.org/wiki/Golden_ratio
     * https://martin.ankerl.com/2009/12/09/how-to-create-random-colors-programmatically/
     * https://stackoverflow.com/a/5104386
     * https://stackoverflow.com/a/1168328
     */
    class unique_color_generator final
    {
    private:
        static constexpr double kGoldenRatioConjugate{ 0.618033988749895 };

    private:
        hsv_color_t _next_hsv;

    public:
        unique_color_generator()
            : _next_hsv{ detail::random_engine{ 0.0, 360.0 }.get_value(), 0.45, 0.95 }
        { }

        unique_color_generator(const rgb_color_t& initial_rgb)
            : _next_hsv{ rgb2hsv(initial_rgb) }
        { }

        unique_color_generator(const hsv_color_t& initial_hsv) 
            : _next_hsv{ initial_hsv }
        { }

        rgb_color_t get_next_color() {
            const auto color = hsv2rgb(_next_hsv);
            double next_hue = detail::fmap(_next_hsv.h(), 0.0, 360.0, 0.0, 1.0);
            next_hue = std::fmod(next_hue + kGoldenRatioConjugate, 1.0);
            next_hue = detail::fmap(next_hue, 0.0, 1.0, 0.0, 360.0);
            _next_hsv.set_h(next_hue);
            return color;
        }

    }; // class

} // namespace
_CXLIB_NAMESPACE_END