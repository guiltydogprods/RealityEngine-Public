#include "stdafx.h"
#include "debugRender.h"
#include "renderer.h"
#include "rgfx/vertexPacker.h"
#include "stb/stretchy_buffer.h"

//#define DISABLE_DEBUG_RENDER
#define ION_PI (3.14159265359f)
#define ION_180_OVER_PI (180.0f / ION_PI)
#define ION_PI_OVER_180 (ION_PI / 180.0f)

#define DEGREES(x) ((x) * ION_180_OVER_PI)
#define RADIANS(x) ((x) * ION_PI_OVER_180)

#ifdef RE_PLATFORM_WIN64
const int32_t kNumEyePasses = 2;
#else
const int32_t kNumEyePasses = 1;
#endif

static rgfx_pipeline s_debugRenderPipeline = { 0 };
static rgfx_vertexBuffer s_debugLineVertexBuffer = { 0 };
static rgfx_vertexBuffer s_debugPrimVertexBuffer = { 0 };
static rgfx_renderable s_rend = { 0 };
static rgfx_debugLine* s_debugLineArray = { 0 };
//static uint32_t s_indexCount = 0;

void rgfx_debugRenderInitialize(const rgfx_debugRenderInitParams* params)
{
#ifdef DISABLE_DEBUG_RENDER
	return;
#endif
	assert(s_debugLineArray == NULL);

	s_debugRenderPipeline = rgfx_createPipeline(&(rgfx_pipelineDesc) {
		.vertexShader = rgfx_loadShader("shaders/debug.vert", kVertexShader, 0),
		.fragmentShader = rgfx_loadShader("shaders/debug.frag", kFragmentShader, 0),
	});

	uint32_t vertexStride = 0;
	{
		rgfx_vertexElement vertexStreamElements[] =
		{
			{ kFloat, 3, false },
			{ kUnsignedByte, 4, true },
		};
		int32_t numElements = STATIC_ARRAY_SIZE(vertexStreamElements);
		rgfx_vertexFormat vertexFormat = rgfx_registerVertexFormat(numElements, vertexStreamElements);
		vertexStride = rgfx_getVertexFormatInfo(vertexFormat)->stride;
		s_debugLineVertexBuffer = rgfx_createVertexBuffer(&(rgfx_vertexBufferDesc) {
			.format = vertexFormat,
			.capacity = 64 * 1024,
		});
	}
	{
		rgfx_vertexElement vertexStreamElements[] =
		{
			{ kFloat, 3, false },
			{ kInt2_10_10_10_Rev, 4, true },
//			{ kInt2_10_10_10_Rev, 4, true },
//			{ kInt2_10_10_10_Rev, 4, true },
//			{ kFloat, 2, false },
		};
		int32_t numElements = STATIC_ARRAY_SIZE(vertexStreamElements);
		rgfx_vertexFormat vertexFormat = rgfx_registerVertexFormat(numElements, vertexStreamElements);
		vertexStride = rgfx_getVertexFormatInfo(vertexFormat)->stride;
		s_debugPrimVertexBuffer = rgfx_createVertexBuffer(&(rgfx_vertexBufferDesc) {
			.format = vertexFormat,
			.capacity = 64 * 1024,
			.indexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
				.type = kIndexBuffer,
				.capacity = 64 * 1024,
				.stride = sizeof(uint16_t),
				.flags = kMapWriteBit | kDynamicStorageBit
			})
		});
	}
//	rgfx_createDebugSphere(0.5f, 12, 12);
	s_rend = rgfx_createDebugCapsule(0.5f, 1.0f, 10, 7);
}

void rgfx_drawDebugLine(vec3 p1, rgfx_debugColor col1, vec3 p2, rgfx_debugColor col2)
{
#ifdef DISABLE_DEBUG_RENDER
	return;
#endif
	rgfx_debugLine* lineEntry = sb_add(s_debugLineArray, 1);
	assert(lineEntry != NULL);
	lineEntry->p1 = p1;
	lineEntry->p2 = p2;
	lineEntry->col1 = col1;
	lineEntry->col2 = col2;
}

