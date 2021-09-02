#pragma once

#include "vec4.h"

typedef vec4 plane;

plane plane_init(vec4 p0, vec4 p1, vec4 p2)
{
	// CLR - This is based on the method from Real-time Collision Detection.
	vec4 p1p0 = p1 - p0;
	vec4 p2p0 = p2 - p0;
	vec4 vec = vec4_normalize(vec4_cross(p1p0, p2p0));
	vec4 retVal = vec4_dot(vec, p0) * kVec4Origin + vec;

	return retVal;
}