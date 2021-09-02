#include "stdafx.h"
#include "renderer.h"
#ifdef RE_PLATFORM_WIN64
#include "GL4.6/renderer_gl.h"
#else
#include "GLES3.2/renderer_gles.h"
#endif
#include "resource.h"
#include "assert.h"
#include "rgfx/mesh.h"
#include "gui.h"
#include "system.h"
#include "math/vec4.h"
#include "stb/stretchy_buffer.h"
#include "hash.h"

#ifdef VRSYSTEM_OCULUS
#include "OVR_CAPI_GL.h"
#endif

#ifndef RE_PLATFORM_WIN64
#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"

//extern VrApi vrapi;
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "vrsystem.h"

#define USE_SEPARATE_SHADERS_OBJECTS
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

extern inline GLuint rgfx_getNativeBufferFlags(uint32_t flags);
extern inline rgfx_buffer rgfx_createBuffer(const rgfx_bufferDesc* desc);
extern inline void rgfx_destroyBuffer(rgfx_buffer buffer);
extern inline void* rgfx_mapBuffer(rgfx_buffer buffer);
extern inline void* rgfx_mapBufferRange(rgfx_buffer buffer, int64_t offset, int64_t size);
extern inline void rgfx_unmapBuffer(rgfx_buffer buffer);
extern inline void rgfx_bindBuffer(int32_t index, rgfx_buffer buffer);
extern inline void rgfx_bindBufferRange(int32_t index, rgfx_buffer buffer, int64_t offset, int64_t size);

extern inline rgfx_vertexBuffer rgfx_createVertexBuffer(const rgfx_vertexBufferDesc* desc);
extern inline void rgfx_destroyVertexBuffer(rgfx_vertexBuffer vb);
extern inline uint32_t rgfx_writeVertexData(rgfx_vertexBuffer vb, size_t sizeInBytes, uint8_t* vertexData);
extern inline uint32_t rgfx_writeIndexData(rgfx_vertexBuffer vb, size_t sizeInBytes, uint8_t* indexData, size_t sourceIndexSize);

extern inline GLenum rgfx_getNativePrimType(rgfx_primType primType);
extern inline GLenum rgfx_getNativeTextureFormat(rgfx_format format);
extern inline rgfx_pass rgfx_createPass(const rgfx_passDesc* desc);


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-pointer-arithmetic"

//static __attribute__((aligned(64))) rgfx_rendererData s_rendererData = {};
__attribute__((aligned(64))) rgfx_rendererData s_rendererData = {};

#ifdef _DEBUG
void openglCallbackFunction(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	(void)source; (void)type; (void)id;
	(void)severity; (void)length; (void)userParam;

	if (severity == GL_DEBUG_SEVERITY_MEDIUM || severity == GL_DEBUG_SEVERITY_HIGH)
		printf("%s\n", message);
	if (severity == GL_DEBUG_SEVERITY_HIGH)
	{
		if (type != GL_DEBUG_TYPE_PERFORMANCE)
		{
			printf("High Severity...\n");
		}
		//			abort();
	}
}
#endif

