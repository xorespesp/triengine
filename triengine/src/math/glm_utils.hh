#pragma once
#include "../common.h"

namespace triengine::math
{
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