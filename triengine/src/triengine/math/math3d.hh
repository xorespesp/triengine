#pragma once
#include <triengine/common.h>
#include <triengine/utility/debug_utils.hh>

#include <type_traits>
#include <algorithm>

namespace triengine::math
{
	template <typename _Scalar>
	static inline Eigen::Vector2<_Scalar> vec2_all(_Scalar v) {
		return Eigen::Vector2<_Scalar>::Constant(v);
	}

	template <typename _Scalar>
	static inline Eigen::Vector3<_Scalar> vec3_all(_Scalar v) {
		return Eigen::Vector3<_Scalar>::Constant(v);
	}

	template <typename _Scalar>
	static inline Eigen::Vector4<_Scalar> vec4_all(_Scalar v) {
		return Eigen::Vector4<_Scalar>::Constant(v);
	}

	template <typename _Scalar>
	static inline Eigen::Matrix2<_Scalar> mat2_all(_Scalar v) {
		return Eigen::Matrix2<_Scalar>::Constant(v);
	}

	template <typename _Scalar>
	static inline Eigen::Matrix3<_Scalar> mat3_all(_Scalar v) {
		return Eigen::Matrix3<_Scalar>::Constant(v);
	}

	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> mat4_all(_Scalar v) {
		return Eigen::Matrix4<_Scalar>::Constant(v);
	}

	template <typename _Scalar>
	static inline Eigen::Vector2<_Scalar> vec2_identity() {
		return Eigen::Vector2<_Scalar>::Identity();
	}

	template <typename _Scalar>
	static inline Eigen::Vector3<_Scalar> vec3_identity() {
		return Eigen::Vector3<_Scalar>::Identity();
	}

	template <typename _Scalar>
	static inline Eigen::Vector4<_Scalar> vec4_identity() {
		return Eigen::Vector4<_Scalar>::Identity();
	}

	template <typename _Scalar>
	static inline Eigen::Matrix2<_Scalar> mat2_identity() {
		return Eigen::Matrix2<_Scalar>::Identity();
	}

	template <typename _Scalar>
	static inline Eigen::Matrix3<_Scalar> mat3_identity() {
		return Eigen::Matrix3<_Scalar>::Identity();
	}

	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> mat4_identity() {
		return Eigen::Matrix4<_Scalar>::Identity();
	}

