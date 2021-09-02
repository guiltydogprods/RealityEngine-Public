#include "stdafx.h"
#include "rgfx/renderer.h"
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

extern VrApi vrapi;
#endif

#include "vrsystem.h"

#if !defined( GL_EXT_buffer_storage )
static const int GL_MAP_PERSISTENT_BIT_EXT = 0x0040;
static const int GL_MAP_COHERENT_BIT_EXT = 0x0080;
static const int GL_DYNAMIC_STORAGE_BIT_EXT = 0x0100;
static const int GL_CLIENT_STORAGE_BIT_EXT = 0x0200;
typedef void(GL_APIENTRY* PFNGLBUFFERSTORAGEEXTPROC) (GLenum target, GLsizeiptr size, const void* data, GLbitfield flags);

PFNGLBUFFERSTORAGEEXTPROC glBufferStorageEXT = NULL;
#endif

#if !defined( GL_EXT_texture_filter_anisotropic )
//#define GL_EXT_texture_filter_anisotropic 1

static const int GL_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FE;
static const int GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;

//#define GLEW_EXT_texture_filter_anisotropic GLEW_GET_VAR(__GLEW_EXT_texture_filter_anisotropic)
#endif /* GL_EXT_texture_filter_anisotropic */

#if !defined( GL_EXT_multisampled_render_to_texture )
typedef void (GL_APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);

PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT = NULL;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT = NULL;
#endif

#if !defined( GL_OVR_multiview )
static const int GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR = 0x9630;
static const int GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR = 0x9632;
static const int GL_MAX_VIEWS_OVR = 0x9631;
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews);

PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR = NULL;
#endif

#if !defined( GL_OVR_multiview_multisampled_render_to_texture )
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLsizei samples, GLint baseViewIndex, GLsizei numViews);

PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR = NULL;
#endif

#define USE_SEPARATE_SHADERS_OBJECTS
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-pointer-arithmetic"

static __attribute__((aligned(64))) rgfx_rendererData s_rendererData = {};

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
	rgfx_iniializeScene();
//	s_rendererData.extensions = params->extensions;
/*
	glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress("glRenderbufferStorageMultisampleEXT");
	glFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT");

	glFramebufferTextureMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)eglGetProcAddress("glFramebufferTextureMultiviewOVR");
	glFramebufferTextureMultisampleMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)eglGetProcAddress("glFramebufferTextureMultisampleMultiviewOVR");

	glBufferStorageEXT = (PFNGLBUFFERSTORAGEEXTPROC)eglGetProcAddress("glBufferStorageEXT");
*/
	s_rendererData.numEyeBuffers = 1;
	if (rvrs_isInitialized())
	{
		memset(s_rendererData.eyeFbInfo, 0, sizeof(rgfx_eyeFbInfo) * kEyeCount);
		s_rendererData.numEyeBuffers = (int32_t)kEyeCount; // VRAPI_FRAME_LAYER_EYE_MAX;

		int32_t numBuffers = kEyeCount; // glewIsSupported("GL_OVR_multiview2")) ? 1 : 2;
		// Create the frame buffers.
		for (int32_t eye = 0; eye < numBuffers; eye++)
		{
			rgfx_createEyeFrameBuffers(eye, 0, 0, kFormatSRGBA8, 4, false); // params->extensions.multi_view);
		}
		s_rendererData.numEyeBuffers = numBuffers;
	}

	GLint numVertexAttribs;
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numVertexAttribs);
	s_rendererData.numVertexBuffers = 0;
#ifdef _DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(openglCallbackFunction, NULL);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, true);
#endif

	glDepthFunc(GL_LESS);
	glCullFace(GL_BACK);
	glDepthMask(GL_TRUE);
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
				.flags = GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT
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
				.flags = GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT
			})
		});
	}

	s_rendererData.cameraTransforms = rgfx_createBuffer(&(rgfx_bufferDesc) {
		.capacity = 1024,
		.stride = sizeof(Transforms),
		.flags = GL_MAP_WRITE_BIT
	});

	s_rendererData.lights = rgfx_createBuffer(&(rgfx_bufferDesc) {
		.capacity = sizeof(rgfx_light) * 8,
		.stride = sizeof(rgfx_light),
		.flags = GL_MAP_WRITE_BIT
	});
}

void rgfx_terminate()
{
}

