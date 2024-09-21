/*
	File: math.h
	Author: Philip Haynes
*/

#ifndef AZAUDIO_MATH_H
#define AZAUDIO_MATH_H

#include <stdint.h>
#include <math.h>

#include "header_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

static const float AZA_TAU = 6.283185307179586f;
static const float AZA_PI = 3.14159265359f;

#define AZA_DEG_TO_RAD(x) ((x) * AZA_PI / 180.0f)
#define AZA_RAD_TO_DEG(x) ((x) * 180.0f / AZA_PI)

typedef union azaVec3 {
	struct {
		float x, y, z;
	};
	float data[3];
} azaVec3;

static inline azaVec3 azaAddVec3(azaVec3 lhs, azaVec3 rhs) {
	return AZA_CLITERAL(azaVec3) {
		lhs.x + rhs.x,
		lhs.y + rhs.y,
		lhs.z + rhs.z,
	};
}

static inline azaVec3 azaSubVec3(azaVec3 lhs, azaVec3 rhs) {
	return AZA_CLITERAL(azaVec3) {
		lhs.x - rhs.x,
		lhs.y - rhs.y,
		lhs.z - rhs.z,
	};
}

static inline azaVec3 azaMulVec3(azaVec3 lhs, azaVec3 rhs) {
	return AZA_CLITERAL(azaVec3) {
		lhs.x * rhs.x,
		lhs.y * rhs.y,
		lhs.z * rhs.z,
	};
}

static inline azaVec3 azaDivVec3(azaVec3 lhs, azaVec3 rhs) {
	return AZA_CLITERAL(azaVec3) {
		lhs.x / rhs.x,
		lhs.y / rhs.y,
		lhs.z / rhs.z,
	};
}

static inline azaVec3 azaMulVec3Scalar(azaVec3 lhs, float rhs) {
	return AZA_CLITERAL(azaVec3) {
		lhs.x * rhs,
		lhs.y * rhs,
		lhs.z * rhs,
	};
}

static inline azaVec3 azaDivVec3Scalar(azaVec3 lhs, float rhs) {
	return AZA_CLITERAL(azaVec3) {
		lhs.x / rhs,
		lhs.y / rhs,
		lhs.z / rhs,
	};
}

static inline float azaVec3Dot(azaVec3 lhs, azaVec3 rhs) {
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

// Euclidian norm
static inline float azaVec3Norm(azaVec3 a) {
	return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

// Euclidian norm squared
static inline float azaVec3NormSqr(azaVec3 a) {
	return a.x * a.x + a.y * a.y + a.z * a.z;
}

// 3x3 matrix with the conventions matching GLSL (same conventions as AzCore)
// - column-major memory layout
// - post-multiplication (transforms are applied in right-to-left order)
// - multiplication means lhs rows are dotted with rhs columns
// - vectors are row vectors on the lhs, and column vectors on the rhs
typedef union azaMat3 {
	struct {
		azaVec3 right, up, forward;
	};
	azaVec3 cols[3];
	float data[9];
} azaMat3;

static inline azaVec3 azaMat3Col(azaMat3 mat, uint32_t index) {
	return mat.cols[index];
}

static inline azaVec3 azaMat3Row(azaMat3 mat, uint32_t index) {
	return AZA_CLITERAL(azaVec3) {
		mat.data[index + 0],
		mat.data[index + 3],
		mat.data[index + 6],
	};
}

static inline azaVec3 azaMulMat3(azaMat3 lhs, azaMat3 rhs) {
	return AZA_CLITERAL(azaVec3) {
		lhs.data[0] * rhs.data[0] + lhs.data[3] * rhs.data[1] + lhs.data[6] * rhs.data[2],
		lhs.data[1] * rhs.data[3] + lhs.data[4] * rhs.data[4] + lhs.data[7] * rhs.data[5],
		lhs.data[2] * rhs.data[6] + lhs.data[5] * rhs.data[7] + lhs.data[8] * rhs.data[8],
	};
}

static inline azaVec3 azaMulMat3Vec3(azaMat3 lhs, azaVec3 rhs) {
	return AZA_CLITERAL(azaVec3) {
		lhs.data[0] * rhs.x + lhs.data[3] * rhs.y + lhs.data[6] * rhs.z,
		lhs.data[1] * rhs.x + lhs.data[4] * rhs.y + lhs.data[7] * rhs.z,
		lhs.data[2] * rhs.x + lhs.data[5] * rhs.y + lhs.data[8] * rhs.z,
	};
}

static inline azaVec3 azaMulVec3Mat3(azaVec3 lhs, azaMat3 rhs) {
	return AZA_CLITERAL(azaVec3) {
		azaVec3Dot(lhs, rhs.cols[0]),
		azaVec3Dot(lhs, rhs.cols[1]),
		azaVec3Dot(lhs, rhs.cols[2]),
	};
}


#ifdef __cplusplus
}
#endif
#endif // AZAUDIO_MATH_H