void rgfx_initialize(const rgfx_initParams* params)
{
	memset(&s_rendererData, 0, sizeof(rgfx_rendererData));

	rgfx_initializeResourceManager();
	rgfx_initializeScene();

	rgfx_initializeExtensions(params->extensions);

	s_rendererData.numEyeBuffers = 1;
	if (rvrs_isInitialized())
	{
		memset(s_rendererData.eyeFbInfo, 0, sizeof(rgfx_eyeFbInfo) * kEyeCount);
		s_rendererData.numEyeBuffers = (int32_t)kEyeCount; // VRAPI_FRAME_LAYER_EYE_MAX;

		int32_t numBuffers = params->extensions.multi_view ? 1 : kEyeCount;
		// Create the frame buffers.
		for (int32_t eye = 0; eye < numBuffers; eye++)
		{
			rgfx_createEyeFrameBuffers(eye, params->eyeWidth, params->eyeHeight, kFormatSRGBA8, 4, params->extensions.multi_view);
		}
		s_rendererData.numEyeBuffers = numBuffers;
	}

	GLint uniformBufferAlignSize = 0;
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uniformBufferAlignSize);
	s_rendererData.uniformBufferAlignSize = uniformBufferAlignSize;

	GLint numVertexAttribs;
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numVertexAttribs);
#ifdef _DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(openglCallbackFunction, NULL);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, true);
#endif

	glDepthFunc(GL_LESS);
	glCullFace(GL_BACK);
	glDepthMask(GL_TRUE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_SCISSOR_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_FRAMEBUFFER_SRGB);

	const uint32_t bufferSize = 2 * 1024 * 1024;

	// Unskinned Vertex Buffer
	rgfx_vertexBuffer unskinnedVb = { 0 };
	{
		rgfx_vertexElement vertexStreamElements[] =
		{
			{ kFloat, 3, false },
			{ kInt2_10_10_10_Rev, 4, true },
			{ kInt2_10_10_10_Rev, 4, true },
			{ kInt2_10_10_10_Rev, 4, true },
			{ kFloat, 2, false },
		};
		unskinnedVb = rgfx_createVertexBuffer(&(rgfx_vertexBufferDesc) {
			.format = rgfx_registerVertexFormat(STATIC_ARRAY_SIZE(vertexStreamElements), vertexStreamElements),
			.capacity = bufferSize,
			.indexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
				.type = kIndexBuffer,
				.capacity = bufferSize,
				.stride = sizeof(uint16_t),
				.flags = kMapWriteBit | kDynamicStorageBit
			})
		});
	}

	// Skinned Vertex Buffer
	rgfx_vertexBuffer skinnedVb = { 0 };
	{
		rgfx_vertexElement vertexStreamElements[] =
		{
			{ kFloat, 3, false },
			{ kInt2_10_10_10_Rev, 4, true },
			{ kInt2_10_10_10_Rev, 4, true },
			{ kInt2_10_10_10_Rev, 4, true },
			{ kFloat, 2, false },
			{ kSignedInt, 4, false },
			{ kFloat, 4, false },
		};
		skinnedVb = rgfx_createVertexBuffer(&(rgfx_vertexBufferDesc) {
			.format = rgfx_registerVertexFormat(STATIC_ARRAY_SIZE(vertexStreamElements), vertexStreamElements),
			.capacity = bufferSize,
			.indexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
				.type = kIndexBuffer,
				.capacity = bufferSize,
				.stride = sizeof(uint16_t),
				.flags = kMapWriteBit | kDynamicStorageBit
			})
		});
	}

	s_rendererData.cameraTransforms = rgfx_createBuffer(&(rgfx_bufferDesc) {
		.capacity = sizeof(rgfx_cameraParams),
		.stride = 0,
		.flags = kMapWriteBit
	});

	s_rendererData.lights = rgfx_createBuffer(&(rgfx_bufferDesc) {
		.capacity = sizeof(rgfx_light) * 8,
		.stride = 0,
		.flags = kMapWriteBit
	});
	rgfx_setGameCamera(NULL);
}

void rgfx_initPostEffects(void)
{
	s_rendererData.fsQuadPipeline = rgfx_createPipeline(&(rgfx_pipelineDesc) {
		.vertexShader = rgfx_loadShader("shaders/fsquad.vert", kVertexShader, 0),
		.fragmentShader = rgfx_loadShader("shaders/fsquad.frag", kFragmentShader, 0),
	});
}

void rgfx_initializeResourceManager()
{
	void* bufferPtr = NULL;
	int32_t elementSize = 0;
	int32_t capacity = 0;
	for (int32_t i = 0; i < kResourceNum; ++i)
	{
		rgfx_resourceType type = (rgfx_resourceType)i;
		switch (type)
		{
		case kResourceUnknown:
			continue;
		case kResourceBuffer:
			bufferPtr = s_rendererData.resourceInfo[i].resourceBuffer = s_rendererData.buffers;
			elementSize = s_rendererData.resourceInfo[i].elementSize = sizeof(rgfx_bufferInfo);
			capacity = MAX_BUFFERS;
			break;
		case kResourceTexture:
			bufferPtr = s_rendererData.resourceInfo[i].resourceBuffer = s_rendererData.textures;
			elementSize = s_rendererData.resourceInfo[i].elementSize = sizeof(rgfx_textureInfo);
			capacity = MAX_TEXTURES;
			break;
		case kResourceVertexBuffer:
			bufferPtr = s_rendererData.resourceInfo[i].resourceBuffer = s_rendererData.vertexBufferInfo;
			elementSize = s_rendererData.resourceInfo[i].elementSize = sizeof(rgfx_vertexBufferInfo);
			capacity = MAX_VERTEX_BUFFERS;
		}
		for (int32_t j = 0; j < capacity; ++j)
		{
			rgfx_resourceHeader* header = (rgfx_resourceHeader*)((uint8_t*)bufferPtr + j * elementSize);
			header->next = j < (capacity - 1) ? j + 1 : -1;
		}
	}
}

int32_t rgfx_allocResource(rgfx_resourceType type)
{
	rgfx_resource* resource = &s_rendererData.resourceInfo[type];
	if (resource->freeList < 0)
		return -1;

	resource->count++;
	int32_t resourceIndex = resource->freeList;
	rgfx_resourceHeader* header = (rgfx_resourceHeader*)((uint8_t*)resource->resourceBuffer + resourceIndex * resource->elementSize);
	resource->freeList = header->next;
	header->next = -1;

	return resourceIndex;
}