rgfx_vertexBuffer rgfx_createVertexBuffer(const rgfx_vertexBufferDesc* desc)
{
	int32_t formatIdx = desc->format.id - 1;
	assert(formatIdx >= 0);
	rgfx_vertexFormatInfo* formatInfo = &s_rendererData.vertexFormats[formatIdx];
	int32_t vbIdx = rsys_hm_find(&s_rendererData.vertexBufferLookup, formatInfo->hash);
	if (vbIdx < 0)
	{
		vbIdx = s_rendererData.numVertexBuffers++;
		assert(vbIdx < MAX_VERTEX_BUFFERS);

		rsys_hm_insert(&s_rendererData.vertexBufferLookup, formatInfo->hash, vbIdx);

		rgfx_vertexBufferInfo* info = &s_rendererData.vertexBufferInfo[vbIdx];

		static const uint32_t kDefaultSize = 2 * 1024 * 1024;
		info->format = desc->format;
		info->vertexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
			.type = kVertexBuffer,
			.capacity = desc->capacity > 0 ? desc->capacity : kDefaultSize,
			.stride = formatInfo->stride,
			.flags = GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT
		});
		info->indexBuffer = desc->indexBuffer;

		uint32_t stride = formatInfo->stride;
		int32_t numVertexElements = formatInfo->numElements;

		int32_t bufferIdx = info->vertexBuffer.id - 1;
		assert(bufferIdx >= 0);
		rgfx_bufferInfo* rawBuffer = &s_rendererData.buffers[bufferIdx];
		GLuint vertexArrayName;
		glGenVertexArrays(1, &vertexArrayName);
		glBindVertexArray(vertexArrayName);
		glBindBuffer(GL_ARRAY_BUFFER, rawBuffer->name);

		rgfx_vertexElement* elements = formatInfo->elements;
		uint32_t offset = 0;
		for (uint32_t i = 0; i < numVertexElements; ++i)
		{
			GLenum type;
			uint16_t elementSize = 0;
			bool isInteger = false;
			char* elementName = NULL;
			switch (elements[i].m_type)
			{
			case kFloat:
				type = GL_FLOAT;
				elementSize = sizeof(float) * elements[i].m_size;
				elementName = "GL_FLOAT";
				break;
			case kHalf:
				type = GL_HALF_FLOAT;
				elementSize = sizeof(uint16_t) * elements[i].m_size;
				elementName = "GL_HALF_FLOAT";
				break;
			case kInt2_10_10_10_Rev:
				type = GL_INT_2_10_10_10_REV;
				elementSize = sizeof(uint32_t);
				elementName = "GL_INT_2_10_10_10_REV";
				break;
			case kSignedInt:
				type = GL_INT;
				elementSize = sizeof(int32_t) * elements[i].m_size;
				isInteger = true;
				elementName = "GL_INT";
				break;
			case kUnsignedByte:
				type = GL_UNSIGNED_BYTE;
				elementSize = sizeof(uint8_t) * elements[i].m_size;
				isInteger = elements[i].m_normalized == true ? false : true;
				elementName = "GL_UNSIGNED_BYTE";
				break;
			default:
				assert(0); // (false, "Unsupported element type.\n");
			}
			rsys_print("Element: Index = %d, Size = %d, Type = %s, Offset = %d\n", i, formatInfo->elements[i].m_size, elementName, offset);
			if (isInteger)
				glVertexAttribIPointer(i, elements[i].m_size, type, stride, BUFFER_OFFSET(offset));
			else
				glVertexAttribPointer(i, elements[i].m_size, type, elements[i].m_normalized == 0 ? GL_FALSE : GL_TRUE, stride, BUFFER_OFFSET(offset));
			offset += elementSize;
			glEnableVertexAttribArray(i);
		}
		if (info->indexBuffer.id > 0)
		{
			int32_t ibIdx = info->indexBuffer.id - 1;
			rgfx_bufferInfo* indexBuffer = &s_rendererData.buffers[ibIdx];
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->name);
		}
		glBindVertexArray(0);
/*
// CLR - Use this if we switch VAOs to DSA.
		if (info->indexBuffer.id > 0)
		{
			int32_t ibIdx = info->indexBuffer.id - 1;
			rgfx_bufferInfo* indexBuffer = &s_rendererData.buffers[ibIdx];
			glVertexArrayElementBuffer(vertexArrayName, indexBuffer->name);
			//			int32_t ibIdx = info->indexBuffer.id - 1;
			//			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->name);
		}
*/
		for (uint32_t i = 0; i < numVertexElements; ++i)
		{
			glDisableVertexAttribArray(i);
		}
		info->vao = vertexArrayName;
	}
	return (rgfx_vertexBuffer) { .id = vbIdx + 1 };
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

