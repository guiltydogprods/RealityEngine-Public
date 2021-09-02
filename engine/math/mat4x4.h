#pragma once

#include "vec4.h"
#include <math.h>

#define RE_USE_SIMD

typedef struct mat4x4
{
	union
	{
		struct
		{
			vec4 xAxis;
			vec4 yAxis;
			vec4 zAxis;
			vec4 wAxis;
		};
		vec4 axis[4];
	};
} mat4x4 __attribute__ ((aligned(16)));

#define MakeShuffleMask(x,y,z,w)           (x | (y<<2) | (z<<4) | (w<<6))

// vec(0, 1, 2, 3) -> (vec[x], vec[y], vec[z], vec[w])
#define VecSwizzleMask(vec, mask)          _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(vec), mask))
#define VecSwizzle(vec, x, y, z, w)        VecSwizzleMask(vec, MakeShuffleMask(x,y,z,w))
#define VecSwizzle1(vec, x)                VecSwizzleMask(vec, MakeShuffleMask(x,x,x,x))
// special swizzle
#define VecSwizzle_0022(vec)               _mm_moveldup_ps(vec)
#define VecSwizzle_1133(vec)               _mm_movehdup_ps(vec)

// return (vec1[x], vec1[y], vec2[z], vec2[w])
#define VecShuffle(vec1, vec2, x,y,z,w)    _mm_shuffle_ps(vec1, vec2, MakeShuffleMask(x,y,z,w))
// special shuffle
#define VecShuffle_0101(vec1, vec2)        _mm_movelh_ps(vec1, vec2)
#define VecShuffle_2323(vec1, vec2)        _mm_movehl_ps(vec2, vec1)

static inline mat4x4 mat4x4_identity()
{
	static mat4x4 s_identity = {
		.xAxis = (vec4) {1.0f, 0.0f, 0.0f, 0.0f},
		.yAxis = (vec4) {0.0f, 1.0f, 0.0f, 0.0f},
		.zAxis = (vec4) {0.0f, 0.0f, 1.0f, 0.0f},
		.wAxis = (vec4) {0.0f, 0.0f, 0.0f, 1.0f},
	};

	return s_identity;
}

static inline mat4x4 mat4x4_transpose(const mat4x4 mat)
{
	vec4 xAxis = mat.xAxis;
	vec4 yAxis = mat.yAxis;
	vec4 zAxis = mat.zAxis;
	vec4 wAxis = mat.wAxis;

#ifdef RE_PLATFORM_WIN64
	_MM_TRANSPOSE4_PS(xAxis, yAxis, zAxis, wAxis);
	return (mat4x4) {
		.xAxis = xAxis,
		.yAxis = yAxis,
		.zAxis = zAxis,
		.wAxis = wAxis,
	};
#else
	return (mat4x4) {
		.xAxis = { xAxis.x, yAxis.x, zAxis.x, wAxis.x },
		.yAxis = { xAxis.y, yAxis.y, zAxis.y, wAxis.y },
		.zAxis = { xAxis.z, yAxis.z, zAxis.z, wAxis.z },
		.wAxis = { xAxis.w, yAxis.w, zAxis.w, wAxis.w }
	};
#endif
}

