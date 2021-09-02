#pragma once

typedef struct VertexPacker
{
	uint8_t* base;
	uint8_t* ptr;
}VertexPacker;

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef clamp
#define clamp(a, b, c)		(min(max((a), (b)), (c)))
#endif

void packPosition(VertexPacker* packer, float x, float y, float z);
void packFrenet(VertexPacker* packer, float x, float y, float z);
void packUV(VertexPacker* packer, float u, float v);
void packJointIndices(VertexPacker* packer, uint8_t index1, uint8_t index2);
void packJointWeights(VertexPacker* packer, float weight1, float weight2);
void packRenderPart(VertexPacker* packer, float part);
void packJointIndices4x16s(VertexPacker* packer, int16_t index1, int16_t index2, int16_t index3, int16_t index4);
void packJointWeights4(VertexPacker* packer, float weight1, float weight2, float weight3, float weight4);