extern GLint g_viewIdLoc;

void rgfx_drawDebugLines(int32_t eye, bool bHmdMounted)
{
#ifdef DISABLE_DEBUG_RENDER
	return;
#endif
	int32_t numLines = sb_count(s_debugLineArray);
	if (numLines == 0)
		goto skipLines; // return;

	rgfx_buffer buffer = rgfx_getVertexBufferBuffer(s_debugLineVertexBuffer, kVertexBuffer);
	float *data = (float*)rgfx_mapBuffer(buffer);

	for (int32_t i = 0; i < numLines; ++i)
	{
		*(data++) = s_debugLineArray[i].p1.x;
		*(data++) = s_debugLineArray[i].p1.y;
		*(data++) = s_debugLineArray[i].p1.z;
		*((rgfx_debugColor*)data++) = s_debugLineArray[i].col1;
		*(data++) = s_debugLineArray[i].p2.x;
		*(data++) = s_debugLineArray[i].p2.y;
		*(data++) = s_debugLineArray[i].p2.z;
		*((rgfx_debugColor*)data++) = s_debugLineArray[i].col2;
	}
	rgfx_unmapBuffer(buffer);

	rgfx_bindVertexBuffer(s_debugLineVertexBuffer);

//	glDisable(GL_DEPTH_TEST);
//	glDisable(GL_FRAMEBUFFER_SRGB);
	rgfx_bindPipeline(s_debugRenderPipeline);

	GLuint program = rgfx_getPipelineProgram(s_debugRenderPipeline, kVertexShader);
	if (g_viewIdLoc >= 0)  // NOTE: will not be present when multiview path is enabled.
	{
		glProgramUniform1i(program, g_viewIdLoc, eye);
	}

	rgfx_bindBuffer(1, rgfx_getTransforms());

	glDrawArrays(GL_LINES, 0, numLines * 2);
	int32_t lastEyeIndex = kNumEyePasses - 1;
	assert(lastEyeIndex >= 0);
	if (!bHmdMounted || bHmdMounted && eye == lastEyeIndex) {
		sb_free(s_debugLineArray);
		s_debugLineArray = NULL;
	}

skipLines:
#if 1
	rgfx_bindVertexBuffer(s_debugPrimVertexBuffer);

	rgfx_bindPipeline(s_debugRenderPipeline);

	program = rgfx_getPipelineProgram(s_debugRenderPipeline, kVertexShader);
	if (g_viewIdLoc >= 0)  // NOTE: will not be present when multiview path is enabled.
	{
		glProgramUniform1i(program, g_viewIdLoc, eye);
	}

	rgfx_bindBuffer(1, rgfx_getTransforms());

//	rgfx_drawRenderableInstanced(s_rend, 1);

	rgfx_unbindVertexBuffer();
#endif
//	glEnable(GL_DEPTH_TEST);
//	glEnable(GL_FRAMEBUFFER_SRGB);
}