uint32_t rgfx_writeVertexData(rgfx_vertexBuffer vb, size_t sizeInBytes, uint8_t* vertexData)
{
	int32_t vbIdx = vb.id - 1;
	assert(vbIdx >= 0);
	rgfx_buffer buffer = s_rendererData.vertexBufferInfo[vbIdx].vertexBuffer;
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	uint32_t writeIndex = s_rendererData.buffers[bufferIdx].writeIndex;
	glBindBuffer(GL_ARRAY_BUFFER, s_rendererData.buffers[bufferIdx].name);
	glBufferSubData(GL_ARRAY_BUFFER, writeIndex, sizeInBytes, vertexData);
	s_rendererData.buffers[bufferIdx].writeIndex += sizeInBytes;
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return writeIndex / s_rendererData.buffers[bufferIdx].stride;
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

uint32_t rgfx_writeIndexData(rgfx_vertexBuffer vb, size_t sizeInBytes, uint8_t* indexData, size_t sourceIndexSize)
{
	int32_t vbIdx = vb.id - 1;
	assert(vbIdx >= 0);
	rgfx_buffer buffer = s_rendererData.vertexBufferInfo[vbIdx].indexBuffer;
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_rendererData.buffers[bufferIdx].name);
	uint32_t writeIndex = s_rendererData.buffers[bufferIdx].writeIndex;
	if (sourceIndexSize == s_rendererData.buffers[bufferIdx].stride)
	{
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, s_rendererData.buffers[bufferIdx].writeIndex, sizeInBytes, indexData);
		s_rendererData.buffers[bufferIdx].writeIndex += sizeInBytes;
	}
	else
	{
		uint32_t* srcPtr = (uint32_t*)indexData;
		uint8_t* mappedPtr = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, s_rendererData.buffers[bufferIdx].capacity, s_rendererData.buffers[bufferIdx].flags & GL_MAP_WRITE_BIT);
		uint16_t* destPtr = (uint16_t*)(mappedPtr + s_rendererData.buffers[bufferIdx].writeIndex);
		int32_t numElements = sizeInBytes / sourceIndexSize;
		for (int32_t i = 0; i < numElements; ++i)
		{
			uint32_t index = srcPtr[i];
			assert(index <= 0xffff);
			destPtr[i] = (uint16_t)index;
		}
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
		s_rendererData.buffers[bufferIdx].writeIndex += (sizeInBytes / 2);
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	return writeIndex / s_rendererData.buffers[bufferIdx].stride;
}

void rgfx_writeCompressedMipImageData(rgfx_texture tex, int32_t mipLevel, GLenum internalformat, int32_t width, int32_t height, int32_t imageSize, const void* data)
{
	int32_t texIdx = tex.id - 1;
	assert(texIdx >= 0);
	assert(texIdx < MAX_TEXTURES);
	rgfx_textureInfo* texInfo = &s_rendererData.textures[texIdx];
	glBindTexture(texInfo->target, texInfo->name);
	glCompressedTexImage2D(texInfo->target, mipLevel, internalformat, width, height, 0, imageSize, data);
	glBindTexture(texInfo->target, 0);
}

void rgfx_writeCompressedTexSubImage3D(rgfx_texture tex, int32_t mipLevel, int32_t xOffset, int32_t yOffset, int32_t zOffset, int32_t width, int32_t height, int32_t depth, rgfx_format format, int32_t imageSize, const void* data)
{
	int32_t texIdx = tex.id - 1;
	assert(texIdx >= 0);
	assert(texIdx < MAX_TEXTURES);

	GLenum internalFormat = GL_RGBA8;
	switch (format)
	{
	case kFormatRGBDXT1:
		internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		break;
	case kFormatRGBADXT3:
		internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		break;
	case kFormatRGBADXT5:
		internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		break;
	case kFormatSRGBDXT1:
		internalFormat = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
		break;
	case kFormatSRGBDXT3:
		internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
		break;
	case kFormatSRGBDXT5:
		internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
		break;
	default:
		assert(0);
	}

	rgfx_textureInfo* texInfo = &s_rendererData.textures[texIdx];
	glBindTexture(texInfo->target, texInfo->name);
	glCompressedTexSubImage3D(texInfo->target, mipLevel, xOffset, yOffset, zOffset, width, height, depth, internalFormat, imageSize, data);
	glBindTexture(texInfo->target, 0);
}