void rgfx_freeResource(rgfx_resourceType type, int32_t idx)
{
	rgfx_resource* resource = &s_rendererData.resourceInfo[type];
	rgfx_resourceHeader* header = (rgfx_resourceHeader*)((uint8_t*)resource->resourceBuffer + idx * resource->elementSize);
	header->next = resource->freeList;
	resource->count--;
	resource->freeList = idx;
}

rgfx_bufferInfo* rgfx_getVertexBufferInfo(rgfx_vertexBuffer vb)
{
	int32_t vbIdx = vb.id - 1;
	assert(vbIdx >= 0);
	rgfx_buffer buffer = s_rendererData.vertexBufferInfo[vbIdx].vertexBuffer;
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	return &s_rendererData.buffers[bufferIdx];
}

rgfx_buffer rgfx_getVertexBufferBuffer(rgfx_vertexBuffer vb, rgfx_bufferType type)
{
	int32_t vbIdx = vb.id - 1;
	assert(vbIdx >= 0);
	switch (type)
	{
	case kVertexBuffer:
		return s_rendererData.vertexBufferInfo[vbIdx].vertexBuffer;
		break;
	case kIndexBuffer:
		return s_rendererData.vertexBufferInfo[vbIdx].indexBuffer;
		break;
	default:
		rsys_print("Warning: Invalid buffer type requested.\n");
	};
	return (rgfx_buffer) { 0 };
}

rgfx_bufferInfo* rgfx_getIndexBufferInfo(rgfx_vertexBuffer vb)
{
	int32_t vbIdx = vb.id - 1;
	assert(vbIdx >= 0);
	rgfx_buffer buffer = s_rendererData.vertexBufferInfo[vbIdx].indexBuffer;
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	return &s_rendererData.buffers[bufferIdx];
}

void rgfx_writeCompressedMipImageData(rgfx_texture tex, int32_t mipLevel, rgfx_format format, int32_t width, int32_t height, int32_t imageSize, const void* data)
{
	int32_t texIdx = tex.id - 1;
	assert(texIdx >= 0);
	assert(texIdx < MAX_TEXTURES);

	GLenum internalFormat = rgfx_getNativeTextureFormat(format);

	rgfx_textureInfo* texInfo = &s_rendererData.textures[texIdx];
	glBindTexture(texInfo->target, texInfo->name);
	glCompressedTexImage2D(texInfo->target, mipLevel, internalFormat, width, height, 0, imageSize, data);
	glBindTexture(texInfo->target, 0);
}

void rgfx_writeCompressedTexSubImage3D(rgfx_texture tex, int32_t mipLevel, int32_t xOffset, int32_t yOffset, int32_t zOffset, int32_t width, int32_t height, int32_t depth, rgfx_format format, int32_t imageSize, const void* data)
{
	int32_t texIdx = tex.id - 1;
	assert(texIdx >= 0);
	assert(texIdx < MAX_TEXTURES);

	GLenum internalFormat = rgfx_getNativeTextureFormat(format);
	if (s_rendererData.textures[texIdx].internalFormat == 0)
	{
		assert(mipLevel == 0);
		GLuint name = s_rendererData.textures[texIdx].name;
		GLenum target = s_rendererData.textures[texIdx].target;
		glBindTexture(target, name);
		int32_t allocDepth = s_rendererData.textures[texIdx].depth;
		int32_t mipCount = log2(width) + 1;
		switch (target)
		{
		case GL_TEXTURE_2D:
			glTexStorage2D(target, mipCount, internalFormat, width, height);
			break;
		case GL_TEXTURE_2D_ARRAY:
			assert(allocDepth > 0);
			glTexStorage3D(target, mipCount, internalFormat, width, height, allocDepth);
			break;
		default:
			rsys_print("Currently unsupported texture type.\n");
			assert(0);
		}
		s_rendererData.textures[texIdx].width = width;
		s_rendererData.textures[texIdx].height = height;
		s_rendererData.textures[texIdx].internalFormat = internalFormat;
	}

	assert(internalFormat == s_rendererData.textures[texIdx].internalFormat);

	rgfx_textureInfo* texInfo = &s_rendererData.textures[texIdx];
	glBindTexture(texInfo->target, texInfo->name);
	glCompressedTexSubImage3D(texInfo->target, mipLevel, xOffset, yOffset, zOffset, width, height, depth, internalFormat, imageSize, data);
	glBindTexture(texInfo->target, 0);
}

rgfx_bufferInfo* rgfx_getBufferInfo(rgfx_buffer buffer)
{
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	return &s_rendererData.buffers[bufferIdx];
}

GLint g_viewIdLoc = -1;

void rgfx_bindEyeFrameBuffer(int32_t eye)
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_rendererData.eyeFbInfo[eye].colourBuffers[0]);
}

rgfx_eyeFbInfo* rgfx_getEyeFrameBuffer(int32_t eye)
{
	assert(eye >= 0);
	assert(eye < 2);

	return &s_rendererData.eyeFbInfo[s_rendererData.numEyeBuffers == 1 ? 0 : eye];
}

