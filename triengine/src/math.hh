#pragma once
#include "common.h"
#include "misc/debug_utils.hh"

namespace triengine::math
{
	namespace
	{
		/**
		 * Internal Math Constants.
		 */
		static constexpr double
			kDegToRad = 0.017453292519943295769236907685, // Pi/180
			kRadToDeg = 57.29577951308232087679815481410, // 180/Pi
			kPi = 3.141592653589793238462643383280; // Pi

	} // namespace

	/**
	 * Return M_PI
 	 */
	template <typename _Scalar>
	constexpr _Scalar pi() noexcept {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");
		return static_cast<_Scalar>(kPi);
	}

	/**
	 * Convert angle unit: radian <-> degree
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

	/**
	 * Create 3d rotation matrix.
	 *
	 * [Refs]
	 * https://github.com/GameTechDev/OpenGL-ES-3.0-Deferred-Rendering/blob/149ac95e12c9b35d5c1d3b78200258f1907bc4dc/src/vec_math.h#L612
	 * https://github.com/meshonline/kinect-openni-bvh-saver/blob/a6eba7471ec98f458a72e1751ee9a947268b67e9/vec_math.h#L548
	 * https://en.wikipedia.org/wiki/Rotation_matrix#Basic_3D_rotations
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix3<_Scalar> mat3_rotation_x(const _Scalar rad)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		const _Scalar
			c = std::cos(rad),
			s = std::sin(rad);

		Eigen::Matrix3<_Scalar> mr{ Eigen::Matrix3<_Scalar>::Identity() };
		mr.row(1).y() = c;
		mr.row(1).z() = s;
		mr.row(2).y() = -s;
		mr.row(2).z() = c;

		return mr;
	}

	/**
	 * Create 3d rotation matrix.
	 *
	 * [Refs]
	 * https://github.com/GameTechDev/OpenGL-ES-3.0-Deferred-Rendering/blob/149ac95e12c9b35d5c1d3b78200258f1907bc4dc/src/vec_math.h#L623
	 * https://github.com/meshonline/kinect-openni-bvh-saver/blob/a6eba7471ec98f458a72e1751ee9a947268b67e9/vec_math.h#L558
	 * https://en.wikipedia.org/wiki/Rotation_matrix#Basic_3D_rotations
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix3<_Scalar> mat3_rotation_y(const _Scalar rad)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		const _Scalar
			c = std::cos(rad),
			s = std::sin(rad);

		Eigen::Matrix3<_Scalar> mr{ Eigen::Matrix3<_Scalar>::Identity() };
		mr.row(0).x() = c;
		mr.row(0).z() = -s;
		mr.row(2).x() = s;
		mr.row(2).z() = c;

		return mr;
	}

	/**
	 * Create 3d rotation matrix.
	 *
	 * [Refs]
	 * https://github.com/GameTechDev/OpenGL-ES-3.0-Deferred-Rendering/blob/149ac95e12c9b35d5c1d3b78200258f1907bc4dc/src/vec_math.h#L634
	 * https://github.com/meshonline/kinect-openni-bvh-saver/blob/a6eba7471ec98f458a72e1751ee9a947268b67e9/vec_math.h#L568
	 * https://en.wikipedia.org/wiki/Rotation_matrix#Basic_3D_rotations
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix3<_Scalar> mat3_rotation_z(const _Scalar rad)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		const _Scalar
			c = std::cos(rad),
			s = std::sin(rad);

		Eigen::Matrix3<_Scalar> mr{ Eigen::Matrix3<_Scalar>::Identity() };
		mr.row(0).x() = c;
		mr.row(0).y() = s;
		mr.row(1).x() = -s;
		mr.row(1).y() = c;

		return mr;
	}

	/**
	 * Create a quaternion representing the rotation of direction vector `a` into direction vector `b`.
	 * In other words, the built rotation represent a rotation sending the line of direction `a` to the line of direction `b`, both lines passing through the origin.
	 * Note that the two input vectors do not have to be normalized, and do not need to have the same norm.
	 *
	 * [Refs]
	 * https://stackoverflow.com/q/57982427/3865427
	 */
	template <typename _Scalar>
	static inline Eigen::Quaternion<_Scalar> quat_from_two_vectors(
		const Eigen::Vector3<_Scalar>& vec_a,
		const Eigen::Vector3<_Scalar>& vec_b)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");
		Eigen::Quaternion<_Scalar> q;
		q.setFromTwoVectors(vec_a, vec_b);
		return q;
	}

	/**
	 * Equivalent of: `glm::lookAtRH` (`glm::lookAt`)
	 * 
	 * Ref:
	 * glm/ext/matrix_transform.inl
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> lookAt(
		const Eigen::Vector3<_Scalar>& eye,
		const Eigen::Vector3<_Scalar>& center,
		const Eigen::Vector3<_Scalar>& up)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		const Eigen::Vector3<_Scalar> 
			f{ (center - eye).normalized() },
			s{ f.cross(up).normalized() },
			u{ s.cross(f) };

		Eigen::Matrix4f result{ Eigen::Matrix4f::Identity() };
		result(0, 0) = s.x();
		result(0, 1) = s.y();
		result(0, 2) = s.z();
		result(1, 0) = u.x();
		result(1, 1) = u.y();
		result(1, 2) = u.z();
		result(2, 0) = -f.x();
		result(2, 1) = -f.y();
		result(2, 2) = -f.z();
		result(0, 3) = -s.dot(eye);
		result(1, 3) = -u.dot(eye);
		result(2, 3) = f.dot(eye);

		return result;
	}

	/**
	 * Equivalent of: `glm::perspectiveRH_NO` (`glm::perspective`)
	 * 
	 * Ref:
	 * glm/ext/matrix_clip_space.inl
	 */
	template<typename _Scalar>
	static inline Eigen::Matrix4f perspective(
		const _Scalar fovy,
		const _Scalar aspect,
		const _Scalar zNear,
		const _Scalar zFar)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		//TRIENGINE_ASSERT(
		//	std::abs(aspect - std::numeric_limits<_Scalar>::epsilon()) > static_cast<_Scalar>(0)
		//);

		const _Scalar tanHalfFovy = std::tan(fovy / static_cast<_Scalar>(2));

		Eigen::Matrix4<_Scalar> result{ Eigen::Matrix4<_Scalar>::Zero() };
		result(0, 0) = static_cast<_Scalar>(1) / (aspect * tanHalfFovy);
		result(1, 1) = static_cast<_Scalar>(1) / tanHalfFovy;
		result(2, 2) = - (zFar + zNear) / (zFar - zNear);
		result(3, 2) = - static_cast<_Scalar>(1);
		result(2, 3) = - (static_cast<_Scalar>(2) * zFar * zNear) / (zFar - zNear);

		return result;
	}

	/**
	 * Equivalent of: `glm::rotate`
	 * 
	 * Ref:
	 * glm/ext/quaternion_transform.inl
	 */
	template<typename _Scalar>
	static inline Eigen::Quaternion<_Scalar> rotate(
		Eigen::Quaternion<_Scalar> const& q,
		_Scalar const angle,
		Eigen::Vector3<_Scalar> const& v)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		Eigen::Vector3<_Scalar> Tmp = v;

		// Axis of rotation must be normalised
		_Scalar const len = Tmp.norm();
		if (std::abs(len - static_cast<_Scalar>(1)) > static_cast<_Scalar>(0.001)) {
			_Scalar const oneOverLen = static_cast<_Scalar>(1) / len;
			Tmp.x() *= oneOverLen;
			Tmp.y() *= oneOverLen;
			Tmp.z() *= oneOverLen;
		}

		_Scalar const AngleRad(angle);
		_Scalar const Sin = std::sin(AngleRad * static_cast<_Scalar>(0.5));

		return q * Eigen::Quaternion<_Scalar>(
			/*w*/std::cos(AngleRad * static_cast<_Scalar>(0.5)),
			/*x*/Tmp.x() * Sin,
			/*y*/Tmp.y() * Sin,
			/*z*/Tmp.z() * Sin
		);
	}
	
	/**
	 * Equivalent of: `glm::rotate`
	 * 
	 * Ref:
	 * glm/ext/matrix_transform.inl
	 */
	template<typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> rotate(
		Eigen::Matrix4<_Scalar> const& m,
		_Scalar const angle,
		Eigen::Vector3<_Scalar> const& v)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		_Scalar const 
			a = angle,
			c = std::cos(a),
			s = std::sin(a);

		Eigen::Vector3<_Scalar> 
			axis(v.normalized()),
			temp((_Scalar(1) - c) * axis);

		Eigen::Matrix4<_Scalar> Rotate;
		//Rotate[0][0] = c + temp[0] * axis[0];
		//Rotate[0][1] = temp[0] * axis[1] + s * axis[2];
		//Rotate[0][2] = temp[0] * axis[2] - s * axis[1];

		//Rotate[1][0] = temp[1] * axis[0] - s * axis[2];
		//Rotate[1][1] = c + temp[1] * axis[1];
		//Rotate[1][2] = temp[1] * axis[2] + s * axis[0];

		//Rotate[2][0] = temp[2] * axis[0] + s * axis[1];
		//Rotate[2][1] = temp[2] * axis[1] - s * axis[0];
		//Rotate[2][2] = c + temp[2] * axis[2];

		Eigen::Matrix4<_Scalar> Result;
		//Result[0] = m[0] * Rotate[0][0] + m[1] * Rotate[0][1] + m[2] * Rotate[0][2];
		//Result[1] = m[0] * Rotate[1][0] + m[1] * Rotate[1][1] + m[2] * Rotate[1][2];
		//Result[2] = m[0] * Rotate[2][0] + m[1] * Rotate[2][1] + m[2] * Rotate[2][2];
		//Result[3] = m[3];

		return Result;
	}

	/**
	 * Equivalent of: `glm::eulerAngleXYZ`
	 *
	 * Ref:
	 * glm/gtx/euler_angles.inl
	 */
	template<typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> eulerAngleXYZ
	(
		_Scalar const t1,
		_Scalar const t2,
		_Scalar const t3
	)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		_Scalar c1 = std::cos(-t1);
		_Scalar c2 = std::cos(-t2);
		_Scalar c3 = std::cos(-t3);
		_Scalar s1 = std::sin(-t1);
		_Scalar s2 = std::sin(-t2);
		_Scalar s3 = std::sin(-t3);

		Eigen::Matrix4<_Scalar> Result;
		Result(0, 0) = c2 * c3;
		Result(1, 0) = -c1 * s3 + s1 * s2 * c3;
		Result(2, 0) = s1 * s3 + c1 * s2 * c3;
		Result(3, 0) = static_cast<_Scalar>(0);

		Result(0, 1) = c2 * s3;
		Result(1, 1) = c1 * c3 + s1 * s2 * s3;
		Result(2, 1) = -s1 * c3 + c1 * s2 * s3;
		Result(3, 1) = static_cast<_Scalar>(0);

		Result(0, 2) = -s2;
		Result(1, 2) = s1 * c2;
		Result(2, 2) = c1 * c2;
		Result(3, 2) = static_cast<_Scalar>(0);

		Result(0, 3) = static_cast<_Scalar>(0);
		Result(1, 3) = static_cast<_Scalar>(0);
		Result(2, 3) = static_cast<_Scalar>(0);
		Result(3, 3) = static_cast<_Scalar>(1);
		return Result;
	}

	/**
	 * Equivalent of: `glm::scale`
	 * 
	 *    Scaling Matrix        Model matrix
	 *    | SX,  0,  0,  0 |    | M00 M01 M02 M03 |
	 *    |  0, SY,  0,  0 |  * | M10 M11 M12 M13 |
	 *    |  0,  0, SZ,  0 |    | M20 M21 M22 M23 |
	 *    |  0,  0,  0,  1 |    | M30 M31 M32 M33 |
	 */
	template<typename _Scalar>
	static inline const Eigen::Matrix4<_Scalar> scale(
		const Eigen::Matrix4<_Scalar>& model,
		const Eigen::Vector3<_Scalar>& scale_vec)
	{
		Eigen::Matrix4<_Scalar> Result = model;
		Result.row(0) *= scale_vec(0);
		Result.row(1) *= scale_vec(1);
		Result.row(2) *= scale_vec(2);
		return Result;
	}

