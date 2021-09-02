#ifndef VEC4_H
#define VEC4_H 
#pragma once

#ifdef RE_PLATFORM_WIN64
#include <smmintrin.h>
#else
#include <arm_neon.h>
#endif

#define RE_USE_SIMD

#ifdef RE_USE_SIMD
typedef float vec4 __attribute__((ext_vector_type(4)));
//typedef float vec4 __attribute__((vector_size(4 * sizeof(float))));
typedef int32_t vec4i __attribute__((vector_size(4 * sizeof(int32_t))));
typedef float vec3 __attribute__((ext_vector_type(3)));		//CLR - Move this somewhere else more deserving of it.
typedef float vec2 __attribute__((ext_vector_type(2)));		//CLR - Move this somewhere else more deserving of it.

static const vec4 kVec4Zero = { 0.0f, 0.0f, 0.0f, 0.0f };
static const vec4 kVec4Origin = { 0.0f, 0.0f, 0.0f, 1.0f };

#ifndef RE_PLATFORM_WIN64
static const int32x4_t kMaskW = { 0xffffffff, 0xffffffff, 0xffffffff, 0 };
#endif
#else

typedef struct vec4
{
	float x;
	float y;
	float z;
	float w;
}vec4;
#endif

static inline vec4 vec4_init(const float x, const float y, const float z, const float w)
{
#ifdef RE_USE_SIMD
#ifdef RE_PLATFORM_WIN64
	return _mm_set_ps(w, z, y, x);
#else
	return (vec4) { x, y, z, w };
#endif
#else
	return (vec4) { x, y, z, w };
#endif
}

static inline vec4 vec4_load(float* address)
{
#ifdef RE_PLATFORM_WIN64
	return _mm_load_ps(address);
#else
	return (vec4) { address[0], address[1], address[2], address[3] };
#endif
}

static inline void vec4_store(float* address, const vec4 vec)
{
#ifdef RE_USE_SIMD
#ifdef RE_PLATFORM_WIN64
	_mm_store_ps(address, vec);
#else
	address[0] = vec[0];
	address[1] = vec[1];
	address[2] = vec[2];
	address[3] = vec[3];
#endif
#else
	address[0] = vec.x;
	address[1] = vec.y;
	address[2] = vec.z;
	address[3] = vec.w;
#endif
}


static inline bool vec4_compare(const vec4 v1, const vec4 v2)
{
#ifdef RE_USE_SIMD
#ifdef RE_PLATFORM_WIN64
	__m128i vcmp = _mm_cmpeq_epi32(v1, v2);
	uint16_t mask = _mm_movemask_epi8(vcmp);
	return (mask == 0xffff);
#else
	uint32x4_t vcmp = vceqq_f32(v1, v2);		// matching elements will contain 0xffffffff, not maching elements contain 0x0.
	vcmp = vcmp & (uint32x4_t) { 1, 2, 4, 8 };	// matching elements will contain 1, 2, 4 or 8.
	uint32_t retval = vaddvq_u32(vcmp);			// Add across vector will sum to 0xf if all elements are equal.
	return (retval == 0xf);
#endif
#else
	return (v1.x == v2.x && v1.y == v2.y && v1.z == v2.z && v1.w && v2.w); //false;
#endif
}

static inline bool vec4_isZero(const vec4 v)
{
	return vec4_compare(v, kVec4Zero);
}

static inline bool vec4_compareGt(const vec4 v1, const vec4 v2)
{
#ifdef RE_USE_SIMD
#ifdef RE_PLATFORM_WIN64
	__m128i vcmp = _mm_cmpgt_epi32(v1, v2);
	uint16_t mask = _mm_movemask_epi8(vcmp);
	return (mask == 0xffff);
#else
	uint32x4_t vcmp = vceqq_f32(v1, v2);		// matching elements will contain 0xffffffff, not maching elements contain 0x0.
	vcmp = vcmp & (uint32x4_t) { 1, 2, 4, 8 };	// matching elements will contain 1, 2, 4 or 8.
	uint32_t retval = vaddvq_u32(vcmp);			// Add across vector will sum to 0xf if all elements are equal.
	return (retval == 0xf);
#endif
#else
	return (v1.x == v2.x && v1.y == v2.y && v1.z == v2.z && v1.w && v2.w); //false;
#endif
}

static inline vec4 vec4_compareAll(const vec4 v1, const vec4 v2)
{
#ifdef RE_PLATFORM_WIN64
	return _mm_cmpeq_epi32(v1, v2);
#else
	(void)v1;
	(void)v2;
	return (vec4) { 0.0f, 0.0f, 0.0f, 0.0f };
#endif
}

static inline vec4 vec4_add(const vec4 v1, const vec4 v2)
{
#ifdef RE_USE_SIMD
#ifdef RE_PLATFORM_WIN64
	return _mm_add_ps(v1, v2);
#else
	return v1 + v2;
#endif
#else
	return (vec4) { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z, v1.w + v2.w };
#endif
}

static inline vec4 vec4_sub(const vec4 v1, const vec4 v2)
{
#ifdef RE_USE_SIMD
#ifdef RE_PLATFORM_WIN64
	return _mm_sub_ps(v1, v2);
#else
	return v1 - v2;;
#endif
#else
	return (vec4) { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z, v1.w - v2.w };
#endif
}