static inline mat4x4 mat4x4_orthoInverse(const mat4x4 mat)
{
	mat4x4 result;
#ifdef RE_PLATFORM_WIN64
	// transpose 3x3, we know m03 = m13 = m23 = 0
	__m128 t0 = VecShuffle_0101(mat.xAxis, mat.yAxis); // 00, 01, 10, 11
	__m128 t1 = VecShuffle_2323(mat.xAxis, mat.yAxis); // 02, 03, 12, 13
	result.xAxis = VecShuffle(t0, mat.zAxis, 0,2,0,3); // 00, 10, 20, 23(=0)
	result.yAxis = VecShuffle(t0, mat.zAxis, 1,3,1,3); // 01, 11, 21, 23(=0)
	result.zAxis = VecShuffle(t1, mat.zAxis, 0,2,2,3); // 02, 12, 22, 23(=0)

	// last line
	result.wAxis =                           _mm_mul_ps(result.xAxis, VecSwizzle1(mat.wAxis, 0));
	result.wAxis = _mm_add_ps(result.wAxis, _mm_mul_ps(result.yAxis, VecSwizzle1(mat.wAxis, 1)));
	result.wAxis = _mm_add_ps(result.wAxis, _mm_mul_ps(result.zAxis, VecSwizzle1(mat.wAxis, 2)));
	result.wAxis = _mm_sub_ps(_mm_setr_ps(0.f, 0.f, 0.f, 1.f), result.wAxis);
#else
	vec4 xAxis = (vec4){ mat.xAxis[0], mat.yAxis[0], mat.zAxis[0], 0.0f }; //CLR: 3x3 transpose should be SIMDfied.
	vec4 yAxis = (vec4){ mat.xAxis[1], mat.yAxis[1], mat.zAxis[1], 0.0f };
	vec4 zAxis = (vec4){ mat.xAxis[2], mat.yAxis[2], mat.zAxis[2], 0.0f };
	float32x2_t w_xy = vget_low_f32(mat.wAxis);
	float32x2_t w_zw = vget_high_f32(mat.wAxis);
	vec4 xxxx = vdupq_lane_f32(w_xy, 0);
	vec4 yyyy = vdupq_lane_f32(w_xy, 1);
	vec4 zzzz = vdupq_lane_f32(w_zw, 0);
	vec4 wAxis = xAxis * xxxx;
	wAxis = wAxis + yAxis * yyyy;
	wAxis = wAxis + zAxis * zzzz;
	result.xAxis = xAxis;
	result.yAxis = yAxis;
	result.zAxis = zAxis;
	result.wAxis = (vec4) {0.0f, 0.0f, 0.0f, 1.0f} - wAxis;
#endif
	return result;
}

#if 0 // CLR - This is the matrix inverse from Foundations of Game Engine Development vol. 1 - Mathematics but it's not working right now.
static inline mat4x4 mat4x4_inverse(const mat4x4 mat)
{
	const vec4 xAxis = mat.xAxis;
	const vec4 yAxis = mat.yAxis;
	const vec4 zAxis = mat.zAxis;
	const vec4 wAxis = mat.wAxis;

	const vec4 a = (vec4){ xAxis.x, xAxis.y, xAxis.z, 0.0f };
	const vec4 b = (vec4){ yAxis.x, yAxis.y, yAxis.z, 0.0f };
	const vec4 c = (vec4){ zAxis.x, yAxis.y, zAxis.z, 0.0f };
	const vec4 d = (vec4){ wAxis.x, wAxis.y, wAxis.z, 0.0f };

	float x = wAxis.x;
	float y = wAxis.y;
	float z = wAxis.z;
	float w = wAxis.w;

	vec4 s = vec4_cross(a, b);
	vec4 t = vec4_cross(c, d);
	vec4 u = a * y - b * x;
	vec4 v = c * w - d * z;
	float invDet = 1.0f / (vec4_dot(s, v).x, vec4_dot(t, u).x);
	s *= invDet;
	t *= invDet;
	u *= invDet;
	v *= invDet;

	vec4 r0 = vec4_cross(b, v) + t * y;
	vec4 r1 = vec4_cross(v, a) - t * x;
	vec4 r2 = vec4_cross(d, u) + s * w;
	vec4 r3 = vec4_cross(u, c) - s * z;

	mat4x4 retVal;
	retVal.xAxis = (vec4){ r0.x, r0.y, r0.z, -vec4_dot(b, t).x };
	retVal.yAxis = (vec4){ r1.x, r1.y, r1.z,  vec4_dot(a, t).x };
	retVal.zAxis = (vec4){ r2.x, r2.y, r2.z, -vec4_dot(d, s).x };
	retVal.wAxis = (vec4){ r3.x, r3.y, r3.z,  vec4_dot(c, s).x };

	return retVal;
}
#endif

static inline mat4x4 mat4x4_inverse(const mat4x4 mat)
{
	mat4x4 srcMat = {
		.xAxis = mat.xAxis,
		.yAxis = mat.yAxis,
		.zAxis = mat.zAxis,
		.wAxis = mat.wAxis,
	};

	mat4x4 resMat = mat4x4_identity();

	for (int32_t i = 0; i < 4; ++i)
	{
		const float invDiag = 1.0f / srcMat.axis[i][i];
		resMat.axis[i] *= invDiag;
		srcMat.axis[i] *= invDiag;

		for (int32_t j = 0; j < 4; ++j)
		{
			const float notDiag = j != i ? srcMat.axis[j][i] : 0.0f;
			resMat.axis[j] -= resMat.axis[i] * notDiag;
			srcMat.axis[j] -= srcMat.axis[i] * notDiag;
		}
	}
	return resMat;
}