	/**
	 * @brief Extends an Eigen::Matrix<_Scalar, 3, 3> to an Eigen::Matrix<_Scalar, 4, 4> (no translation).
	 * The input 3x3 matrix is copied to the top-left 3x3 block of a 4x4 identity matrix.
	 * The rest of the 4x4 matrix remains as an identity matrix (translation (0,0,0), m(3,3)=1).
	 * @tparam _Scalar The scalar type (e.g., float, double). Must be a floating-point type.
	 * @param rotationScalePart The 3x3 matrix to extend (typically rotation or rotation/scale).
	 * @return The extended 4x4 matrix.
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> extend_to_mat4(
		const Eigen::Matrix3<_Scalar>& rotationScalePart) {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");
		Eigen::Matrix4<_Scalar> mat4 = Eigen::Matrix4<_Scalar>::Identity();
		mat4.template block<3, 3>(0, 0) = rotationScalePart;
		return mat4;
	}

	/**
	 * @brief Creates an Eigen::Matrix<_Scalar, 4, 4> representing only a translation.
	 * The rotation/scale part (top-left 3x3 block) will be an identity matrix.
	 * The rest of the 4x4 matrix remains as an identity matrix (m(3,3)=1).
	 * @tparam _Scalar The scalar type (e.g., float, double). Must be a floating-point type.
	 * @param translationPart The 3D vector for the translation part.
	 * @return The 4x4 matrix representing only the translation.
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> extend_to_mat4(
		const Eigen::Vector3<_Scalar>& translationPart) {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");
		Eigen::Matrix4<_Scalar> mat4 = Eigen::Matrix4<_Scalar>::Identity();
		mat4.template block<3, 1>(0, 3) = translationPart;
		return mat4;
	}

	/**
	 * @brief Extends an Eigen::Matrix<_Scalar, 3, 3> and a translation Eigen::Vector<_Scalar, 3>
	 * to an Eigen::Matrix<_Scalar, 4, 4>.
	 * @tparam _Scalar The scalar type (e.g., float, double). Must be a floating-point type.
	 * @param rotationScalePart The 3x3 matrix for the top-left block (rotation/scale).
	 * @param translationPart The 3D vector for the translation part.
	 * @return The resulting 4x4 transformation matrix.
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> extend_to_mat4(
		const Eigen::Matrix3<_Scalar>& rotationScalePart,
		const Eigen::Vector3<_Scalar>& translationPart) {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");
		Eigen::Matrix4<_Scalar> mat4 = Eigen::Matrix4<_Scalar>::Identity();
		mat4.template block<3, 3>(0, 0) = rotationScalePart;
		mat4.template block<3, 1>(0, 3) = translationPart;
		return mat4;
	}

	/**
	 * @brief Extends an Eigen::AngleAxis<_Scalar> (rotation) to an Eigen::Matrix<_Scalar, 4, 4> (no translation).
	 * @tparam _Scalar The scalar type (e.g., float, double). Must be a floating-point type.
	 * @param rotationPart The AngleAxis rotation to extend.
	 * @return The 4x4 matrix representing the rotation.
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> extend_to_mat4(
		const Eigen::AngleAxis<_Scalar>& rotationPart) {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");
		Eigen::Matrix4<_Scalar> mat4 = Eigen::Matrix4<_Scalar>::Identity();
		mat4.template block<3, 3>(0, 0) = rotationPart.toRotationMatrix();
		return mat4;
	}

	/**
	 * @brief Extends an Eigen::AngleAxis<_Scalar> (rotation) and a translation Eigen::Vector<_Scalar, 3>
	 * to an Eigen::Matrix<_Scalar, 4, 4>.
	 * @tparam _Scalar The scalar type (e.g., float, double). Must be a floating-point type.
	 * @param rotationPart The AngleAxis for the rotation part.
	 * @param translationPart The 3D vector for the translation part.
	 * @return The resulting 4x4 transformation matrix.
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> extend_to_mat4(
		const Eigen::AngleAxis<_Scalar>& rotationPart,
		const Eigen::Vector3<_Scalar>& translationPart) {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");
		Eigen::Matrix4<_Scalar> mat4 = Eigen::Matrix4<_Scalar>::Identity();
		mat4.template block<3, 3>(0, 0) = rotationPart.toRotationMatrix();
		mat4.template block<3, 1>(0, 3) = translationPart;
		return mat4;
	}

	/**
	 * @brief Extends an Eigen::Quaternion<_Scalar> (rotation) to an Eigen::Matrix<_Scalar, 4, 4> (no translation).
	 * @tparam _Scalar The scalar type (e.g., float, double). Must be a floating-point type.
	 * @param rotationPart The Quaternion rotation to extend.
	 * @return The 4x4 matrix representing the rotation.
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> extend_to_mat4(
		const Eigen::Quaternion<_Scalar>& rotationPart) {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");
		Eigen::Matrix4<_Scalar> mat4 = Eigen::Matrix4<_Scalar>::Identity();
		mat4.template block<3, 3>(0, 0) = rotationPart.toRotationMatrix();
		return mat4;
	}

	/**
	 * @brief Extends an Eigen::Quaternion<_Scalar> (rotation) and a translation Eigen::Vector<_Scalar, 3>
	 * to an Eigen::Matrix<_Scalar, 4, 4>.
	 * @tparam _Scalar The scalar type (e.g., float, double). Must be a floating-point type.
	 * @param rotationPart The Quaternion for the rotation part.
	 * @param translationPart The 3D vector for the translation part.
	 * @return The resulting 4x4 transformation matrix.
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> extend_to_mat4(
		const Eigen::Quaternion<_Scalar>& rotationPart,
		const Eigen::Vector3<_Scalar>& translationPart) {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");
		Eigen::Matrix4<_Scalar> mat4 = Eigen::Matrix4<_Scalar>::Identity();
		mat4.template block<3, 3>(0, 0) = rotationPart.toRotationMatrix();
		mat4.template block<3, 1>(0, 3) = translationPart;
		return mat4;
	}

	/**
	 * Create 3d rotation matrix around x-axis. (right-handed)
	 *
	 * [Refs]
	 * https://en.wikipedia.org/wiki/Rotation_matrix#Basic_3D_rotations
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix3<_Scalar> mat3_rotation_x_RH(const _Scalar rad)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		const _Scalar
			c = std::cos(rad),
			s = std::sin(rad);

		Eigen::Matrix3<_Scalar> R{ Eigen::Matrix3<_Scalar>::Identity() };
		R(1, 1) = c;
		R(1, 2) = -s;
		R(2, 1) = s;
		R(2, 2) = c;
		return R;
	}

	/**
	 * Create 3d rotation matrix around x-axis. (left-handed)
	 *
	 * [Refs]
	 * https://en.wikipedia.org/wiki/Rotation_matrix#Basic_3D_rotations
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix3<_Scalar> mat3_rotation_x_LH(const _Scalar rad)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		const _Scalar
			c = std::cos(rad),
			s = std::sin(rad);

		Eigen::Matrix3<_Scalar> R{ Eigen::Matrix3<_Scalar>::Identity() };
		R(1, 1) = c;
		R(1, 2) = s;
		R(2, 1) = -s;
		R(2, 2) = c;
		return R;
	}

	/**
	 * Create 3d rotation matrix around y-axis. (right-handed)
	 *
	 * [Refs]
	 * https://en.wikipedia.org/wiki/Rotation_matrix#Basic_3D_rotations
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix3<_Scalar> mat3_rotation_y_RH(const _Scalar rad)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		const _Scalar
			c = std::cos(rad),
			s = std::sin(rad);

		Eigen::Matrix3<_Scalar> R{ Eigen::Matrix3<_Scalar>::Identity() };
		R(0, 0) = c;
		R(0, 2) = s;
		R(2, 0) = -s;
		R(2, 2) = c;
		return R;
	}

	/**
	 * Create 3d rotation matrix around y-axis. (left-handed)
	 *
	 * [Refs]
	 * https://en.wikipedia.org/wiki/Rotation_matrix#Basic_3D_rotations
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix3<_Scalar> mat3_rotation_y_LH(const _Scalar rad)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		const _Scalar
			c = std::cos(rad),
			s = std::sin(rad);

		Eigen::Matrix3<_Scalar> R{ Eigen::Matrix3<_Scalar>::Identity() };
		R(0, 0) = c;
		R(0, 2) = -s;
		R(2, 0) = s;
		R(2, 2) = c;
		return R;
	}

	/**
	 * Create 3d rotation matrix around z-axis. (right-handed)
	 *
	 * [Refs]
	 * https://en.wikipedia.org/wiki/Rotation_matrix#Basic_3D_rotations
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix3<_Scalar> mat3_rotation_z_RH(const _Scalar rad)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		const _Scalar
			c = std::cos(rad),
			s = std::sin(rad);

		Eigen::Matrix3<_Scalar> R{ Eigen::Matrix3<_Scalar>::Identity() };
		R(0, 0) = c;
		R(0, 1) = -s;
		R(1, 0) = s;
		R(1, 1) = c;
		return R;
	}

	/**
	 * Create 3d rotation matrix around z-axis. (left-handed)
	 *
	 * [Refs]
	 * https://en.wikipedia.org/wiki/Rotation_matrix#Basic_3D_rotations
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix3<_Scalar> mat3_rotation_z_LH(const _Scalar rad)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		const _Scalar
			c = std::cos(rad),
			s = std::sin(rad);

		Eigen::Matrix3<_Scalar> R{ Eigen::Matrix3<_Scalar>::Identity() };
		R(0, 0) = c;
		R(0, 1) = s;
		R(1, 0) = -s;
		R(1, 1) = c;
		return R;
	}

	/**
	 * Unrotate quaternion.
	 *
	 * [Note]
	 * Quaternion inversion(or just conjugate for the normalized case) creates the inverse rotation(the same rotation in the opposite direction).
	 *
	 * [Refs]
	 * https://math.stackexchange.com/a/581728
	 */
	template <typename _Ty>
	static inline Eigen::Quaternion<_Ty> quat_unrotate(const Eigen::Quaternion<_Ty>& q_target)
	{
		static_assert(std::is_floating_point_v<_Ty>, "!!");
		return q_target.inverse();
	}