rgfx_buffer rgfx_createBuffer(const rgfx_bufferDesc* desc)
{
	assert(s_rendererData.numBuffers < MAX_BUFFERS);

	GLuint type;
	switch (desc->type)
	{
	case kUniformBuffer:
		type = GL_UNIFORM_BUFFER;
		break;
	case kStorageBuffer:
		type = GL_SHADER_STORAGE_BUFFER;
		break;
	case kVertexBuffer:
		type = GL_ARRAY_BUFFER;
		break;
	case kIndexBuffer:
		type = GL_ELEMENT_ARRAY_BUFFER;
		break;
	case kIndirectBuffer:
		type = GL_DRAW_INDIRECT_BUFFER;
		break;
	default:
		type = GL_UNIFORM_BUFFER;
	}

	GLuint bufferName;
	glGenBuffers(1, &bufferName);
	glBindBuffer(type, bufferName);
	glBufferStorage(type, desc->capacity, desc->data, desc->flags);
	int32_t idx = s_rendererData.numBuffers++;
	if (desc->flags & (/*GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_MAP_COHERENT_BIT |*/ GL_MAP_PERSISTENT_BIT_EXT))
	{
		void* mappedPtr = glMapBufferRange(type, 0, desc->capacity, desc->flags);
		s_rendererData.buffers[idx].mappedPtr = mappedPtr;
	}
	s_rendererData.buffers[idx].type = desc->type;
	s_rendererData.buffers[idx].capacity = desc->capacity;
	s_rendererData.buffers[idx].writeIndex = 0;
	s_rendererData.buffers[idx].stride = desc->stride;
	s_rendererData.buffers[idx].flags = desc->flags;
	s_rendererData.buffers[idx].name = bufferName;

	return (rgfx_buffer) { .id = idx + 1 };
}

void* rgfx_mapBuffer(rgfx_buffer buffer)
{
	static const uint32_t mapBits = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	assert((s_rendererData.buffers[bufferIdx].flags & mapBits) != 0);
	uint32_t flags = s_rendererData.buffers[bufferIdx].flags & ~GL_DYNAMIC_STORAGE_BIT;
	return glMapNamedBufferRange(s_rendererData.buffers[bufferIdx].name, 0, s_rendererData.buffers[bufferIdx].capacity, flags); // s_rendererData.buffers[bufferIdx].flags);
}

void* rgfx_mapBufferRange(rgfx_buffer buffer, int64_t offset, int64_t size)
{
	static const uint32_t mapBits = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	assert((s_rendererData.buffers[bufferIdx].flags & mapBits) != 0);
	return glMapNamedBufferRange(s_rendererData.buffers[bufferIdx].name, offset, size, s_rendererData.buffers[bufferIdx].flags);
}

void rgfx_unmapBuffer(rgfx_buffer buffer)
{
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	glUnmapNamedBuffer(s_rendererData.buffers[bufferIdx].name);
}

void rgfx_bindBuffer(int32_t index, rgfx_buffer buffer)
{
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	GLenum target = GL_UNIFORM_BUFFER;
	switch (s_rendererData.buffers[bufferIdx].type)
	{
	case kUniformBuffer:
		target = GL_UNIFORM_BUFFER;
		break;
	case kStorageBuffer:
		target = GL_SHADER_STORAGE_BUFFER;
			break;
	default:
		assert(0);
	}
	glBindBufferBase(target, index, s_rendererData.buffers[bufferIdx].name);
}

void rgfx_bindBufferRange(int32_t index, rgfx_buffer buffer, int64_t offset, int64_t size)
{
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	GLenum target = GL_UNIFORM_BUFFER;
	switch (s_rendererData.buffers[bufferIdx].type)
	{
	case kUniformBuffer:
		target = GL_UNIFORM_BUFFER;
		break;
	case kStorageBuffer:
		target = GL_SHADER_STORAGE_BUFFER;
		break;
	default:
		assert(0);
	}
	glBindBufferRange(target, index, s_rendererData.buffers[bufferIdx].name, offset, size);
}

rgfx_bufferInfo* rgfx_getBufferInfo(rgfx_buffer buffer)
{
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	return &s_rendererData.buffers[bufferIdx];
}

