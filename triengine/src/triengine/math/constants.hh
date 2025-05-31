#pragma once
#include <type_traits>

namespace triengine::math
{
	/**
	 * Math Constants.
	 */
	static constexpr double
		kPi = 3.141592653589793238462643383280, // Pi
		kDegToRad = 0.017453292519943295769236907685, // Pi/180
		kRadToDeg = 57.29577951308232087679815481410, // 180/Pi
		kGravityEarth = 9.80665; // Gravity acceleration on Earth. [Unit: m/s^2]

	/**
	 * Return `M_PI`
	 */
	template <typename _Scalar>
	constexpr _Scalar pi() noexcept {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");
		return static_cast<_Scalar>(kPi);
	}

	/**
	 * Convert angle unit: [radian] <-> [degree]
	 */
	template <typename _Scalar>
	constexpr _Scalar rad2deg(const _Scalar rad) noexcept {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");
		return rad * static_cast<_Scalar>(kRadToDeg);
	}

	template <typename _Scalar>
	constexpr _Scalar deg2rad(const _Scalar deg) noexcept {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");
		return deg * static_cast<_Scalar>(kDegToRad);
	}

} // namespace