	/**
	 * Combine two rotation quaternions. (apply offset rotation to target rotation)
	 * Rotation applying order: `q_first` -> `q_second`
	 *
	 * [Note]
	 * multiplying two quaternions is the same as applying both rotations in sequence.
	 * multiplying order matters when composing quaternions;
	 * rotations applying is always from right to left:
	 *     e.g #1) `QW == Qp * Qch` It means we apply `Qch` first, and `Qp` then.
	 *     e.g #2) `Qch == Qp.Inversed * QW` So we apply `QW` first, then unrotate it by `Qp` back.
	 *     e.g #3) `Qp == QW * Qch.Inversed` So we apply inverse `Qch` rotation. Then total `QW`. It yield `Qp`.
	 *     (Note: #2 & #3 work so, provided that `QW` is obtained by #1 formula)
	 *
	 * [Refs]
	 * https://math.stackexchange.com/a/2131519
	 * https://stackoverflow.com/q/26963207
	 */
	template <typename _Ty>
	static inline Eigen::Quaternion<_Ty> quat_combine(
		const Eigen::Quaternion<_Ty>& q_first,
		const Eigen::Quaternion<_Ty>& q_second)
	{
		static_assert(std::is_floating_point_v<_Ty>, "!!");
		// Rotation applying order: `q_first` -> `q_second`
		return Eigen::Quaternion<_Ty>{ q_second * q_first };
	}