#if defined(_TRIENGINE_HAS_GLM)
	//
	// Convert Eigen vector/matrix to GLM vector/matrix
	//
	template<glm::qualifier _Q, typename _Scalar, int _Rows, int _Cols>
	static inline glm::mat<_Rows, _Cols, _Scalar, _Q> eigen2glm(
		const Eigen::Matrix<_Scalar, _Rows, _Cols>& eMat)
	{
		glm::mat<_Rows, _Cols, _Scalar, _Q> gMat;
		for (int row = 0; row < _Rows; ++row) {
			for (int col = 0; col < _Cols; ++col) {
				gMat[col][row] = eMat(row, col);
			}
		}
		return gMat;
	}

	template<glm::qualifier _Q, typename _Scalar, int _Size>
	static inline glm::vec<_Size, _Scalar, _Q> eigen2glm(
		const Eigen::Matrix<_Scalar, _Size, 1>& eVec)
	{
		glm::vec<_Size, _Scalar, _Q> gVec;
		for (int i = 0; i < _Size; ++i) {
			gVec[i] = eVec(i);
		}
		return gVec;
	}

	//
	// Convert GLM vector/matrix to Eigen vector/matrix
	//
	template<typename _Scalar, int _Rows, int _Cols, glm::qualifier _Q>
	static inline Eigen::Matrix<_Scalar, _Rows, _Cols> glm2eigen(
		const glm::mat<_Rows, _Cols, _Scalar, _Q>& gMat)
	{
		Eigen::Matrix<_Scalar, _Rows, _Cols> eMat;
		for (int row = 0; row < _Rows; ++row) {
			for (int col = 0; col < _Cols; ++col) {
				eMat(row, col) = gMat[col][row];
			}
		}
		return eMat;
	}

	template<typename _Scalar, int _Size, glm::qualifier _Q>
	static inline Eigen::Matrix<_Scalar, _Size, 1> glm2eigen(
		const glm::vec<_Size, _Scalar, _Q>& gVec)
	{
		Eigen::Matrix<_Scalar, _Size, 1> eVec;
		for (int i = 0; i < _Size; ++i) {
			eVec(i) = gVec[i];
		}
		return eVec;
	}
#endif // ^^^ _TRIENGINE_HAS_GLM ^^^

} // namespace