void rgfx_bindTexture(int32_t slot, rgfx_texture tex)
{
	int32_t texIdx = tex.id - 1;
	assert(texIdx >= 0);
	assert(texIdx < MAX_TEXTURES);
	assert(slot >= 0);
	assert(slot < 16);
	glActiveTexture(GL_TEXTURE0+slot);
	glBindTexture(s_rendererData.textures[texIdx].target, s_rendererData.textures[texIdx].name);
}

void rgfx_bindVertexBuffer(rgfx_vertexBuffer vb)
{
	int32_t vbIdx = vb.id - 1;
	assert(vbIdx >= 0);
	glBindVertexArray(s_rendererData.vertexBufferInfo[vbIdx].vao);
}

void rgfx_unbindVertexBuffer()
{
	glBindVertexArray(0);
}

uint32_t rgfx_bindIndexBuffer(rgfx_buffer buffer)
{
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_rendererData.buffers[bufferIdx].name);
	return s_rendererData.buffers[bufferIdx].writeIndex / s_rendererData.buffers[bufferIdx].stride;
}

rgfx_vertexFormat rgfx_registerVertexFormat(int32_t numVertexElements, rgfx_vertexElement* vertexElements)
{
	assert(s_rendererData.numVertexFormats < MAX_VERTEX_FORMATS);

	//	(void)numVertexElements;
	//	(void)vertexElements;
	//	uint32_t hash;
	//	MurmurHash3_x86_32(vertexElements, sizeof(VertexElement) * numVertexElements, 0xdeadbeef, &hash);
	//	printf("Hash: 0x%08x\n", hash);

	uint32_t vertexFormatHash = hashData(vertexElements, sizeof(rgfx_vertexElement) * numVertexElements); // , 0xdeadbeef, & hash);
	int32_t vertexFormatIndex = rsys_hm_find(&s_rendererData.vertexFormatLookup, vertexFormatHash);
	if (vertexFormatIndex < 0)
	{
		uint8_t elementOffset = 0;
		vertexFormatIndex = s_rendererData.numVertexFormats++;
		for (int32_t i = 0; i < numVertexElements; ++i)
		{
			uint16_t type = vertexElements[i].m_type;
			size_t elementSize = 0;
			char* elementName = NULL;
			switch (type)
			{
			case kFloat:
				elementSize = sizeof(GLfloat) * vertexElements[i].m_size;
				elementName = "GL_FLOAT";
				break;
			case kInt2_10_10_10_Rev:
				elementSize = sizeof(GLint);
				elementName = "GL_INT_2_10_10_10_REV";
				break;
			case kSignedInt:
				elementSize = sizeof(GLint) * vertexElements[i].m_size;
				elementName = "GL_INT";
				break;
			case kUnsignedByte:
				elementSize = sizeof(GLubyte) * vertexElements[i].m_size;
				elementName = "GL_UNSIGNED_BYTE";
				break;
			default:
				assert(0);
			}
			printf("Element: Index = %d, Size = %d, Type = %s, Offset = %d\n", i, vertexElements[i].m_size, elementName, elementOffset);
			elementOffset += elementSize;
			s_rendererData.vertexFormats[vertexFormatIndex].elements[i].m_type = vertexElements[i].m_type;
			s_rendererData.vertexFormats[vertexFormatIndex].elements[i].m_size = vertexElements[i].m_size;
			s_rendererData.vertexFormats[vertexFormatIndex].elements[i].m_normalized = vertexElements[i].m_normalized;
		}
		s_rendererData.vertexFormats[vertexFormatIndex].numElements = numVertexElements;
		s_rendererData.vertexFormats[vertexFormatIndex].stride = elementOffset;
		s_rendererData.vertexFormats[vertexFormatIndex].hash = vertexFormatHash;
		rsys_hm_insert(&s_rendererData.vertexFormatLookup, vertexFormatHash, vertexFormatIndex);

		printf("Vertex stride = %d\n", elementOffset);
	}
	return (rgfx_vertexFormat) { .id = vertexFormatIndex + 1 };
}

rgfx_vertexFormatInfo* rgfx_getVertexFormatInfo(rgfx_vertexFormat format)
{
	int32_t formatIdx = format.id - 1;
	assert(formatIdx >= 0);
	return &s_rendererData.vertexFormats[formatIdx];
}

