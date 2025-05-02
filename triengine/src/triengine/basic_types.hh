#pragma once
#include <triengine/common.h>

#include <memory>
#include <vector>
#include <array>

namespace triengine
{
    /// normalized 3-channel RGB color.
    template <typename _Scalar>
    class color3_ {
    public:
        using scalar_type = _Scalar;
        using this_type = color3_;
        static constexpr size_t SizeAtCompileTime = 3;

        static_assert(std::is_floating_point_v<scalar_type>, "!!");

    public:
        std::array<scalar_type, SizeAtCompileTime> rgb{};

    public:
        static inline this_type all(scalar_type scalar) {
            return this_type{ scalar, scalar, scalar };
        }

        static inline this_type zero() {
            return all(static_cast<scalar_type>(0));
        }

    public:
        color3_() = default;
        color3_(scalar_type r_, scalar_type g_, scalar_type b_) : rgb{ r_, g_, b_ } {}
        explicit color3_(const Eigen::Vector3<scalar_type>& v) : rgb{ v[0], v[1], v[2] } {}

        size_t size() const noexcept { return rgb.size(); }

        const scalar_type* data() const noexcept { return rgb.data(); }
        scalar_type* data() noexcept { return rgb.data(); }

        const scalar_type& r() const noexcept { return rgb[0]; }
        scalar_type& r() noexcept { return rgb[0]; }

        const scalar_type& g() const noexcept { return rgb[1]; }
        scalar_type& g() noexcept { return rgb[1]; }

        const scalar_type& b() const noexcept { return rgb[2]; }
        scalar_type& b() noexcept { return rgb[2]; }

        const scalar_type& operator[](size_t i) const noexcept { return rgb[i]; }
        scalar_type& operator[](size_t i) noexcept { return rgb[i]; }

        const scalar_type& operator()(size_t i) const noexcept { return rgb[i]; }
        scalar_type& operator()(size_t i) noexcept { return rgb[i]; }

        Eigen::Vector3<scalar_type> to_eigen() const {
            return Eigen::Vector3<scalar_type>{ rgb[0], rgb[1], rgb[2] };
        }
    };

    /// normalized 4-channel RGBA color.
    template <typename _Scalar>
    class color4_ {
    public:
        using scalar_type = _Scalar;
        using this_type = color4_;
        static constexpr size_t SizeAtCompileTime = 4;

        static_assert(std::is_floating_point_v<scalar_type>, "!!");

    public:
        std::array<scalar_type, SizeAtCompileTime> rgba{};

    public:
        static inline this_type all(scalar_type scalar) {
            return this_type{ scalar, scalar, scalar, scalar };
        }

        static inline this_type zero() {
            return all(static_cast<scalar_type>(0));
        }

    public:
        color4_() = default;
        color4_(scalar_type r_, scalar_type g_, scalar_type b_, scalar_type a_) : rgba{ r_, g_, b_, a_ } {}
        explicit color4_(const Eigen::Vector4<scalar_type>& v) : rgba{ v[0], v[1], v[2], v[3] } {}

        size_t size() const noexcept { return rgba.size(); }

        const scalar_type* data() const noexcept { return rgba.data(); }
        scalar_type* data() noexcept { return rgba.data(); }

        const scalar_type& r() const noexcept { return rgba[0]; }
        scalar_type& r() noexcept { return rgba[0]; }

        const scalar_type& g() const noexcept { return rgba[1]; }
        scalar_type& g() noexcept { return rgba[1]; }

        const scalar_type& b() const noexcept { return rgba[2]; }
        scalar_type& b() noexcept { return rgba[2]; }
        
        const scalar_type& a() const noexcept { return rgba[3]; }
        scalar_type& a() noexcept { return rgba[3]; }

        const scalar_type& operator[](size_t i) const noexcept { return rgba[i]; }
        scalar_type& operator[](size_t i) noexcept { return rgba[i]; }

        const scalar_type& operator()(size_t i) const noexcept { return rgba.at(i); }
        scalar_type& operator()(size_t i) noexcept { return rgba.at(i); }

        Eigen::Vector4<scalar_type> to_eigen() const {
            return Eigen::Vector4<scalar_type>{ rgba[0], rgba[1], rgba[2], rgba[3] };
        }
    };

    using color3_f32 = color3_<float>;
    using color3_f64 = color3_<double>;
    
    using color4_f32 = color4_<float>;
    using color4_f64 = color4_<double>;

    template <typename _Scalar>
    using vec2_ = Eigen::Matrix<_Scalar, 2, 1/*, Eigen::DontAlign*/>;
    using vec2_f32 = vec2_<float>;
    using vec2_f64 = vec2_<double>;
    using vec2_i32 = vec2_<int32_t>;
    using vec2_i64 = vec2_<int64_t>;
    using vec2_u32 = vec2_<uint32_t>;
    using vec2_u64 = vec2_<uint64_t>;

    template <typename _Scalar>
    using vec3_ = Eigen::Matrix<_Scalar, 3, 1/*, Eigen::DontAlign*/>;
    using vec3_f32 = vec3_<float>;
    using vec3_f64 = vec3_<double>;
    using vec3_i32 = vec3_<int32_t>;
    using vec3_i64 = vec3_<int64_t>;
    using vec3_u32 = vec3_<uint32_t>;
    using vec3_u64 = vec3_<uint64_t>;

    template <typename _Scalar>
    using vec4_ = Eigen::Matrix<_Scalar, 4, 1/*, Eigen::DontAlign*/>;
    using vec4_f32 = vec4_<float>;
    using vec4_f64 = vec4_<double>;
    using vec4_i32 = vec4_<int32_t>;
    using vec4_i64 = vec4_<int64_t>;
    using vec4_u32 = vec4_<uint32_t>;
    using vec4_u64 = vec4_<uint64_t>;

    template <typename _Scalar>
    using mat2_ = Eigen::Matrix<_Scalar, 2, 2/*, Eigen::DontAlign*/>;
    using mat2_f32 = mat2_<float>;
    using mat2_f64 = mat2_<double>;

    template <typename _Scalar>
    using mat3_ = Eigen::Matrix<_Scalar, 3, 3/*, Eigen::DontAlign*/>;
    using mat3_f32 = mat3_<float>;
    using mat3_f64 = mat3_<double>;

    template <typename _Scalar>
    using mat4_ = Eigen::Matrix<_Scalar, 4, 4/*, Eigen::DontAlign*/>;
    using mat4_f32 = mat4_<float>;
    using mat4_f64 = mat4_<double>;

    template <typename _Scalar>
    using quat_ = Eigen::Quaternion<_Scalar>;
    using quat_f32 = quat_<float>;
    using quat_f64 = quat_<double>;

} // namespace