rgfx_pass rgfx_createPass(const rgfx_passDesc* desc)
{
	assert(s_rendererData.numPasses < MAX_RENDER_PASSES);
	int32_t col0Idx = desc->colorAttachments[0].rt.id - 1;
	assert(col0Idx >= 0);
	int32_t depthIdx = desc->depthAttachment.rt.id - 1;
	assert(depthIdx >= 0);
	GLuint frameBuffer;
	glCreateFramebuffers(1, &frameBuffer);
	glNamedFramebufferTexture(frameBuffer, GL_COLOR_ATTACHMENT0, s_rendererData.rtInfo[col0Idx].name, 0);
	glNamedFramebufferTexture(frameBuffer, GL_DEPTH_ATTACHMENT, s_rendererData.rtInfo[depthIdx].name, 0);
	GLint status = glCheckNamedFramebufferStatus(frameBuffer, GL_DRAW_FRAMEBUFFER);
	(void)status;
	assert(status == GL_FRAMEBUFFER_COMPLETE); //, "Framebuffer Incomplete\n");

	int32_t passIdx = s_rendererData.numPasses++;
	s_rendererData.passes[passIdx].frameBufferId = frameBuffer;
	int32_t rtIdx = desc->colorAttachments[0].rt.id - 1;
	assert(rtIdx >= 0);
	s_rendererData.passes[passIdx].width = s_rendererData.rtInfo[rtIdx].width;	// CLR - Probably should check all RT dimensions match.
	s_rendererData.passes[passIdx].height = s_rendererData.rtInfo[rtIdx].height;
	s_rendererData.passes[passIdx].flags = s_rendererData.rtInfo[rtIdx].flags & kRTMultiSampledBit ? kPFNeedsResolve : 0;

	return (rgfx_pass) { .id = passIdx + 1 };
}

GLint g_viewIdLoc = -1;

rgfx_pipeline rgfx_createPipeline(const rgfx_pipelineDesc* desc)
{
#ifdef USE_SEPARATE_SHADERS_OBJECTS
	assert(s_rendererData.numPipelines < MAX_PIPELINES);
	int32_t vertIdx = desc->vertexShader.id - 1;
	assert(vertIdx >= 0);
	int32_t fragIdx = desc->fragmentShader.id - 1;
	assert(fragIdx >= 0);

	GLuint pipeline;
	glGenProgramPipelines(1, &pipeline);
	glBindProgramPipeline(pipeline);
	glUseProgramStages(pipeline, GL_VERTEX_SHADER_BIT, s_rendererData.shaders[vertIdx].name);
	glUseProgramStages(pipeline, GL_FRAGMENT_SHADER_BIT, s_rendererData.shaders[fragIdx].name);
//	g_viewIdLoc = glGetProgramUniformLocation(s_rendererData.shaders[vertIdx].name, "ViewID");
	g_viewIdLoc = glGetProgramResourceLocation(s_rendererData.shaders[vertIdx].name, GL_UNIFORM, "ViewID");

	glValidateProgramPipeline(pipeline);
	GLint infoLogLen = 0;
	glGetProgramPipelineiv(pipeline, GL_INFO_LOG_LENGTH, &infoLogLen);
	char* infoLog = (char*)malloc(sizeof(char) * infoLogLen);
	glGetProgramPipelineInfoLog(pipeline, infoLogLen, NULL, infoLog);
	glBindProgramPipeline(0);

	int32_t pipeIdx = s_rendererData.numPipelines++;
	s_rendererData.pipelines[pipeIdx].name = pipeline;
	s_rendererData.pipelines[pipeIdx].vertexProgramName = s_rendererData.shaders[vertIdx].name;
	s_rendererData.pipelines[pipeIdx].fragmentProgramName = s_rendererData.shaders[fragIdx].name;

	return (rgfx_pipeline) { .id = pipeIdx + 1 };
#else
	assert(s_rendererData.numPipelines < MAX_PIPELINES);
	int32_t vertIdx = desc->vertexShader.id - 1;
	assert(vertIdx >= 0);
	int32_t fragIdx = desc->fragmentShader.id - 1;
	assert(fragIdx >= 0);

	GLuint shaderProgram = glCreateProgram();

	if (s_rendererData.shaders[vertIdx].name != 0)
	{
		glAttachShader(shaderProgram, s_rendererData.shaders[vertIdx].name);
		//		glDeleteShader(fragmentShader);
	}

	if (s_rendererData.shaders[fragIdx].name != 0)
	{
		glAttachShader(shaderProgram, s_rendererData.shaders[fragIdx].name);
		//		glDeleteShader(fragmentShader);
	}
	glLinkProgram(shaderProgram);

	g_viewIdLoc = glGetUniformLocation(shaderProgram, "ViewID");

	int32_t pipeIdx = s_rendererData.numPipelines++;
	s_rendererData.pipelines[pipeIdx].name = shaderProgram;

	return (rgfx_pipeline) { .id = pipeIdx + 1 };
#endif
}