vec3 vertices[64000];
void rgfx_drawDebugSphere(float radius, const uint32_t numSlices, const uint32_t numStacks, rgfx_debugColor col)
{
	uint32_t vertexCount = 0;
	for (uint32_t stackNumber = 0; stackNumber <= numStacks; ++stackNumber)
	{
		for (uint32_t sliceNumber = 0; sliceNumber < numSlices; ++sliceNumber)
		{
			float theta = stackNumber * ION_PI / numStacks;
			float phi = sliceNumber * 2 * ION_PI / numSlices;
			float sinTheta = sinf(theta);
			float sinPhi = sinf(phi);
			float cosTheta = cosf(theta);
			float cosPhi = cosf(phi);
		
//			vec4 position = { radius * cosPhi * sinTheta, 1.0f + (radius * sinPhi * sinTheta), radius * cosTheta, 0.0f };		//Poles along Z axis
			vec4 position = { radius * cosPhi * sinTheta, 1.0f + radius * cosTheta, radius * sinPhi * sinTheta, 0.0f };			//Poles along Y axis
			vertices[vertexCount++] = position.xyz;

//			Vector4 colour = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
//			m_colours.push_back(colour);
//			Vector4 normal = vertex;		//Vec4( vertex.m_x, vertex.m_y, vertex.m_z, 0.0f );
//			m_normals.push_back(normal);
//			float u = asin(normal.X()) / ion::kPi + 0.5f;
//			float v = asin(normal.Y()) / ion::kPi + 0.5f;
//			Vector4 texCoord = Vector4(u, v, 0.0f, 0.0f);
//			m_texCoords.push_back(texCoord);
		}
	}
	for (unsigned int stackNumber = 0; stackNumber < numStacks; ++stackNumber)
	{
		for (unsigned int sliceNumber = 0; sliceNumber <= numSlices; ++sliceNumber)
		{
			uint32_t v1 = (stackNumber * numSlices) + (sliceNumber % numSlices);
			uint32_t v2 = ((stackNumber + 1) * numSlices) + (sliceNumber % numSlices);
			rgfx_debugColor col = kDebugColorGreen;
			rgfx_drawDebugLine(vertices[v1], col, vertices[v2], col);

			uint32_t v3 = v1; // (stackNumber * numSlices) + ((sliceNumber + 1) % numSlices);
			uint32_t v4 = ((stackNumber) * numSlices) + ((sliceNumber + 1) % numSlices);
			rgfx_drawDebugLine(vertices[v3], col, vertices[v4], col);
		}
	}
}