	/**
	 * Calculate relative(local) rotation between two quaternions.
	 *
	 * [Note]
	 * Invert `q_parent` and multiply `q_target` to get relative(local) rotation.
	 *
	 * [Refs]
	 * https://math.stackexchange.com/a/2131519
	 * https://math.stackexchange.com/a/2355070
	 * https://math.stackexchange.com/a/581728
	 * https://stackoverflow.com/a/72849029
	 */
	template <typename _Ty>
	static inline Eigen::Quaternion<_Ty> quat_relative(
		const Eigen::Quaternion<_Ty>& q_target,
		const Eigen::Quaternion<_Ty>& q_parent)
	{
		// Equivalent of: `Vec_Math::Quaternion q_delta = Vec_Math::quat_left_multiply(q_target, Vec_Math::quat_inverse(q_parent));`
		// Note: In vector multiply operation, left multiply means that you multiply the left vector with the right vector.
		//       e.g) left_multiply(a, b) == b * a
		// 
		// TODO: use `q_parent.inverse()` or `q_parent.normalized.conjugate()` ?
		return Eigen::Quaternion<_Ty>/* q_delta */{ q_parent.inverse()* q_target };
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
	 * Create a quaternion from euler angles
	 */
	template <typename _Scalar>
	static inline Eigen::Quaternion<_Scalar> quat_from_euler(
		const Eigen::Vector3<_Scalar>& euler_angles_rad,
		const std::string_view euler_axis_order/* e.g: "XYZ", "ZYX", ... */)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		if (euler_axis_order.size() != 3) {
			throw std::invalid_argument{ "Invalid euler axis order size" };
		}

		// Rotation order: first axis -> second axis -> third axis (intrinsic rotation)
		// Q_total = Q_first_axis * Q_second_axis * Q_third_axis
		Eigen::Quaternion<_Scalar> q_total = Eigen::Quaternion<_Scalar>::Identity();
		for (size_t i = 0; i < euler_axis_order.size(); ++i) {
			switch (euler_axis_order[i]) {
			case 'X': case 'x':
				q_total = q_total * Eigen::AngleAxis<_Scalar>(euler_angles_rad(i), Eigen::Vector3<_Scalar>::UnitX());
				break;
			case 'Y': case 'y':
				q_total = q_total * Eigen::AngleAxis<_Scalar>(euler_angles_rad(i), Eigen::Vector3<_Scalar>::UnitY());
				break;
			case 'Z': case 'z':
				q_total = q_total * Eigen::AngleAxis<_Scalar>(euler_angles_rad(i), Eigen::Vector3<_Scalar>::UnitZ());
				break;
			default:
				throw std::invalid_argument{ "Invalid euler axis char" };
			}
		}

		return q_total;
	}

	/**
	 * Perform 3d geometric-transformation(rotation + translation).
	 *
	 *    | x' |   | R R R T |   | x |
	 *    | y' | = | R R R T | * | y |
	 *    | z' |   | R R R T |   | z |
	 *    | 1  |   | 0 0 0 1 |   | 1 |
	 *
	 * [Refs]
	 * https://inyongs.tistory.com/132
	 */
	template <typename _Ty>
	static inline Eigen::Vector3<_Ty> vec3_transform(
		const Eigen::Vector3<_Ty>& point3d/* target 3d point (column vector) */,
		const Eigen::Matrix4<_Ty>& trans/* 3d geometric transform matrix */)
	{
		static_assert(std::is_floating_point_v<_Ty>, "!!");

		Eigen::Vector4<_Ty> vec4;
		vec4.fill(static_cast<_Ty>(1.0));
		vec4.head<3>() = point3d;

		vec4 = trans * vec4;

		return Eigen::Vector3<_Ty>{ vec4.head<3>() };
	}