void rgfx_setGameCamera(const rgfx_gameCameraDesc* gameCameraDesc)
{
	bool gameCameraDescIsNULL = gameCameraDesc == NULL;
	bool xAxisZero = gameCameraDescIsNULL ? true : vec4_isZero(gameCameraDesc->worldMatrix.xAxis);
	bool yAxisZero = gameCameraDescIsNULL ? true : vec4_isZero(gameCameraDesc->worldMatrix.yAxis);
	bool zAxisZero = gameCameraDescIsNULL ? true : vec4_isZero(gameCameraDesc->worldMatrix.zAxis);
	bool wAxisZero = gameCameraDescIsNULL ? true : vec4_isZero(gameCameraDesc->worldMatrix.wAxis);

	if (xAxisZero && yAxisZero && zAxisZero && wAxisZero)
	{
		s_rendererData.gameCamera.worldMatrix = mat4x4_identity();
	}
	else
	{
		s_rendererData.gameCamera.worldMatrix = gameCameraDesc->worldMatrix;
	}

	xAxisZero = gameCameraDescIsNULL ? true : vec4_isZero(gameCameraDesc->viewMatrix.xAxis);
	yAxisZero = gameCameraDescIsNULL ? true : vec4_isZero(gameCameraDesc->viewMatrix.yAxis);
	zAxisZero = gameCameraDescIsNULL ? true : vec4_isZero(gameCameraDesc->viewMatrix.zAxis);
	wAxisZero = gameCameraDescIsNULL ? true : vec4_isZero(gameCameraDesc->viewMatrix.wAxis);

	if (xAxisZero && yAxisZero && zAxisZero && wAxisZero)
	{
		s_rendererData.gameCamera.viewMatrix = mat4x4_orthoInverse(s_rendererData.gameCamera.worldMatrix);
	}
	else
	{
		s_rendererData.gameCamera.viewMatrix = gameCameraDesc->viewMatrix;
	}
}

void rgfx_updateTransforms(const rgfx_cameraDesc* cameraDesc)
{
	int32_t numCamera = cameraDesc->count > 0 ? cameraDesc->count : 1;

	int32_t idx = s_rendererData.cameraTransforms.id - 1;
	assert(idx >= 0);
	glBindBuffer(GL_UNIFORM_BUFFER, s_rendererData.buffers[idx].name);
	rgfx_cameraParams* block = (rgfx_cameraParams*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(rgfx_cameraParams), s_rendererData.buffers[idx].flags | GL_MAP_INVALIDATE_BUFFER_BIT);

	vec4 fogPlane = (vec4){ 0.0f, 1.0, 0.0f, -0.78f };
	for (int32_t cam = 0; cam < numCamera; ++cam)
	{
		mat4x4 viewProjMatrx = mat4x4_mul(cameraDesc->camera[cam].projectionMatrix, cameraDesc->camera[cam].viewMatrix);
		block->camera[cam].viewProjectionMatrix = s_rendererData.cameras[cam].viewProjectionMatrix = viewProjMatrx;
		block->camera[cam].position = s_rendererData.cameras[cam].position = cameraDesc->camera[cam].position;
		s_rendererData.cameras[cam].viewMatrix = cameraDesc->camera[cam].viewMatrix;
		s_rendererData.cameras[cam].projectionMatrix = cameraDesc->camera[cam].projectionMatrix;
	}
	block->fogPlane = fogPlane;
	glUnmapBuffer(GL_UNIFORM_BUFFER);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

rgfx_texture rgfx_createTextureArray(const char** textureNames, int32_t numTextures)
{
	rgfx_texture texArray = rgfx_createTexture(&(rgfx_textureDesc) {
		.depth = numTextures,
		.type = kTexture2DArray,
		.minFilter = kFilterLinearMipmapLinear,
		.magFilter = kFilterLinear,
		.wrapS = kWrapModeRepeat,
		.wrapT = kWrapModeRepeat,			
		.anisoFactor = 1.0f,
	});

	for (int32_t tex = 0; tex < numTextures; ++tex)
	{
		texture_load(textureNames[tex], &(rgfx_textureLoadDesc) {
			.texture = texArray,
			.slice = tex,
		});
	}
	return texArray;
}

rgfx_buffer rgfx_getTransforms()
{
	return s_rendererData.cameraTransforms;
}

rgfx_buffer rgfx_getLightsBuffer()
{
	return s_rendererData.lights;
}

mat4x4 rgfx_getCameraViewMatrix(int32_t cam)
{
	assert(cam >= 0);
	assert(cam < MAX_CAMERAS);

	return s_rendererData.cameras[cam].viewMatrix;
}

mat4x4 rgfx_getCameraProjectionMatrix(int32_t cam)
{
	assert(cam >= 0);
	assert(cam < MAX_CAMERAS);

	return s_rendererData.cameras[cam].projectionMatrix;
}

mat4x4 rgfx_getCameraViewProjMatrix(int32_t cam)
{
	assert(cam >= 0);
	assert(cam < MAX_CAMERAS);

	return mat4x4_mul(s_rendererData.cameras[cam].projectionMatrix, s_rendererData.cameras[cam].viewMatrix);
}

/*
void rgfx_addLight(vec4 position, vec4 color)
{
	int32_t idx = s_rendererData.lights.id - 1;
	assert(idx >= 0);
	glBindBuffer(GL_UNIFORM_BUFFER, s_rendererData.buffers[idx].name);
	rgfx_light* mappedPtr = (rgfx_light*)glMapBufferRange(GL_UNIFORM_BUFFER, sizeof(rgfx_light) * s_rendererData.lightCount++, sizeof(rgfx_light), s_rendererData.buffers[idx].flags);

	if (mappedPtr)
	{
		mappedPtr->position = position;
		mappedPtr->color = color;
		glUnmapBuffer(GL_UNIFORM_BUFFER);
	}
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
*/

void rgfx_removeAllMeshInstances()
{
	s_rendererData.numMeshInstances = 0;
}

void rgfx_beginDefaultPass(const rgfx_passAction* passAction, int width, int height)
{
	float r = passAction->colors[0].val[0];
	float g = passAction->colors[0].val[1];
	float b = passAction->colors[0].val[2];
	float a = passAction->colors[0].val[3];

	glClearColor(r, g, b, a);
	glClearDepthf(passAction->depth.val);
	GLbitfield clearMask = 0;
	clearMask |= passAction->colors[0].action == kActionClear ? GL_COLOR_BUFFER_BIT : 0;
	clearMask |= passAction->depth.action == kActionClear ? GL_DEPTH_BUFFER_BIT : 0;
	if (clearMask)
	{
		glClear(clearMask);
	}
	glViewport(0, 0, width, height);
	s_rendererData.currentPassInfo.frameBufferId = 0;
	s_rendererData.currentPassInfo.width = width;
	s_rendererData.currentPassInfo.height = height;
	s_rendererData.currentPassInfo.flags = 0;
}

void rgfx_beginPass(rgfx_pass pass, const rgfx_passAction* passAction)
{
	int32_t passIdx = pass.id - 1;
	assert(passIdx >= 0);
	rgfx_passInfo* passInfo = &s_rendererData.passes[passIdx];
	GLuint frameBufferId = passInfo->frameBufferId;
	int32_t width = passInfo->width;
	int32_t height = passInfo->height;
	rgfx_passFlags flags = passInfo->flags;

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBufferId);
#ifdef _DEBUG
	GLuint status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE); //, "Framebuffer Incomplete\n");