static inline vec4 vec4_vecScale(const vec4 v1, const vec4 v2)
{
	return v1 * v2;
}

static inline vec4 vec4_dot(const vec4 v1, const vec4 v2)
{
#ifdef RE_USE_SIMD
#ifdef RE_PLATFORM_WIN64
	//	const int imm8 = 0;
	return _mm_dp_ps(v1, v2, 0xff);
#else
	vec4 prod = vmulq_f32(v1, v2);
	vec4 sum1 = vaddq_f32(prod, vrev64q_f32(prod));
	vec4 sum2 = vaddq_f32(sum1, vcombine_f32(vget_high_f32(sum1), vget_low_f32(sum1)));
	return sum2;
#endif
#else
	float dp = v1.x * v2.x + v1.y * v2.y + v1.z *v2.z;
	return (vec4) { dp, dp, dp, dp };
#endif
}

static inline vec4 vec4_dot4(const vec4 v1, const vec4 v2)
{
#ifdef RE_USE_SIMD
#ifdef RE_PLATFORM_WIN64
	//	const int imm8 = 0;
	return _mm_dp_ps(v1, v2, 0xff);
#else
	vec4 prod = vmulq_f32(v1, v2);
	vec4 sum1 = vaddq_f32(prod, vrev64q_f32(prod));
	vec4 sum2 = vaddq_f32(sum1, vcombine_f32(vget_high_f32(sum1), vget_low_f32(sum1)));
	return sum2;
#endif
#else
	float dp = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	return (vec4) { dp, dp, dp, dp };
#endif
}

static inline vec4 vec4_cross(const vec4 a, const vec4 b)
{
#ifdef RE_USE_SIMD
#ifdef RE_PLATFORM_WIN64
	return _mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1)), _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2))),
		   			  _mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2)), _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1))));
#else
	float32x2_t a_xy = vget_low_f32(a);
	float32x2_t b_xy = vget_low_f32(b);
	vec4 lhs_yzx = vcombine_f32(vext_f32(a_xy, vget_high_f32(a), 1), a_xy);
	vec4 rhs_yzx = vcombine_f32(vext_f32(b_xy, vget_high_f32(b), 1), b_xy);
	// Compute cross in order zxy
	vec4 s3 = vsubq_f32(vmulq_f32(rhs_yzx, a), vmulq_f32(lhs_yzx, b));
	// Permute cross to order xyz and zero out the fourth value
	float32x2_t low = vget_low_f32(s3);
	s3 = vcombine_f32(vext_f32(low, vget_high_f32(s3), 1), low);

	return (vec4)vandq_s32((int32x4_t)s3, kMaskW);
#endif
#else
	float x = (a.y * b.z) - (a.z * b.y);
	float y = (a.z * b.x) - (a.x * b.z);
	float z = (a.x * b.y) - (a.y * b.x);
	return (vec4) {x, y, z, 0.0f};
#endif
}

static inline vec4 vec4_length(const vec4 v)
{
#ifdef RE_USE_SIMD
#ifdef RE_PLATFORM_WIN64
	// 0x7F = 0111 1111 ~ means we don't want the w-component multiplied
	// and the result written to all 4 components
	vec4 dp = _mm_dp_ps(v, v, 0x7f);

	// compute rsqrt of the dot product
	dp = _mm_sqrt_ps(dp);

	// vec * rsqrt(dot(vec, vec))
	return dp; //_mm_mul_ps(v, dp);
#else
	vec4 dp = vec4_dot(v, v);
	float32x4_t sqrt_reciprocal = vrsqrteq_f32(dp);
	dp = vrsqrtsq_f32(dp * sqrt_reciprocal, sqrt_reciprocal) * sqrt_reciprocal;

	return 1.0f / dp;
#endif
#else
	vec4 dp = vec4_dot(v, v);
	float recip = 1.0f / dp.x;

	return (vec4) { recip* v.x, recip* v.y, recip* v.z, 0.0f };
#endif
}

static inline vec4 vec4_normalize(const vec4 v)
{
#ifdef RE_USE_SIMD
#ifdef RE_PLATFORM_WIN64
	// 0x7F = 0111 1111 ~ means we don't want the w-component multiplied
	// and the result written to all 4 components
	vec4 dp = _mm_dp_ps(v, v, 0x7f); 
		
	// compute rsqrt of the dot product
	dp = _mm_rsqrt_ps(dp);

	// vec * rsqrt(dot(vec, vec))
	return _mm_mul_ps(v, dp);	
#else
	vec4 dp = vec4_dot(v, v);
	float32x4_t sqrt_reciprocal = vrsqrteq_f32(dp);
	dp = vrsqrtsq_f32(dp * sqrt_reciprocal, sqrt_reciprocal) * sqrt_reciprocal;

	return v * dp;
#endif
#else
	vec4 dp = vec4_dot(v, v);
	float recip = 1.0f / dp.x;

	return (vec4) { recip * v.x, recip * v.y, recip * v.z, 0.0f };
#endif
}

static inline vec4 vec4_lerp(vec4 v1, vec4 v2, float t)
{
	const vec4 invT = (vec4)( 1.0f - t );
	const vec4 T = (vec4)( t );

	return invT * v1 + T * v2;
}

#endif