void rgfx_createEyeFrameBuffers(int32_t idx, int32_t width, int32_t height, rgfx_format format, int32_t sampleCount, bool useMultiview)
{
	(void)idx;
	(void)width;
	(void)height;
	(void)format;
	(void)sampleCount;
	(void)useMultiview;
	rvrs_getEyeWidthHeight(&width, &height);
	s_rendererData.eyeFbInfo[idx].width = width;
	s_rendererData.eyeFbInfo[idx].height = height;
	s_rendererData.eyeFbInfo[idx].multisamples = sampleCount;
	s_rendererData.eyeFbInfo[idx].useMultiview = (useMultiview && (glFramebufferTextureMultiviewOVR != NULL)) ? true : false;
	s_rendererData.eyeFbInfo[idx].textureSwapChainIndex = 0;

	useMultiview = false;
	GLenum colorFormat = GL_SRGB8_ALPHA8;
	s_rendererData.eyeFbInfo[idx].colorTextureSwapChain = rvrs_createTextureSwapChain(useMultiview ? kVrTextureType2DArray : kVrTextureType2D, colorFormat, width, height, 1, 3);
	s_rendererData.eyeFbInfo[idx].textureSwapChainLength = rvrs_getTextureSwapChainLength(s_rendererData.eyeFbInfo[idx].colorTextureSwapChain);
#ifndef RENDER_DIRECT_TO_SWAPCHAIN
	s_rendererData.eyeFbInfo[idx].depthBuffers = (GLuint*)malloc(s_rendererData.eyeFbInfo[idx].textureSwapChainLength * sizeof(GLuint));
	s_rendererData.eyeFbInfo[idx].colourBuffers = (GLuint*)malloc(s_rendererData.eyeFbInfo[idx].textureSwapChainLength * sizeof(GLuint));

	GLuint colourBuffer;
	glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &colourBuffer);
	glTextureStorage2DMultisample(colourBuffer, 4, GL_SRGB8_ALPHA8 , width, height, GL_TRUE);
	glTextureParameteri(colourBuffer, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(colourBuffer, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureParameteri(colourBuffer, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(colourBuffer, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	GLuint depthBuffer;
	glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &depthBuffer);
	glTextureStorage2DMultisample(depthBuffer, 4, GL_DEPTH_COMPONENT32F, width, height, GL_TRUE);
	glTextureParameteri(depthBuffer, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(depthBuffer, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureParameteri(depthBuffer, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(depthBuffer, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	GLint status = 0;
	glCreateFramebuffers(1, &s_rendererData.eyeFbInfo[idx].colourBuffers[0]);
	glNamedFramebufferTexture(s_rendererData.eyeFbInfo[idx].colourBuffers[0], GL_DEPTH_ATTACHMENT, depthBuffer, 0);
	glNamedFramebufferTexture(s_rendererData.eyeFbInfo[idx].colourBuffers[0], GL_COLOR_ATTACHMENT0, colourBuffer, 0);
	status = glCheckNamedFramebufferStatus(s_rendererData.eyeFbInfo[idx].colourBuffers[0], GL_DRAW_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE);
#endif
}

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


rgfx_renderTarget rgfx_createRenderTarget(const rgfx_renderTargetDesc* desc)
{
	assert(s_rendererData.numRenderTargets < MAX_RENDER_TARGETS);
	int32_t width = desc->width;
	int32_t height = desc->height;
	rgfx_format format = desc->format;
	rgfx_filter minFilter = desc->minFilter;
	rgfx_filter magFilter = desc->magFilter;
	int32_t sampleCount = desc->sampleCount;

	GLuint bufferTarget = sampleCount > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
	GLuint bufferName;

	glCreateTextures(bufferTarget, 1, &bufferName);

	if (bufferTarget == GL_TEXTURE_2D_MULTISAMPLE)
	{
		switch (format)
		{
		case kFormatARGB8888:
			glTextureStorage2DMultisample(bufferName, sampleCount, GL_RGBA8, width, height, GL_TRUE);
			break;
		case kFormatSRGBA8:
			glTextureStorage2DMultisample(bufferName, sampleCount, GL_SRGB8_ALPHA8_EXT, width, height, GL_TRUE);
			break;
		case kFormatD24S8:
			glTextureStorage2DMultisample(bufferName, sampleCount, GL_DEPTH_COMPONENT, width, height, GL_TRUE);
			break;
		case kFormatD32F:
			glTextureStorage2DMultisample(bufferName, sampleCount, GL_DEPTH_COMPONENT32F, width, height, GL_TRUE);
			break;
		default:
			assert(0); //AssertMsg(0, "Unsupported Render format\n");
		}
	}
	else
	{
		switch (desc->format)
		{
		case kFormatARGB8888:
			glTextureStorage2D(bufferName, 1, GL_RGBA8, width, height);
			break;
		case kFormatSRGBA8:
			glTextureStorage2D(bufferName, 1, GL_SRGB8_ALPHA8_EXT, width, height);
			break;
		case kFormatD24S8:
			glTextureStorage2D(bufferName, 1, GL_DEPTH_COMPONENT, width, height);
			break;
		case kFormatD32F:
			glTextureStorage2D(bufferName, 1, GL_DEPTH_COMPONENT32F, width, height);
			break;
		default:
			assert(0); //AssertMsg(0, "Unsupported Render formdat\n");
		}
		rgfx_filter filters[2] = { minFilter, magFilter };
		GLenum glFilters[2];
		for (int32_t i = 0; i < 2; ++i)
		{
			switch (filters[i])
			{
			case kFilterNearest:
				glFilters[i] = GL_NEAREST;
				break;
			case kFilterLinear:
				glFilters[i] = GL_LINEAR;
				break;
			case kFilterNearestMipmapNearest:
				glFilters[i] = GL_NEAREST_MIPMAP_NEAREST;
				break;
			case kFilterNearestMipmapLinear:
				glFilters[i] = GL_NEAREST_MIPMAP_LINEAR;
				break;
			case kFilterLinearMipmapNearest:
				glFilters[i] = GL_LINEAR_MIPMAP_NEAREST;
				break;
			case kFilterLinearMipmapLinear:
				glFilters[i] = GL_LINEAR_MIPMAP_LINEAR;
				break;
			default:
				assert(0);
			}
		}
		glTextureParameteri(bufferName, GL_TEXTURE_MAG_FILTER, glFilters[0]);
		glTextureParameteri(bufferName, GL_TEXTURE_MIN_FILTER, glFilters[1]);
		glTextureParameteri(bufferName, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(bufferName, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	
	int32_t rtIdx = s_rendererData.numRenderTargets++;
	s_rendererData.rtInfo[rtIdx].name = bufferName;
	s_rendererData.rtInfo[rtIdx].width = width;
	s_rendererData.rtInfo[rtIdx].height = height;
	s_rendererData.rtInfo[rtIdx].flags = desc->sampleCount > 1 ? kRTMultiSampledBit : 0;

	return (rgfx_renderTarget) { .id = rtIdx + 1 };
}

rgfx_texture rgfx_createTexture(const rgfx_textureDesc* desc)
{
	assert(s_rendererData.numTextures < MAX_TEXTURES);
	int32_t width = desc->width;
	int32_t height = desc->height;
	int32_t depth = desc->depth;
	int32_t mipCount = desc->mipCount > 0 ? desc->mipCount : 1;
	rgfx_format format = desc->format;
	rgfx_filter minFilter = desc->minFilter;
	rgfx_filter magFilter = desc->magFilter;
	rgfx_wrapMode wrapS = desc->wrapS;
	rgfx_wrapMode wrapT = desc->wrapT;
	float anisoFactor = desc->anisoFactor;

	GLenum target = GL_TEXTURE_2D;
	switch (desc->type)
	{
	case kTexture2D:
		target = GL_TEXTURE_2D;
		break;
	case kTexture2DArray:
		target = GL_TEXTURE_2D_ARRAY;
		break;
	default:
		rsys_print("Currently unsupported texture type.\n");
		assert(0);
	}

	GLuint texName;
	glGenTextures(1, &texName);
	glBindTexture(target, texName);

	rgfx_filter srcFilters[2] = { minFilter, magFilter };
	GLenum glFilters[2];
	rgfx_wrapMode srcWrap[2] = { wrapS, wrapT };
	GLenum glWrap[2];

	for (uint32_t i = 0; i < 2; ++i)
	{
		switch (srcFilters[i])
		{
		case kFilterNearest:
			glFilters[i] = GL_NEAREST;
			break;
		case kFilterLinear:
			glFilters[i] = GL_LINEAR;
			break;
		case kFilterNearestMipmapNearest:
			glFilters[i] = GL_NEAREST_MIPMAP_NEAREST;
			break;
		case kFilterNearestMipmapLinear:
			glFilters[i] = GL_NEAREST_MIPMAP_LINEAR;
			break;
		case kFilterLinearMipmapNearest:
			glFilters[i] = GL_LINEAR_MIPMAP_NEAREST;
			break;
		case kFilterLinearMipmapLinear:
			glFilters[i] = GL_LINEAR_MIPMAP_LINEAR;
			break;
		default:
			break;
		}
		switch (srcWrap[i])
		{
		case kWrapModeClamp:
			glWrap[i] = GL_CLAMP;
			break;
		case kWrapModeRepeat:
			glWrap[i] = GL_REPEAT;
			break;
		default:
			glWrap[i] = GL_REPEAT;
		}
	}

	if (format > 0)
	{
		GLenum internalFormat = GL_RGBA8;
		switch (format)
		{
		case kFormatRGBDXT1:
			internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
			break;
		case kFormatRGBADXT3:
			internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			break;
		case kFormatRGBADXT5:
			internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			break;
		case kFormatSRGBDXT1:
			internalFormat = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
			break;
		case kFormatSRGBDXT3:
			internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
			break;
		case kFormatSRGBDXT5:
			internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
			break;
		default:
			assert(0);
		}

		switch (desc->type)
		{
		case kTexture2D:
			glTexStorage2D(target, mipCount, internalFormat, width, height);
			break;
		case kTexture2DArray:
			glTexStorage3D(target, mipCount, internalFormat, width, height, depth);
			break;
		default:
			rsys_print("Currently unsupported texture type.\n");
			assert(0);
		}
	}

	glTexParameteri(target, GL_TEXTURE_MIN_FILTER, glFilters[0]);
	glTexParameteri(target, GL_TEXTURE_MAG_FILTER, glFilters[1]);
	glTexParameteri(target, GL_TEXTURE_WRAP_S, glWrap[0]);
	glTexParameteri(target, GL_TEXTURE_WRAP_T, glWrap[1]);
	GLfloat maxAnisotropy = 0.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
	glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy * anisoFactor);

	glBindTexture(target, 0);

	int32_t texIdx = s_rendererData.numTextures++;
	s_rendererData.textures[texIdx].name = texName;
	s_rendererData.textures[texIdx].target = target;

	return (rgfx_texture) { .id = texIdx + 1 };
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

void rgfx_updateTransforms(const Transforms* transforms)
{
	s_rendererData.cameras[0].viewMatrix = transforms->viewMatrix[0];
	s_rendererData.cameras[0].projectionMatrix = transforms->projectionMatrix[0];
	s_rendererData.cameras[0].camPos = transforms->camPos[0];
	s_rendererData.cameras[1].viewMatrix = transforms->viewMatrix[1];
	s_rendererData.cameras[1].projectionMatrix = transforms->projectionMatrix[1];
	s_rendererData.cameras[1].camPos = transforms->camPos[1];

	int32_t idx = s_rendererData.cameraTransforms.id - 1;
	assert(idx >= 0);
	glBindBuffer(GL_UNIFORM_BUFFER, s_rendererData.buffers[idx].name);
	Transforms* block = (Transforms*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(Transforms), s_rendererData.buffers[idx].flags | GL_MAP_INVALIDATE_BUFFER_BIT);
	block->viewMatrix[0] = transforms->viewMatrix[0];
	block->viewMatrix[1] = transforms->viewMatrix[1];
	block->projectionMatrix[0] = transforms->projectionMatrix[0];
	block->projectionMatrix[1] = transforms->projectionMatrix[1];
	block->camPos[0] = transforms->camPos[0];
	block->camPos[1] = transforms->camPos[1];
	glUnmapBuffer(GL_UNIFORM_BUFFER);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
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

void rgfx_endPass()
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

void checkForCompileErrors(GLuint shader) //, GLint shaderType)
{
	int32_t error = 0;

	glGetShaderiv(shader, GL_COMPILE_STATUS, &error);
	if (!error)
	{
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1)
		{
			char* infoLog = (char*)malloc(sizeof(char) * infoLen);
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			GLint shaderType = 0;
			glGetShaderiv(shader, GL_SHADER_TYPE, &shaderType);
			if (shaderType == GL_VERTEX_SHADER)
			{
				printf("Error: compiling vertexShader\n%s\n", infoLog);
			}
			else if (shaderType == GL_FRAGMENT_SHADER)
			{
				printf("Error: compiling fragmentShader\n%s\n", infoLog);
			}
			else if (shaderType == GL_COMPUTE_SHADER)
			{
				printf("Error: compiling computeShader\n%s\n", infoLog);
			}
			else
			{
				printf("Error: unknown shader type\n%s\n", infoLog);
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
		size_t length = file_length(file);
		char* vs = (char*)malloc(length + 1);
		memset(vs, 0, length + 1);
		file_read(vs, length, file);
		file_close(file);

		static const char* programVersion = "#version 460 core\n";
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
			case kMFSkinned:
				compileStrings[stringIndex++] = "#define USE_SKINNING\n";
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
		checkForCompileErrors(glShaderName);
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

