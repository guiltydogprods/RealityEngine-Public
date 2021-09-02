#include "stdafx.h"
#include "vertexPacker.h"

void packPosition(VertexPacker* packer, float x, float y, float z)
{
	float* destPtr = (float*)packer->ptr;
	destPtr[0] = x;
	destPtr[1] = y;
	destPtr[2] = z;
	packer->ptr += (sizeof(float) * 3);
}

void packFrenet(VertexPacker* packer, float x, float y, float z)
{
	uint32_t* packedPtr = (uint32_t*)packer->ptr;
	*packedPtr = 0;
	//	AssertMsg((fabsf(x) <= 1.0f), "X component is greater than 1.0f\n");
	//	AssertMsg((fabsf(y) <= 1.0f), "Y component is greater than 1.0f\n");
	//	AssertMsg((fabsf(z) <= 1.0f), "Z component is greater than 1.0f\n");
	int32_t xInt = (int32_t)(x * (powf(2.0f, 10.0f - 1.0f) - 1.0f));
	int32_t yInt = (int32_t)(y * (powf(2.0f, 10.0f - 1.0f) - 1.0f));
	int32_t zInt = (int32_t)(z * (powf(2.0f, 10.0f - 1.0f) - 1.0f));
	*packedPtr = (zInt & 0x3ff) << 20 | (yInt & 0x3ff) << 10 | (xInt & 0x3ff);
	//		*packedPtr = (xInt & 0x3ff) << 20 | (yInt & 0x3ff) << 10 | (zInt & 0x3ff);
	packer->ptr += sizeof(uint32_t);
}

void packUV(VertexPacker* packer, float u, float v)
{
	float* destPtr = (float*)packer->ptr;
	destPtr[0] = u;
	destPtr[1] = v;
	packer->ptr += (sizeof(float) * 2);

}

void packJointIndices(VertexPacker* packer, uint8_t index1, uint8_t index2)
{
	uint8_t* destPtr = (uint8_t*)packer->ptr;
	destPtr[0] = index1;
	destPtr[1] = index2;
	packer->ptr += 2;
}

void packJointIndices4x16s(VertexPacker* packer, int16_t index1, int16_t index2, int16_t index3, int16_t index4)
{
	uint8_t* destPtr = (uint8_t*)packer->ptr;
	assert(index1 < 256);
	destPtr[0] = (uint8_t)index1;
	assert(index2 < 256);
	destPtr[1] = (uint8_t)index2;
	assert(index3 < 256);
	destPtr[2] = (uint8_t)index3;
	assert(index4 < 256);
	destPtr[3] = (uint8_t)index4;
	packer->ptr += 4;
}

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef clamp
#define clamp(a, b, c)		(min(max((a), (b)), (c)))
#endif

void packJointWeights(VertexPacker* packer, float weight1, float weight2)
{
	uint8_t* destPtr = (uint8_t*)packer->ptr;
	//	weight1 min(max(weight1, 0.0f), 1.0f);
	weight1 = clamp(weight1, 0.0f, 1.0f) * 255.0f;
	weight2 = clamp(weight2, 0.0f, 1.0f) * 255.0f;
	destPtr[0] = (uint8_t)weight1;
	destPtr[1] = (uint8_t)weight2;
	packer->ptr += 2;
}

void packJointWeights4(VertexPacker* packer, float weight1, float weight2, float weight3, float weight4)
{
	uint8_t* destPtr = (uint8_t*)packer->ptr;
	//	weight1 min(max(weight1, 0.0f), 1.0f);
	weight1 = clamp(weight1, 0.0f, 1.0f) * 255.0f;
	weight2 = clamp(weight2, 0.0f, 1.0f) * 255.0f;
	weight3 = clamp(weight3, 0.0f, 1.0f) * 255.0f;
	weight4 = clamp(weight4, 0.0f, 1.0f) * 255.0f;
	destPtr[0] = (uint8_t)weight1;
	destPtr[1] = (uint8_t)weight2;
	destPtr[2] = (uint8_t)weight3;
	destPtr[3] = (uint8_t)weight4;
	packer->ptr += 4;
}

void packRenderPart(VertexPacker* packer, float part)
{
	uint8_t* destPtr = (uint8_t*)packer->ptr;
	part *= 4.0f;
	part = clamp(roundf(part), 0.0f, 4.0f);
	destPtr[0] = (uint8_t)part;
	destPtr[1] = 0;
	destPtr[2] = 0;
	destPtr[3] = 0;
	packer->ptr += 4;
}