#endif

	float r = passAction->colors[0].val[0];
	float g = passAction->colors[0].val[1];
	float b = passAction->colors[0].val[2];
	float a = passAction->colors[0].val[3];
	glClearColor(r, g, b, a);
	glClearDepthf(passAction->depth.val);

	GLbitfield clearMask = 0;
	clearMask |= passAction->colors[0].action == kActionClear ? GL_COLOR_BUFFER_BIT : 0;
	clearMask |= passAction->depth.action == kActionClear ? GL_DEPTH_BUFFER_BIT : 0;
	if (clearMask)
	{
		glClear(clearMask);
	}
	/*
		if (flags & kPFNeedsResolve)
			glEnable(GL_MULTISAMPLE);
		else
			glDisable(GL_MULTISAMPLE);
	*/
	glViewport(0, 0, width, height);
	glScissor(0, 0, width, height);
	s_rendererData.currentPassInfo.frameBufferId = frameBufferId;
	s_rendererData.currentPassInfo.width = width;
	s_rendererData.currentPassInfo.height = height;
	s_rendererData.currentPassInfo.flags = flags;
}

void rgfx_endPass(bool bScreenshotRequested)
{
	if (s_rendererData.currentPassInfo.frameBufferId > 0 && s_rendererData.currentPassInfo.flags & kPFNeedsResolve)
	{
		GLuint frameBufferId = s_rendererData.currentPassInfo.frameBufferId;
		int32_t width = s_rendererData.currentPassInfo.width;
		int32_t height = s_rendererData.currentPassInfo.height;
		glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBufferId);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
#ifdef RE_PLATFORM_WIN64
		if (bScreenshotRequested)
		{
			uint8_t* pixels = (uint8_t*)malloc(width * height * 4);
			glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			uint8_t* ptr = pixels;
			for (int32_t y = 0; y < height; ++y)
			{
				for (int32_t x = 0; x < width; ++x)
				{
					ptr[3] = 0xff;
					ptr += 4;
				}
			}
			static int32_t screenshotCount = 0;
			char filename[MAX_PATH];
			sprintf(filename, "screenshot_%03d.png", screenshotCount++);
			stbi_flip_vertically_on_write(1);
			stbi_write_png(filename, width, height, 4, pixels, width * 4);

			free(pixels);
		}
#endif
	}
}

void rgfx_bindPipeline(rgfx_pipeline pipeline)
{
	int32_t pipeIdx = pipeline.id - 1;
	assert(pipeIdx >= 0);
#ifdef USE_SEPARATE_SHADERS_OBJECTS
	glBindProgramPipeline(s_rendererData.pipelines[pipeIdx].name);
#else
	glUseProgram(s_rendererData.pipelines[pipeIdx].name);
#endif
}