	/**
	 * Rotate 3d point around the origin.
	 *
	 * [Refs]
	 * https://en.wikipedia.org/wiki/Rotation_matrix#Ambiguities
	 */
	template <typename _Ty>
	static inline Eigen::Vector3<_Ty> vec3_rotate(
		const Eigen::Vector3<_Ty>& point3d/* target 3d point (column vector) */,
		const Eigen::Matrix3<_Ty>& R/* 3d rotation matrix */)
	{
		static_assert(std::is_floating_point_v<_Ty>, "!!");

		// https://en.wikipedia.org/wiki/Rotation_matrix#Ambiguities
		// The point vector can be pre-multiplied by a rotation matrix (`R*v`, where `v` is a column vector), 
		// or post-multiplied by it (`w*R`, where `w` is a row vector).
		// However, `R*v` produces a rotation in the opposite direction with respect to `w*R`.
		// To obtain exactly the same rotation (i.e. the same final coordinates of point vector), 
		// the equivalent row vector must be post-multiplied by the transpose of `R` (i.e. `w*R^T`).

		return R * point3d; // R*v
		//return R.transpose().eval() * point_3d; // (R^T)*v == w*R; equivalent of: `Vec_Math::mat3_mul_vector`
	}

	/**
	 * Rotate 3d point around a specific center.
	 */
	template <typename _Ty>
	static inline Eigen::Vector3<_Ty> vec3_rotate(
		const Eigen::Vector3<_Ty>& point3d/* target 3d point (column vector) */,
		const Eigen::Matrix3<_Ty>& R/* 3d rotation matrix */,
		const Eigen::Vector3<_Ty>& center)
	{
		static_assert(std::is_floating_point_v<_Ty>, "!!");

		Eigen::Vector3<_Ty> p = point3d - center; // Move point to rotation center (point - center)
		p = vec3_rotate(p, R); // Apply rotation
		p += center; // Move back to original center (rotated vector + center)
		return p;
	}

	/**
	 * Distance between two 3D points in 3d space.
	 *
	 * [Refs]
	 * https://www.engineeringtoolbox.com/distance-relationship-between-two-points-d_1854.html
	 */
	template <typename _Ty>
	static inline _Ty vec3_distance(
		const Eigen::Vector3<_Ty>& p1,
		const Eigen::Vector3<_Ty>& p2)
	{
		static_assert(std::is_floating_point_v<_Ty>, "!!");
		return (p1 - p2).norm();
	}

	/**
	 * Generate lookAt(view) matrix
	 * 
	 * Ref:
	 * glm/ext/matrix_transform.inl
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> lookAt(
		const Eigen::Vector3<_Scalar>& eye_position, // camera position vector
		const Eigen::Vector3<_Scalar>& eye_target,   // camera lookat position vector (Point that the camera is looking at)
		const Eigen::Vector3<_Scalar>& world_up      // world up vector
	) {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		// Calculate camera vectors(front, right, up)
		const Eigen::Vector3<_Scalar> 
			eye_front{ (eye_target - eye_position).normalized() },
			eye_right{ eye_front.cross(world_up).normalized() },
			eye_up{ eye_right.cross(eye_front) };

		Eigen::Matrix4<_Scalar> result{ Eigen::Matrix4<_Scalar>::Identity() };
		result(0, 0) = eye_right.x();
		result(0, 1) = eye_right.y();
		result(0, 2) = eye_right.z();
		result(1, 0) = eye_up.x();
		result(1, 1) = eye_up.y();
		result(1, 2) = eye_up.z();
		result(2, 0) = -eye_front.x();
		result(2, 1) = -eye_front.y();
		result(2, 2) = -eye_front.z();
		result(0, 3) = -eye_right.dot(eye_position);
		result(1, 3) = -eye_up.dot(eye_position);
		result(2, 3) = eye_front.dot(eye_position);

		return result;
	}

	/**
	 * Equivalent of: `glm::perspectiveRH_NO` (`glm::perspective`)
	 * 
	 * Ref:
	 * glm/ext/matrix_clip_space.inl
	 */
	template<typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> perspective(
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
	 * Equivalent of: `glm::ortho`
	 * 
	 * Ref:
	 * glm/ext/matrix_clip_space.inl
	 */
	template<typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> ortho(
		const _Scalar left,
		const _Scalar right,
		const _Scalar bottom,
		const _Scalar top)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		Eigen::Matrix4<_Scalar> result = Eigen::Matrix4<_Scalar>::Identity();
		result(0, 0) = static_cast<_Scalar>(2) / (right - left);
		result(1, 1) = static_cast<_Scalar>(2) / (top - bottom);
		result(2, 2) = -static_cast<_Scalar>(1);
		result(0, 3) = -(right + left) / (right - left);
		result(1, 3) = -(top + bottom) / (top - bottom);

		return result;
	}