static inline mat4x4 mat4x4_inverseSlow(const mat4x4 mat)
{
	mat4x4 retVal;
	float* data = (float*)&mat;			//CLR - This trashes the source matrix so ensure we are passing in a copy, not the original.
	float* res_ptr = (float*)&retVal;
	memset(res_ptr, 0, sizeof(mat4x4));
	res_ptr[0] = 1.0f;
	res_ptr[5] = 1.0f;
	res_ptr[10] = 1.0f;
	res_ptr[15] = 1.0f;

	int i, j, k;

	for (i = 0; i < 4; ++i)
	{
		float tmp = 1.0f / data[i * 4 + i];
		for (j = 3; j >= 0; --j)
		{
			res_ptr[i * 4 + j] *= tmp;
			data[i * 4 + j] *= tmp;
		}
		for (j = 0; j < 4; ++j)
		{
			if (j != i)
			{
				tmp = data[j * 4 + i];
				for (k = 3; k >= 0; k--)
				{
					res_ptr[j * 4 + k] -= res_ptr[i * 4 + k] * tmp;
					data[j * 4 + k] -= data[i * 4 + k] * tmp;
				}
			}
		}
	}
	return retVal;
}


static inline mat4x4 mat4x4_lookAt(const vec4 eye, const vec4 at, const vec4 up)
{
	const vec4 zAxis = vec4_normalize(eye - at);
	const vec4 xAxis = vec4_normalize(vec4_cross(up, zAxis));
	const vec4 yAxis = vec4_cross(zAxis, xAxis);
	
	mat4x4 worldMatrix = {
		.xAxis = xAxis,
		.yAxis = yAxis,
		.zAxis = zAxis,
		.wAxis = eye
	};
	return mat4x4_orthoInverse(worldMatrix);
}

static inline mat4x4 mat4x4_lookAtWorld(const vec4 eye, const vec4 at, const vec4 up)
{
	const vec4 zAxis = vec4_normalize(eye - at);
	const vec4 xAxis = vec4_normalize(vec4_cross(up, zAxis));
	const vec4 yAxis = vec4_cross(zAxis, xAxis);

	return (mat4x4) {
		.xAxis = xAxis,
		.yAxis = yAxis,
		.zAxis = zAxis,
		.wAxis = eye
	};
}

static inline mat4x4 mat4x4_create(vec4 rot, vec4 position)
{
	mat4x4 retVal;
	float x = rot[0];
	float y = rot[1];
	float z = rot[2];
	float w = rot[3];
	float xx = x * x * 2.0f;
	float yy = y * y * 2.0f;
	float zz = z * z * 2.0f;
	float xy = x * y * 2.0f;
	float xz = x * z * 2.0f;
	float yz = y * z * 2.0f;
	float wx = w * x * 2.0f;
	float wy = w * y * 2.0f;
	float wz = w * z * 2.0f;

	float m00 = 1.0f - yy - zz;
	float m01 = xy + wz;
	float m02 = xz - wy;
	float m10 = xy - wz;
	float m11 = 1.0f - xx - zz;
	float m12 = yz + wx;
	float m20 = xz + wy;
	float m21 = yz - wx;
	float m22 = 1.0f - xx - yy;

	retVal.xAxis = (vec4) { m00, m01, m02, 0.0f };
	retVal.yAxis = (vec4) { m10, m11, m12, 0.0f };
	retVal.zAxis = (vec4) { m20, m21, m22, 0.0f };
	retVal.wAxis = position;

	return retVal;
}

static inline mat4x4 mat4x4_frustum(const float left, const float right, const float bottom, const float top, const float nearZ, const float farZ)
{
	float A = (right + left) / (right - left);
	float B = (top + bottom) / (top - bottom);
	float C = -(farZ + nearZ) / (farZ - nearZ);
	float D = -(2.0f * farZ * nearZ) / (farZ - nearZ);

	vec4 xAxis = { 2.0f * nearZ / (right - left), 0.0f, 0.0f, 0.0f };
	vec4 yAxis = { 0.0f, 2.0f * nearZ / (top - bottom), 0.0f, 0.0f };		
	vec4 zAxis = { A, B, C, -1.0f };
	vec4 wAxis = { 0.0f, 0.0f, D, 0.0 };

	mat4x4 result;
	result.xAxis = xAxis;
	result.yAxis = yAxis;
	result.zAxis = zAxis;
	result.wAxis = wAxis;

	return result;
}