uint32_t rgfx_getPipelineProgram(rgfx_pipeline pipeline, rgfx_shaderType type)
{
	int32_t pipeIdx = pipeline.id - 1;
	assert(pipeIdx >= 0);
	switch (type)
	{
	case kVertexShader:
		return s_rendererData.pipelines[pipeIdx].vertexProgramName;
	case kFragmentShader:
		return s_rendererData.pipelines[pipeIdx].fragmentProgramName;
	default:
		assert(0);
	}
	return 0;
}


mat4x4* rgfx_mapModelMatricesBuffer(int32_t start, int32_t end)
{
	//	int32_t idx = s_rendererData.instanceMatrices.id;
	//	glBindBuffer(GL_UNIFORM_BUFFER, s_rendererData.buffers[idx].name);
	return NULL; //(mat4x4 *)glMapBufferRange(GL_UNIFORM_BUFFER, start * sizeof(mat4x4), end * sizeof(mat4x4), s_rendererData.buffers[idx].flags);
}

void rgfx_unmapModelMatricesBuffer()
{
	//	glUnmapBuffer(GL_UNIFORM_BUFFER);
	//	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

#if 1
typedef int cmp_t(const void*, const void*);
void insertion_sort(const void *arr, size_t n, size_t elementSize, cmp_t ^cmp_b)
{
	const void* key;
	int i, j;
	for (i = 1; i < n; i++)
	{
		key = (const void*)((uint8_t*)arr + i * elementSize);
		void* keyCpy = alloca(elementSize);
		memcpy(keyCpy, key, sizeof(elementSize));
		j = i - 1;
		while (j >= 0 && cmp_b((const void*)((uint8_t*)arr + j * elementSize), keyCpy) > 0)
		{
			const void* elemJ = (const void*)((uint8_t*)arr + j * elementSize);
			void* elemJ1 = (void*)((uint8_t*)arr + (j+1) * elementSize);
//			arr[j + 1] = arr[j];
			memcpy(elemJ1, elemJ, elementSize);
			j = j - 1;
		}
//		arr[j + 1] = key;
		void* elemJ1 = (void*)((uint8_t*)arr + (j+1) * elementSize);
		memcpy(elemJ1, keyCpy, elementSize);
	}
}
#endif

void rgfx_renderFullscreenQuad(rgfx_texture tex)
{
//	glCullFace(GL_BACK);
//	glDepthMask(GL_TRUE);
//	glDisable(GL_CULL_FACE);
	glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
#ifdef RE_PLATFORM_WIN64
	glEnable(GL_SAMPLE_ALPHA_TO_ONE);
#endif
//	glEnable(GL_BLEND);

	rgfx_bindVertexBuffer((rgfx_vertexBuffer) { .id = 1 });
	rgfx_bindPipeline(s_rendererData.fsQuadPipeline);
	rgfx_bindTexture(0, tex);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	rgfx_unbindVertexBuffer();
//	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//	glEnable(GL_CULL_FACE);
//	glDisable(GL_BLEND);
#ifdef RE_PLATFORM_WIN64
	glDisable(GL_SAMPLE_ALPHA_TO_ONE);
#endif
	glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
}

void checkForCompileErrors(GLuint shader, const char *shaderName) //, GLint shaderType)
{
	int32_t error = 0;

	glGetShaderiv(shader, GL_COMPILE_STATUS, &error);
	if (!error)
	{
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 0)
		{
			infoLen += 128;
			char* infoLog = (char*)malloc(sizeof(char) * infoLen);
			memset(infoLog, 0, infoLen);
			int32_t readLen = 0;
			glGetShaderInfoLog(shader, infoLen, &readLen, infoLog);
			GLint shaderType = 0;
			glGetShaderiv(shader, GL_SHADER_TYPE, &shaderType);
			if (shaderType == GL_VERTEX_SHADER)
			{
				rsys_print("Error: compiling vertexShader %s: %s\n", shaderName, infoLog);
			}
			else if (shaderType == GL_FRAGMENT_SHADER)
			{
				rsys_print("Error: compiling fragmentShader %s: %s\n", shaderName, infoLog);
			}
			else if (shaderType == GL_COMPUTE_SHADER)
			{
				rsys_print("Error: compiling computeShader\n%s\n", infoLog);
			}
			else
			{
				rsys_print("Error: unknown shader type\n%s\n", infoLog);
			}
			free(infoLog);
		}
	}
}