	/**
	 * Equivalent of: `glm::ortho` (with near/far clipping planes)
	 *
	 * Unlike the 4-parameter overload above (which omits depth scaling and is meant for
	 * 2D/text rendering), this variant maps view-space z into NDC z [-1, 1] using the
	 * `zNear`/`zFar` planes, so it is suitable for a depth-correct 3D orthographic camera.
	 *
	 * Ref:
	 * glm/ext/matrix_clip_space.inl
	 */
	template<typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> ortho(
		const _Scalar left,
		const _Scalar right,
		const _Scalar bottom,
		const _Scalar top,
		const _Scalar zNear,
		const _Scalar zFar)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		Eigen::Matrix4<_Scalar> result = Eigen::Matrix4<_Scalar>::Identity();
		result(0, 0) =  static_cast<_Scalar>(2) / (right - left);
		result(1, 1) =  static_cast<_Scalar>(2) / (top - bottom);
		result(2, 2) = -static_cast<_Scalar>(2) / (zFar - zNear);
		result(0, 3) = -(right + left) / (right - left);
		result(1, 3) = -(top + bottom) / (top - bottom);
		result(2, 3) = -(zFar + zNear) / (zFar - zNear);

		return result;
	}

	/**
	 * @brief Translates a 4x4 matrix 'm' by directly adding a 3D vector 'v'
	 * to its existing translation components (the first three elements of the last column).
	 *
	 * This operation modifies only the translation part of the matrix 'm'.
	 * The translation 'v' is applied along the axes of the coordinate system
	 * in which 'm's translation component is currently expressed (e.g., world or parent space).
	 *
	 * This is different from post-multiplying by a translation matrix (m * TranslationMatrix(v)),
	 * as this function does not take into account any rotation or scaling in 'm'
	 * when determining the direction of the added translation; it's a component-wise sum
	 * for the translation part.
	 *
	 * @tparam _Scalar The scalar type (e.g., float, double). Must be a floating-point type.
	 * @param m The input 4x4 matrix (passed by value, modified, and returned).
	 * @param v The 3D translation vector to add to the matrix's translation part.
	 * @return The 4x4 matrix with 'v' added to its translation part.
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> translate_offset(
		Eigen::Matrix4<_Scalar> m, // Pass by value, as it's modified and returned
		const Eigen::Vector3<_Scalar>& v) {
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		// Directly add the vector 'v' to the translation part of matrix 'm':
		m.template block<3, 1>(0, 3) += v;

		// Alternative form:
		// m(0, 3) += v.x();
		// m(1, 3) += v.y();
		// m(2, 3) += v.z();

		return m;
	}

	/**
	 * @brief Translates a 4x4 matrix 'm' by a 3D vector 'v'. (Equivalent of: `glm::translate`)
	 * This operation is equivalent to post-multiplying 'm' by a translation matrix
	 * derived from 'v' (i.e., result = m * TranslationMatrix(v)).
	 * The translation 'v' is applied in the coordinate system defined by 'm'.
	 *
	 * Ref:
	 * glm/ext/quaternion_transform.inl
	 * 
	 * @tparam _Scalar The scalar type (e.g., float, double). Must be a floating-point type.
	 * @param m The input 4x4 matrix to be translated.
	 * @param v The 3D translation vector.
	 * @return The translated 4x4 matrix.
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> translate_local(
		const Eigen::Matrix4<_Scalar>& m,
		const Eigen::Vector3<_Scalar>& v)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		Eigen::Matrix4<_Scalar> result = m; // Start with a copy of the input matrix

		result.col(3) =
			m.col(0) * v.x() +
			m.col(1) * v.y() +
			m.col(2) * v.z() +
			m.col(3)/* * 1.0 */;

		return result;
	}

	/**
	 * @brief Rotates a 4x4 matrix 'm' around a given axis 'axis_v' by 'angle_rad'. 
	 * (Equivalent of: `glm::rotate`)
	 * This function manually constructs a 3x3 rotation matrix using Rodrigues' formula.
	 * The translation part of 'm' and the last row (0,0,0,1 for affine) are preserved.
	 * 
	 * Ref:
	 * glm/ext/matrix_transform.inl
	 *
	 * @tparam _Scalar The scalar type (e.g., float, double). Must be a floating-point type.
	 * @param m The input 4x4 matrix to be rotated.
	 * @param angle_rad The angle of rotation in radians.
	 * @param axis_v The 3D vector representing the axis of rotation. Must not be a zero vector.
	 * If it's a zero vector, the original matrix 'm' is returned.
	 * @return The rotated 4x4 matrix.
	 */
	template <typename _Scalar>
	static inline Eigen::Matrix4<_Scalar> rotate_around_axis(
		const Eigen::Matrix4<_Scalar>& m,
		const _Scalar angle_rad,
		const Eigen::Vector3<_Scalar>& axis_v)
	{
		static_assert(std::is_floating_point_v<_Scalar>, "!!");

		const _Scalar norm_sq = axis_v.squaredNorm();
		if (norm_sq < std::numeric_limits<_Scalar>::epsilon() * std::numeric_limits<_Scalar>::epsilon()) {
			return m; // Rotation axis is zero, return original matrix
		}

		const Eigen::Vector3<_Scalar> axis_normalized = axis_v / std::sqrt(norm_sq);

		const _Scalar c = std::cos(angle_rad);
		const _Scalar s = std::sin(angle_rad);
		const _Scalar one_minus_c = _Scalar(1) - c;

		const _Scalar kx = axis_normalized.x();
		const _Scalar ky = axis_normalized.y();
		const _Scalar kz = axis_normalized.z();

		Eigen::Matrix3<_Scalar> Rotate;

		// Rodrigues' rotation formula elements (R_ij = element at row i, col j)
		Rotate(0, 0) = c + kx * kx * one_minus_c;
		Rotate(0, 1) = kx * ky * one_minus_c - kz * s;
		Rotate(0, 2) = kx * kz * one_minus_c + ky * s;

		Rotate(1, 0) = ky * kx * one_minus_c + kz * s;
		Rotate(1, 1) = c + ky * ky * one_minus_c;
		Rotate(1, 2) = ky * kz * one_minus_c - kx * s;

		Rotate(2, 0) = kz * kx * one_minus_c - ky * s;
		Rotate(2, 1) = kz * ky * one_minus_c + kx * s;
		Rotate(2, 2) = c + kz * kz * one_minus_c;

		// Extract the top-left 3x3 part of m
		Eigen::Matrix3<_Scalar> m_3x3 = m.template topLeftCorner<3, 3>();

		// Perform the 3x3 matrix multiplication using Eigen's operator*
		Eigen::Matrix3<_Scalar> rotated_m_3x3 = m_3x3 * Rotate;

		// Assemble the final 4x4 matrix
		Eigen::Matrix4<_Scalar> Result;
		Result.template topLeftCorner<3, 3>() = rotated_m_3x3;

		// Copy the translation part from the original matrix m
		Result(0, 3) = m(0, 3);
		Result(1, 3) = m(1, 3);
		Result(2, 3) = m(2, 3);

		// Set the last row for a proper affine transformation matrix
		Result(3, 0) = _Scalar(0);
		Result(3, 1) = _Scalar(0);
		Result(3, 2) = _Scalar(0);
		Result(3, 3) = _Scalar(1);

		return Result;
	}

	/**
	 * Equivalent of: `glm::rotate`
	 *
	 * Ref:
	 * glm/ext/quaternion_transform.inl
	 */
	template<typename _Scalar>
	static inline Eigen::Quaternion<_Scalar> rotate_around_axis(
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

} // namespace