static inline mat4x4 mat4x4_translate(const vec4 translate)
{
	mat4x4 resMat;
	resMat.xAxis = (vec4){ 1.0f, 0.0f, 0.0f, 0.0f };
	resMat.yAxis = (vec4){ 0.0f, 1.0f, 0.0f, 0.0f };
	resMat.zAxis = (vec4){ 0.0f, 0.0f, 1.0f, 0.0f };
	resMat.wAxis = translate;

	return resMat;
}

static inline mat4x4 mat4x4_rotate(const float angle, const vec4 axis)
{
	float ax = axis[0];
	float ay = axis[1];
	float az = axis[2];
	float xx = ax * ax;
	float yy = ay * ay;
	float zz = az * az;
	
	float sina = sinf(angle);
	float cosa = cosf(angle);
		
	float oneMinusC = 1.0f - cosa;
		
	float m00 = cosa + xx * oneMinusC;
	float m01 = ax * ay * oneMinusC + az * sina;
	float m02 = ax * az * oneMinusC - ay * sina;
		
	float m10 = ax * ay * oneMinusC - az * sina;
	float m11 = cosa + yy * oneMinusC;
	float m12 = ay * az * oneMinusC + ax * sina;
		
	float m20 = ax * az * oneMinusC + ay * sina;
	float m21 = ay * az * oneMinusC - ax * sina;
	float m22 = cosa + zz * oneMinusC;

	mat4x4 result;
	result.xAxis = (vec4) { m00, m01, m02, 0.0f };
	result.yAxis = (vec4) { m10, m11, m12, 0.0f };
	result.zAxis = (vec4) { m20, m21, m22, 0.0f };
	result.wAxis = (vec4) { 0.0f, 0.0f, 0.0f, 1.0f };

	return result;
}

static inline mat4x4 mat4x4_scale(const float scale)
{
	mat4x4 resMat;
	resMat.xAxis = (vec4){ scale, 0.0f, 0.0f, 0.0f };
	resMat.yAxis = (vec4){ 0.0f, scale, 0.0f, 0.0f };
	resMat.zAxis = (vec4){ 0.0f, 0.0f, scale, 0.0f };
	resMat.wAxis = (vec4){ 0.0f, 0.0f, 0.0f, 1.0f };

	return resMat;
}

static inline mat4x4 mat4x4_mul(const mat4x4 A, const mat4x4 B)
{
	mat4x4 result;

	vec4 a_xAxis = A.xAxis;
	vec4 a_yAxis = A.yAxis;
	vec4 a_zAxis = A.zAxis;
	vec4 a_wAxis = A.wAxis;

	for (uint32_t i = 0; i < 4; ++i)
	{
#ifdef RE_PLATFORM_WIN64
		const float *vec = (const float*)&B.axis[i];
		vec4 b_xxxx = _mm_set1_ps(vec[0]);
		vec4 b_yyyy = _mm_set1_ps(vec[1]);
		vec4 b_zzzz = _mm_set1_ps(vec[2]);
		vec4 b_wwww = _mm_set1_ps(vec[3]);
#else
		vec4 b_xxxx = vdupq_laneq_f32(B.axis[i], 0);
		vec4 b_yyyy = vdupq_laneq_f32(B.axis[i], 1);
		vec4 b_zzzz = vdupq_laneq_f32(B.axis[i], 2);
		vec4 b_wwww = vdupq_laneq_f32(B.axis[i], 3);

#endif
		vec4 acc = a_xAxis * b_xxxx;
		acc = acc + a_yAxis * b_yyyy;
		acc = acc + a_zAxis * b_zzzz;
		result.axis[i] = acc + a_wAxis * b_wwww;
	}
	return result;
}

static inline vec4 vec4_transform(mat4x4 mat, vec4 vec)
{
	vec4 acc = mat.wAxis * vec.w;
	acc = acc + mat.zAxis * vec.z;
	acc = acc + mat.yAxis * vec.y;
	vec4 res = acc + mat.xAxis * vec.x;

	return res;
}