rgfx_shader rgfx_loadShader(const char* shaderName, const rgfx_shaderType type, rgfx_materialFlags flags)
{
	assert(s_rendererData.numShaders < MAX_SHADERS);
	assert(shaderName != NULL);

	char nameBuffer[1024];
	sprintf(nameBuffer, "%s_%d", shaderName, flags);
	uint32_t hashedShaderName = hashString(nameBuffer);
	int32_t shaderIdx = rsys_hm_find(&s_rendererData.shaderLookup, hashedShaderName);
	if (shaderIdx < 0)
	{
		rsys_file file = file_open(shaderName, "rt");
		if (!file_is_open(file))
		{
			printf("Error: Unable to open file %s\n", shaderName);
			exit(1);
		}
		size_t length = file_length(file);
		char* vs = (char*)malloc(length + 1);
		memset(vs, 0, length + 1);
		file_read(vs, length, file);
		file_close(file);

#ifdef RE_PLATFORM_WIN64
		static const char* programVersion = "#version 460 core\n";
#else
		static const char* programVersion = "#version 320 es\n";
#endif
		const char* compileStrings[20] = { 0 }; // programVersion, NULL, NULL, NULL};
		int32_t compileStringCount;
		GLenum shaderType;

		int32_t stringIndex = 0;
		compileStrings[stringIndex++] = programVersion;
		uint32_t remainingFlags = flags;
		for (int32_t bitIdx = 0; bitIdx < 16 - 1 && remainingFlags; ++bitIdx)
		{
			uint16_t bit = (1 << bitIdx) & flags;
			switch (bit)
			{
			case kMFDiffuseMap:
				compileStrings[stringIndex++] = "#define USE_DIFFUSEMAP\n";
				break;
			case kMFNormalMap:
				compileStrings[stringIndex++] = "#define USE_NORMALMAP\n";
				break;
			case kMFMetallicMap:
				compileStrings[stringIndex++] = "#define USE_METALLICMAP\n";
				break;
			case kMFRoughnessMap:
				compileStrings[stringIndex++] = "#define USE_ROUGHNESSMAP\n";
				break;
			case kMFEmissiveMap:
				compileStrings[stringIndex++] = "#define USE_EMISSIVEMAP\n";
				break;
			case kMFAmbientMap:
				compileStrings[stringIndex++] = "#define USE_AOMAP\n";
				break;
			case kMFSkinned:
				compileStrings[stringIndex++] = "#define USE_SKINNING\n";
				break;
			case kMFFog:
				compileStrings[stringIndex++] = "#define USE_FOG\n";
				break;
			case kMFInstancedAO:
				compileStrings[stringIndex++] = "#define USE_INSTANCED_AO\n";
				break;
			case kMFInstancedAlbedo:
				compileStrings[stringIndex++] = "#define USE_INSTANCED_ALBEDO\n";
				break;
			}
			remainingFlags >>= 1;
		}

		switch (type)
		{
		case kVertexShader:
			shaderType = GL_VERTEX_SHADER;
			compileStrings[stringIndex++] = (s_rendererData.extensions.multi_view) ? "#define DISABLE_MULTIVIEW 0\n" : "#define DISABLE_MULTIVIEW 1\n";
			compileStrings[stringIndex++] = vs;
			compileStringCount = stringIndex;
			break;
		case kFragmentShader:
			shaderType = GL_FRAGMENT_SHADER;
			compileStrings[stringIndex++] = vs;
			compileStringCount = stringIndex;
			break;
		default:
			shaderType = GL_INVALID_ENUM;
			compileStringCount = 0;
			assert(0);
		}

#ifdef USE_SEPARATE_SHADERS_OBJECTS
		//	GLuint glShaderName = glCreateShaderProgramv(shaderType, compileStringCount, compileStrings);
		GLuint glShaderName = glCreateShader(shaderType);
		glShaderSource(glShaderName, compileStringCount, compileStrings, NULL);
		glCompileShader(glShaderName);
		checkForCompileErrors(glShaderName, shaderName);
		GLuint glShaderProgramName = glCreateProgram();

		glAttachShader(glShaderProgramName, glShaderName);
		glProgramParameteri(glShaderProgramName, GL_PROGRAM_SEPARABLE, GL_TRUE);
		glLinkProgram(glShaderProgramName);
#else
		GLuint glShaderProgramName = glCreateShader(shaderType);
		glShaderSource(glShaderProgramName, compileStringCount, compileStrings, NULL);
		glCompileShader(glShaderProgramName);
		checkForCompileErrors(glShaderProgramName);
#endif
		free(vs);

		rsys_hm_insert(&s_rendererData.shaderLookup, hashedShaderName, s_rendererData.numShaders);

		shaderIdx = s_rendererData.numShaders++;
		assert(shaderIdx < MAX_SHADERS);
		s_rendererData.shaders[shaderIdx].name = glShaderProgramName;
		s_rendererData.shaders[shaderIdx].type = shaderType;
	}

	return (rgfx_shader) { .id = shaderIdx + 1 };
}

#pragma GCC diagnostic pop