void rgfx_drawDebugAABB(vec3 aabbMin, vec3 aabbMax)
{
	vec4 points[8];
	points[0] = (vec4){ aabbMin.x, aabbMin.y, aabbMin.z, 1.0f };
	points[1] = (vec4){ aabbMax.x, aabbMin.y, aabbMin.z, 1.0f };
	points[2] = (vec4){ aabbMin.x, aabbMax.y, aabbMin.z, 1.0f };
	points[3] = (vec4){ aabbMax.x, aabbMax.y, aabbMin.z, 1.0f };
	points[4] = (vec4){ aabbMin.x, aabbMin.y, aabbMax.z, 1.0f };
	points[5] = (vec4){ aabbMax.x, aabbMin.y, aabbMax.z, 1.0f };
	points[6] = (vec4){ aabbMin.x, aabbMax.y, aabbMax.z, 1.0f };
	points[7] = (vec4){ aabbMax.x, aabbMax.y, aabbMax.z, 1.0f };

	rgfx_drawDebugLine(points[0].xyz, kDebugColorGrey, points[1].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(points[2].xyz, kDebugColorGrey, points[3].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(points[4].xyz, kDebugColorGrey, points[5].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(points[6].xyz, kDebugColorGrey, points[7].xyz, kDebugColorGrey);

	rgfx_drawDebugLine(points[0].xyz, kDebugColorGrey, points[2].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(points[1].xyz, kDebugColorGrey, points[3].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(points[4].xyz, kDebugColorGrey, points[6].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(points[5].xyz, kDebugColorGrey, points[7].xyz, kDebugColorGrey);

	rgfx_drawDebugLine(points[0].xyz, kDebugColorGrey, points[4].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(points[1].xyz, kDebugColorGrey, points[5].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(points[2].xyz, kDebugColorGrey, points[6].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(points[3].xyz, kDebugColorGrey, points[7].xyz, kDebugColorGrey);
}

void rgfx_createDebugSphere(float radius, const uint32_t numSlices, const uint32_t numStacks)
{
	rgfx_bufferInfo* bufferInfo = rgfx_getBufferInfo(rgfx_getVertexBufferBuffer(s_debugPrimVertexBuffer, kVertexBuffer));
	uint32_t vertexStride = bufferInfo->stride;

//	for (uint32_t i = 0; i < vertexCount; ++i)
//	{
//		packPosition(&packer, mesh->VertexPositions[i].x, mesh->VertexPositions[i].y, mesh->VertexPositions[i].z);
//		packFrenet(&packer, mesh->VertexNormals[i].x, mesh->VertexNormals[i].y, mesh->VertexNormals[i].z);
//		packUV(&packer, mesh->VertexUV0[i].x, 1.0f - mesh->VertexUV0[i].y);
//		packJointIndices4x16s(&packer, mesh->BlendIndices[i].x, mesh->BlendIndices[i].y, mesh->BlendIndices[i].z, mesh->BlendIndices[i].w);
//		packJointWeights4(&packer, mesh->BlendWeights[i].x, mesh->BlendWeights[i].y, mesh->BlendWeights[i].z, mesh->BlendWeights[i].w);
//	}
//	uint32_t firstVertex = rgfx_writeVertexData(s_debugPrimVertexBuffer, vertexStride * vertexCount, packer.base);
//	free(memory);

	uint32_t vertexCount = (numStacks + 1) * numSlices;
	VertexPacker packer;
	void* memory = malloc(vertexStride * vertexCount);
//	memset(memory, 0, vertexStride * vertexCount);
	packer.base = packer.ptr = memory;

	for (uint32_t stackNumber = 0; stackNumber <= numStacks; ++stackNumber)
	{
		for (uint32_t sliceNumber = 0; sliceNumber < numSlices; ++sliceNumber)
		{
			float theta = stackNumber * ION_PI / numStacks;
			float phi = sliceNumber * 2 * ION_PI / numSlices;
			float sinTheta = sinf(theta);
			float sinPhi = sinf(phi);
			float cosTheta = cosf(theta);
			float cosPhi = cosf(phi);

//			vec4 position = { radius * cosPhi * sinTheta, 1.0f + (radius * sinPhi * sinTheta), radius * cosTheta, 0.0f };		//Poles along Z axis
			vec4 position = { radius * cosPhi * sinTheta, 1.0f + radius * cosTheta, radius * sinPhi * sinTheta, 0.0f };			//Poles along Y axis

//			vertices[vertexCount++] = vertex;
			packPosition(&packer, position.x, position.y, position.z);
			vec4 normal = vec4_normalize(position);
			packFrenet(&packer, 1.0f, 0.0f, 1.0f); // normal.x, normal.y, normal.z);

//			Vector4 colour = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
//			m_colours.push_back(colour);
//			Vector4 normal = vertex;		//Vec4( vertex.m_x, vertex.m_y, vertex.m_z, 0.0f );
//			m_normals.push_back(normal);
//			float u = asin(normal.X()) / ion::kPi + 0.5f;
//			float v = asin(normal.Y()) / ion::kPi + 0.5f;
//			Vector4 texCoord = Vector4(u, v, 0.0f, 0.0f);
//			m_texCoords.push_back(texCoord);
		}
	}
	uint32_t firstVertex = rgfx_writeVertexData(s_debugPrimVertexBuffer, vertexStride * vertexCount, packer.base);
	free(memory);

	rgfx_buffer indexBuffer = rgfx_getVertexBufferBuffer(s_debugPrimVertexBuffer, kIndexBuffer);
	uint16_t* ptr = (uint16_t*)rgfx_mapBuffer(indexBuffer);
	int32_t indexCount = 0;
	for (unsigned int stackNumber = 0; stackNumber < numStacks; ++stackNumber)
	{
		for (unsigned int sliceNumber = 0; sliceNumber <= numSlices; ++sliceNumber)
		{
			uint32_t v1 = (stackNumber * numSlices) + (sliceNumber % numSlices);
			uint32_t v2 = ((stackNumber + 1) * numSlices) + (sliceNumber % numSlices);
//			m_indices.push_back(v1);
//			m_indices.push_back(v2);
//			rgfx_debugColor col = kDebugColorGreen;
//			rgfx_drawDebugLine(vertices[v1], col, vertices[v2], col);

//			uint32_t v3 = (stackNumber * numSlices) + (sliceNumber % numSlices);
			//			m_indices.push_back(v1);
			//			m_indices.push_back(v2);
//			if (sliceNumber < (numSlices))
//			{
				uint32_t v3 = (stackNumber * numSlices) + ((sliceNumber+1) % numSlices);
				uint32_t v4 = ((stackNumber + 1) * numSlices) + ((sliceNumber + 1) % numSlices);
//				rgfx_drawDebugLine(vertices[v3], col, vertices[v4], col);
//			}
/*
			ptr[0] = v3;
			ptr[1] = v1;
			ptr[2] = v2;
			ptr[3] = v2;
			ptr[4] = v4;
			ptr[5] = v3;
*/
			ptr[0] = v1;
			ptr[1] = v3;
			ptr[2] = v2;
			ptr[3] = v2;
			ptr[4] = v3;
			ptr[5] = v4;
			ptr += 6;
			indexCount += 6;
		}
	}
	rgfx_unmapBuffer(indexBuffer);
//	s_indexCount = indexCount;

	rgfx_renderable rend = rgfx_addRenderable(&(rgfx_renderableInfo) {
		.firstVertex = firstVertex,
		.firstIndex = 0,
		.indexCount = indexCount * 2,
		.materialIndex = -1,
		.pipeline = s_debugRenderPipeline,
		.vertexBuffer = s_debugPrimVertexBuffer,
	});
}

rgfx_renderable rgfx_createDebugCapsule(float radius, float height, const uint32_t horizontal, const uint32_t vertical)
{
	rgfx_bufferInfo* bufferInfo = rgfx_getBufferInfo(rgfx_getVertexBufferBuffer(s_debugPrimVertexBuffer, kVertexBuffer));
	uint32_t vertexStride = bufferInfo->stride;

	//	for (uint32_t i = 0; i < vertexCount; ++i)
	//	{
	//		packPosition(&packer, mesh->VertexPositions[i].x, mesh->VertexPositions[i].y, mesh->VertexPositions[i].z);
	//		packFrenet(&packer, mesh->VertexNormals[i].x, mesh->VertexNormals[i].y, mesh->VertexNormals[i].z);
	//		packUV(&packer, mesh->VertexUV0[i].x, 1.0f - mesh->VertexUV0[i].y);
	//		packJointIndices4x16s(&packer, mesh->BlendIndices[i].x, mesh->BlendIndices[i].y, mesh->BlendIndices[i].z, mesh->BlendIndices[i].w);
	//		packJointWeights4(&packer, mesh->BlendWeights[i].x, mesh->BlendWeights[i].y, mesh->BlendWeights[i].z, mesh->BlendWeights[i].w);
	//	}
	//	uint32_t firstVertex = rgfx_writeVertexData(s_debugPrimVertexBuffer, vertexStride * vertexCount, packer.base);
	//	free(memory);

	uint32_t vertexCount = (horizontal + 1) * (vertical + 1);
	uint32_t indexCount = horizontal * vertical * 6;
	const float latRads = ION_PI * 0.5f;
	const float h = height * 0.5f;

	uint32_t vbSize = vertexCount * vertexStride * 3;
	void* vbMemory = malloc(vbSize);
	uint32_t ibSize = indexCount * sizeof(uint16_t) * 3;
	void* ibMemory = malloc(ibSize);
	uint16_t* indices = (uint16_t*)ibMemory;

	VertexPacker packer;
	packer.base = packer.ptr = vbMemory;

	int32_t vertexIndexOffset = 0;
	int32_t triangleIndexOffset = 0;

	//Cylinder
	{
		for (uint32_t y = 0; y <= vertical; ++y)
		{
			const float yf = (float)y / (float)vertical;
			for (uint32_t x = 0; x <= horizontal; ++x)
			{
				const float xf = (float)x / (float)horizontal;
				const int32_t index = y * (horizontal + 1) + x + vertexIndexOffset;
				rsys_print("Cylinder: Index = %d\n", index);
				vec4 position = { cosf(xf * 2.0f * ION_PI) * radius, sinf(xf * 2.0f * ION_PI) * radius, -h + yf * 2.0f * h, 0.0f };
				packPosition(&packer, position.x, 1.5f+position.y, position.z);
				vec4 normal = vec4_normalize(position);
				packFrenet(&packer, normal.x, normal.y, 0.0f);
			}
		}

		int32_t index = triangleIndexOffset;
		for (uint32_t y = 0; y < vertical; ++y)
		{
			for (uint32_t x = 0; x < horizontal; ++x)
			{
				indices[index + 0] = y * (horizontal + 1) + x;
				indices[index + 1] = y * (horizontal + 1) + x + 1;
				indices[index + 2] = (y + 1) * (horizontal + 1) + x;
				indices[index + 3] = (y + 1) * (horizontal + 1) + x;
				indices[index + 4] = y * (horizontal + 1) + x + 1;
				indices[index + 5] = (y + 1) * (horizontal + 1) + x + 1;
				index += 6;
			}
		}
		vertexIndexOffset += vertexCount;
		triangleIndexOffset += indexCount;
	}

	// Upper Dome
	{
		for (uint32_t y = 0; y <= vertical; y++)
		{
			const float yf = (float)y / (float)vertical;
			const float lat = ION_PI - yf * latRads - 0.5f * ION_PI;
			const float cosLat = cosf(lat);
			for (uint32_t x = 0; x <= horizontal; x++)
			{
				const float xf = (float)x / (float)horizontal;
				const float lon = (0.5f + xf) * ION_PI * 2.0f;
//				const int32_t index = y * (horizontal + 1) + x + vertexIndexOffset;
				vec4 position = { cosf(lon) * cosLat * radius, sinf(lon) * cosLat * radius, h + (radius * sinf(lat)), 0.0f };
				packPosition(&packer, position.x, 1.5f + position.y, position.z);
				vec4 normal = vec4_normalize(position - (vec4) { 0.0f, 0.0f, h, 0.0f });
				packFrenet(&packer, normal.x, normal.y, normal.z);
			}
		}

		int index = triangleIndexOffset;
		for (uint32_t x = 0; x < horizontal; x++)
		{
			for (uint32_t y = 0; y < vertical; y++)
			{
				indices[index + 0] = vertexIndexOffset + y * (horizontal + 1) + x;
				indices[index + 2] = vertexIndexOffset + y * (horizontal + 1) + x + 1;
				indices[index + 1] = vertexIndexOffset + (y + 1) * (horizontal + 1) + x;
				indices[index + 3] = vertexIndexOffset + (y + 1) * (horizontal + 1) + x;
				indices[index + 5] = vertexIndexOffset + y * (horizontal + 1) + x + 1;
				indices[index + 4] = vertexIndexOffset + (y + 1) * (horizontal + 1) + x + 1;
				index += 6;
			}
		}
		vertexIndexOffset += vertexCount;
		triangleIndexOffset += indexCount;
	}

	// Lower Dome
	{
		for (uint32_t y = 0; y <= vertical; y++)
		{
			const float yf = (float)y / (float)vertical;
			const float lat = ION_PI - yf * latRads - 0.5f * ION_PI;
			const float cosLat = cosf(lat);
			for (uint32_t x = 0; x <= horizontal; x++)
			{
				const float xf = (float)x / (float)horizontal;
				const float lon = (0.5f + xf) * ION_PI * 2.0f;
//				const int32_t index = y * (horizontal + 1) + x + vertexIndexOffset;
				vec4 position = { cosf(lon) * cosLat * radius, sinf(lon) * cosLat * radius, -h + -(radius * sinf(lat)), 0.0f };
				packPosition(&packer, position.x, 1.5f + position.y, position.z);
				vec4 normal = vec4_normalize(position + (vec4) { 0.0f, 0.0f, h, 0.0f });
				packFrenet(&packer, normal.x, normal.y, normal.z);
			}
		}
		int index = triangleIndexOffset;
		for (uint32_t x = 0; x < horizontal; x++)
		{
			for (uint32_t y = 0; y < vertical; y++)
			{
				indices[index + 0] = vertexIndexOffset + y * (horizontal + 1) + x;
				indices[index + 1] = vertexIndexOffset + y * (horizontal + 1) + x + 1;
				indices[index + 2] = vertexIndexOffset + (y + 1) * (horizontal + 1) + x;
				indices[index + 3] = vertexIndexOffset + (y + 1) * (horizontal + 1) + x;
				indices[index + 4] = vertexIndexOffset + y * (horizontal + 1) + x + 1;
				indices[index + 5] = vertexIndexOffset + (y + 1) * (horizontal + 1) + x + 1;
				index += 6;
			}
		}
		vertexIndexOffset += vertexCount;
		triangleIndexOffset += indexCount;
	}

	uint32_t firstVertex = rgfx_writeVertexData(s_debugPrimVertexBuffer, vbSize, packer.base);
	uint32_t firstIndex = rgfx_writeIndexData(s_debugPrimVertexBuffer, ibSize, indices, sizeof(uint16_t));

	free(ibMemory);
	free(vbMemory);

//	s_indexCount = indexCount*3;

	rgfx_renderable rend = rgfx_addRenderable(&(rgfx_renderableInfo) {
		.firstVertex = firstVertex,
		.firstIndex = firstIndex,
		.indexCount = ibSize, //indexCount * 3 * 2,
		.materialIndex = -1,
		.pipeline = s_debugRenderPipeline,
		.vertexBuffer = s_debugPrimVertexBuffer,
	});
	return rend;
}

/*
	MPrimSphere::MPrimSphere( float radius, const uint numSlices, const uint numStacks )
	{

		for(unsigned int stackNumber = 0; stackNumber <= numStacks; ++stackNumber)
		{
			for(unsigned int sliceNumber = 0; sliceNumber < numSlices; ++sliceNumber)
			{
				float theta = stackNumber * ion::kPi / numStacks;
				float phi = sliceNumber * 2 * ion::kPi / numSlices;
				float sinTheta = std::sin(theta);
				float sinPhi = std::sin(phi);
				float cosTheta = std::cos(theta);
				float cosPhi = std::cos(phi);
				Vector4 vertex = Vector4( radius * cosPhi * sinTheta, radius * sinPhi * sinTheta, radius * cosTheta, 1.0f);
				//			Vec4 vertex = Vec4( radius * cosPhi * sinTheta, radius * cosTheta, radius * sinPhi * sinTheta, 1.0f);
				m_vertices.push_back( vertex );
				Vector4 colour = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
				m_colours.push_back( colour );
				Vector4 normal = vertex;		//Vec4( vertex.m_x, vertex.m_y, vertex.m_z, 0.0f );
				normal = Normalize( normal );
				m_normals.push_back( normal );
				float u = asin( normal.X() ) / ion::kPi + 0.5f;
				float v = asin( normal.Y() ) / ion::kPi + 0.5f;
				Vector4 texCoord = Vector4( u, v, 0.0f, 0.0f );
				m_texCoords.push_back( texCoord );
			}
		}
		for(unsigned int stackNumber = 0; stackNumber < numStacks; ++stackNumber)
		{
			for(unsigned int sliceNumber = 0; sliceNumber <= numSlices; ++sliceNumber)
			{
				uint v1 = (stackNumber * numSlices) + (sliceNumber % numSlices);
				uint v2 = ((stackNumber + 1) * numSlices) + (sliceNumber % numSlices);
				m_indices.push_back(v1);
				m_indices.push_back(v2);
			}
		}

		//	int numTris = m_indices.size();
		glGenBuffersARB( 1, &m_vertexVBO );
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, m_vertexVBO );
		glBufferDataARB( GL_ARRAY_BUFFER_ARB, m_vertices.size()*4*sizeof(float), &m_vertices[0], GL_STATIC_DRAW_ARB );

		glGenBuffersARB( 1, &m_colourVBO );
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, m_colourVBO );
		glBufferDataARB( GL_ARRAY_BUFFER_ARB, m_colours.size()*4*sizeof(float), &m_colours[0], GL_STATIC_DRAW_ARB );

		glGenBuffersARB( 1, &m_normalVBO );
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, m_normalVBO );
		glBufferDataARB( GL_ARRAY_BUFFER_ARB, m_normals.size()*4*sizeof(float), &m_normals[0], GL_STATIC_DRAW_ARB );

		glGenBuffersARB( 1, &m_texCoordVBO );
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, m_texCoordVBO );
		glBufferDataARB( GL_ARRAY_BUFFER_ARB, m_texCoords.size()*4*sizeof(float), &m_texCoords[0], GL_STATIC_DRAW_ARB );

		glGenBuffersARB( 1, &m_indexVBO );
		glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, m_indexVBO );
		glBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, m_indices.size()*sizeof(uint), &m_indices[0], GL_STATIC_DRAW_ARB );
	}

MPrimCylinder::MPrimCylinder( float radius, float height, const uint numSlices )
{
	float halfHeight = height * 0.5f;
	float degPerSlice = 360.0f / (float)numSlices;

	m_glDisplayList = glGenLists(1);
	glNewList(m_glDisplayList, GL_COMPILE);
// Right End Cap.
	glBegin( GL_TRIANGLE_STRIP );
	for( uint sliceNumber = 0; sliceNumber <= numSlices; ++sliceNumber )
	{
		float theta = 2 * sliceNumber * ion::kPi / numSlices;
		float sinTheta = sinf( theta );
		float cosTheta = cosf( theta );
		float x = halfHeight;	//radius * cosTheta;
		float y = radius * sinTheta;
		float z = -radius * cosTheta;	//-1.0f;
		int col = 192 + int((float)sliceNumber * degPerSlice * colDelta);
		glColor4ub( 0, 0, 255, 255 );
		glVertex3f( x, 0.0f, 0.0f );
		if(sliceNumber == 0)
			glColor4ub( 255, 0, 0, 255 );
		else
			glColor4ub( 0, 0, 128+col, 255 );
		glVertex3f( x, y, z );
	}
	glEnd();
//Left End Cap.
	glBegin( GL_TRIANGLE_STRIP );
	for( uint sliceNumber = 0; sliceNumber <= numSlices; ++sliceNumber )
	{
		float theta = 2 * sliceNumber * ion::kPi / numSlices;
		float sinTheta = sinf( theta );
		float cosTheta = cosf( theta );
		float x = -halfHeight;	//-radius * cosTheta;
		float y = radius * sinTheta;
		float z = -radius * cosTheta;	//1.0f;
		int col = 192 + int((float)sliceNumber * degPerSlice * colDelta);
		if(sliceNumber == 0)
			glColor4ub( 255, 0, 0, 255 );
		else
			glColor4ub( 0, 0, 128+col, 255 );
		glVertex3f( x, y, z );
		glColor4ub( 0, 0, 255, 255 );
		glVertex3f( x, 0.0f, 0.0f );
	}
	glEnd();

	glBegin( GL_TRIANGLE_STRIP );
	for( uint sliceNumber = 0; sliceNumber <= numSlices; ++sliceNumber )
	{
		float theta = 2 * sliceNumber * ion::kPi / numSlices;
		float sinTheta = sinf( theta );
		float cosTheta = cosf( theta );
		float x = halfHeight;	//radius * cosTheta;
		float y = radius * sinTheta;
		float z = -radius * cosTheta;	//1.0f;
		int col = 192 + int((float)sliceNumber * degPerSlice * colDelta);
		if(sliceNumber == 0)
			glColor4ub( 255, 0, 0, 255 );
		else
			glColor4ub( 0, 0, 128+col, 255 );
		glVertex3f( x, y, z );
		x = -halfHeight;
		if(sliceNumber == 0)
			glColor4ub( 255, 0, 0, 255 );
		else
			glColor4ub( 0, 0, 128+col, 255 );
		glVertex3f( x, y, z );
	}

	glEnd();
	glEndList();
}
*/