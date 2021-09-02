#ifndef QUAT_H
#define QUAT_H 

#pragma once

#define RE_USE_SIMD

#ifdef RE_USE_SIMD
typedef float quat __attribute__((ext_vector_type(4)));
static const quat kQuatIdentity = { 0.0f, 0.0f, 0.0f, 1.0f };
#else
typedef struct quat
{
	float x;
	float y;
	float z;
	float w;
}vec4;
#endif

static quat quat_createFromAxisAngle(vec4 axis, float angle)
{
	float halfAngle = angle * 0.5f;
	float s = sinf(halfAngle);
	quat q;
	q.x = axis.x * s;
	q.y = axis.y * s;
	q.z = axis.z * s;
	q.w = cosf(halfAngle);
	return q;
}

static quat quat_lookAt(vec4 sourcePoint, vec4 destPoint)
{
	const vec4 kVecForward = { 0.0f, 0.0f, 1.0f, 0.0f };

	vec4 forwardVector = vec4_normalize(destPoint - sourcePoint);

	float dot = vec4_dot(kVecForward, forwardVector).x;

	if (fabsf(dot - (-1.0f)) < 0.000001f)
	{
		return (vec4) { 0.0f, 1.0f, 0.0f, 3.1415926535897932f };
	}
	if (fabsf(dot - (1.0f)) < 0.000001f)
	{
		return (vec4) { 0.0f, 0.0f, 0.0f, 1.0f };
	}

	float rotAngle = acosf(dot);
	vec4 rotAxis = vec4_cross(kVecForward, forwardVector);
	rotAxis = vec4_normalize(rotAxis);
	return quat_createFromAxisAngle(rotAxis, rotAngle);
}

static quat quat_mul(vec4 left, vec4 right)
{
	const vec4 kNegW = { 1.0f, 1.0f, 1.0f, -1.0f };
	vec4 res = right.xyzw * left.wwww + (right.wwwx * kNegW) * left.xyzx + (right.zxyy * kNegW) * left.yzxy - right.yzxz * left.zxyz;

	return res;
/*
	float x = right.x * left.w + right.w * left.x + right.z * left.y - right.y * left.z;
	float y = right.y * left.w + right.w * left.y + right.x * left.z - right.z * left.x;
	float z = right.z * left.w + right.w * left.z + right.y * left.x - right.x * left.y;
	float w = right.w * left.w - right.x * left.x - right.y * left.y - right.z * left.z;
	return (quat) { x, y, z, w };
*/
}

static float quat_length(quat q)
{
	return sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
}

static quat quat_normalize(quat q)
{
	quat retQuat;
	float length = quat_length(q);

	if (length == 0)
	{
		//		retQuat.w = 1;
		//		retQuat.x = 0;
		//		retQuat.y = 0;
		//		retQuat.z = 0;
		retQuat = kQuatIdentity;
	}
	else
	{
		float inv = 1.0f / length;
		retQuat = q * inv;
		//		retQuat.x = quat.x * inv;
		//		retQuat.y = quat.y * inv;
		//		retQuat.z = quat.z * inv;
		//		retQuat.w = quat.w * inv;
	}
	return retQuat;
}

static vec4 quat_getForwardVector(quat q)
{
	float x = q.x;
	float y = q.y;
	float z = q.z;
	float w = q.w;
	float xx = x * x * 2.0f;
	float yy = y * y * 2.0f;
//	float zz = z * z * 2.0f;
//	float xy = x * y * 2.0f;
	float xz = x * z * 2.0f;
	float yz = y * z * 2.0f;
	float wx = w * x * 2.0f;
	float wy = w * y * 2.0f;
//	float wz = w * z * 2.0f;

	float m20 = xz + wy;
	float m21 = yz - wx;
	float m22 = 1.0f - xx - yy;

	return (vec4){ m20, m21, m22, 0.0f };
}

static mat4x4 quat_getMatrix4x4(quat q)
{
	mat4x4 mat;
	float x = q.x;
	float y = q.y;
	float z = q.z;
	float w = q.w;
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

	mat.xAxis = (vec4){ m00, m01, m02, 0.0f };
	mat.yAxis = (vec4){ m10, m11, m12, 0.0f };
	mat.zAxis = (vec4){ m20, m21, m22, 0.0f };
	//		mat.m_xAxis = Vector4( m00, m10, m20, 0.0f );
	//		mat.m_yAxis = Vector4( m01, m11, m21, 0.0f );
	//		mat.m_zAxis = Vector4( m02, m12, m22, 0.0f );
	mat.wAxis = (vec4){ 0.0f, 0.0f, 0.0f, 1.0f };

	return mat;
}

static quat quat_fromMat4x4(const mat4x4* mat)
{
	float tr = mat->xAxis.x + mat->yAxis.y + mat->zAxis.z;
	float qx, qy, qz, qw;
	if (tr > 0.0f) {
		float S = sqrt(tr + 1.0f) * 2; // S=4*qw 
		qw = 0.25f * S;
		qx = (mat->zAxis.y - mat->yAxis.z) / S;
		qy = (mat->xAxis.z - mat->zAxis.x) / S;
		qz = (mat->yAxis.x - mat->xAxis.y) / S;
	}
	else if ((mat->xAxis.x > mat->yAxis.y) & (mat->xAxis.x > mat->zAxis.z)) {
		float S = sqrt(1.0 + mat->xAxis.x - mat->yAxis.y - mat->zAxis.z) * 2; // S=4*qx 
		qw = (mat->zAxis.y - mat->yAxis.z) / S;
		qx = 0.25f * S;
		qy = (mat->xAxis.y + mat->yAxis.x) / S;
		qz = (mat->xAxis.z + mat->zAxis.x) / S;
	}
	else if (mat->yAxis.y > mat->zAxis.z) {
		float S = sqrt(1.0 + mat->yAxis.y - mat->xAxis.x - mat->zAxis.z) * 2; // S=4*qy
		qw = (mat->xAxis.z - mat->zAxis.x) / S;
		qx = (mat->xAxis.y + mat->yAxis.x) / S;
		qy = 0.25f * S;
		qz = (mat->yAxis.z + mat->zAxis.y) / S;
	}
	else {
		float S = sqrt(1.0 + mat->zAxis.z - mat->xAxis.x - mat->yAxis.y) * 2; // S=4*qz
		qw = (mat->yAxis.x - mat->xAxis.y) / S;
		qx = (mat->xAxis.z + mat->zAxis.x) / S;
		qy = (mat->yAxis.z + mat->zAxis.y) / S;
		qz = 0.25f * S;
	}
	return (quat) { qx, qy, qz, qw };
}

#endif
