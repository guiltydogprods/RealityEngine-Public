#pragma once

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

static const int GL_TEXTURE_SRGB_DECODE_EXT = 0x8A48;
static const int GL_DECODE_EXT = 0x8A49;
static const int GL_SKIP_DECODE_EXT = 0x8A4A;
static const int GL_FRAMEBUFFER_SRGB_EXT = 0x8DB9;
static const int GL_FRAMEBUFFER_SRGB = 0x8DB9;

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

extern rgfx_rendererData s_rendererData;
void openglCallbackFunction(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);

void rgfx_initializeExtensions(const rgfx_extensions extensions)
{
	s_rendererData.extensions = extensions;

	glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress("glRenderbufferStorageMultisampleEXT");
	glFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT");

	glFramebufferTextureMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)eglGetProcAddress("glFramebufferTextureMultiviewOVR");
	glFramebufferTextureMultisampleMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)eglGetProcAddress("glFramebufferTextureMultisampleMultiviewOVR");

	glBufferStorageEXT = (PFNGLBUFFERSTORAGEEXTPROC)eglGetProcAddress("glBufferStorageEXT");
}

void rgfx_terminate()
{
/*
	for (int32_t eye = 0; eye < s_rendererData.numEyeBuffers; eye++)
	{
		glDeleteFramebuffers(s_rendererData.eyeFbInfo[eye].textureSwapChainLength, s_rendererData.eyeFbInfo[eye].colourBuffers);
		if (s_rendererData.eyeFbInfo[eye].useMultiview)
		{
			glDeleteTextures(s_rendererData.eyeFbInfo[eye].textureSwapChainLength, s_rendererData.eyeFbInfo[eye].depthBuffers);
		}
		else
		{
			glDeleteRenderbuffers(s_rendererData.eyeFbInfo[eye].textureSwapChainLength, s_rendererData.eyeFbInfo[eye].depthBuffers);
		}
		rvrs_destroyTextureSwapChain(s_rendererData.eyeFbInfo[eye].colorTextureSwapChain);
		free(s_rendererData.eyeFbInfo[eye].colourBuffers);
		free(s_rendererData.eyeFbInfo[eye].depthBuffers);
		memset(&s_rendererData.eyeFbInfo[eye], 0, sizeof(rgfx_eyeFbInfo));
	}
*/
	rgfx_destroyEyeFrameBuffers();
	s_rendererData.numEyeBuffers = VRAPI_FRAME_LAYER_EYE_MAX;
	memset(&s_rendererData, 0, sizeof(rgfx_rendererData));
}

// -  Buffers
inline GLuint rgfx_getNativeBufferFlags(uint32_t flags)
{
	GLuint nativeFlags = 0;
	if (flags & kMapReadBit)
		nativeFlags |= GL_MAP_READ_BIT;
	if (flags & kMapWriteBit)
		nativeFlags |= GL_MAP_WRITE_BIT;
	if (flags & kMapPersistentBit)
		nativeFlags |= GL_MAP_PERSISTENT_BIT_EXT;
	if (flags & kMapCoherentBit)
		nativeFlags |= GL_MAP_COHERENT_BIT_EXT;
	if (flags & kDynamicStorageBit)
		nativeFlags |= GL_DYNAMIC_STORAGE_BIT_EXT;

	return nativeFlags;
}

rgfx_buffer rgfx_createBuffer(const rgfx_bufferDesc* desc)
{
	int32_t idx = rgfx_allocResource(kResourceBuffer);
	assert(idx >= 0);
	assert(idx < MAX_BUFFERS);

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
	
	GLuint nativeFlags = rgfx_getNativeBufferFlags(desc->flags);
	GLuint bufferName;
	glGenBuffers(1, &bufferName);
	glBindBuffer(type, bufferName);
	glBufferStorageEXT(type, desc->capacity, NULL, nativeFlags);
	if (nativeFlags & (/*GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_MAP_COHERENT_BIT |*/ GL_MAP_PERSISTENT_BIT_EXT))
	{
		void* mappedPtr = glMapBufferRange(type, 0, desc->capacity, nativeFlags);
		s_rendererData.buffers[idx].mappedPtr = mappedPtr;
	}
	s_rendererData.buffers[idx].type = desc->type;
	s_rendererData.buffers[idx].capacity = desc->capacity;
	s_rendererData.buffers[idx].writeIndex = 0;
	s_rendererData.buffers[idx].stride = desc->stride;
	s_rendererData.buffers[idx].flags = nativeFlags;
	s_rendererData.buffers[idx].name = bufferName;

	return (rgfx_buffer) { idx + 1 };
}

inline void rgfx_destroyBuffer(rgfx_buffer buffer)
{
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	glDeleteBuffers(1, &s_rendererData.buffers[bufferIdx].name);
	s_rendererData.buffers[bufferIdx].name = 0;
	s_rendererData.buffers[bufferIdx].capacity = 0;
	s_rendererData.buffers[bufferIdx].writeIndex = 0;
	rgfx_freeResource(kResourceBuffer, bufferIdx);
}

void* rgfx_mapBuffer(rgfx_buffer buffer)
{
	static const uint32_t mapBits = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT; // | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	assert((s_rendererData.buffers[bufferIdx].flags & mapBits) != 0);

	GLuint type;
	switch (s_rendererData.buffers[bufferIdx].type)
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
	glBindBuffer(type, s_rendererData.buffers[bufferIdx].name);
	uint32_t flags = s_rendererData.buffers[bufferIdx].flags & ~GL_DYNAMIC_STORAGE_BIT_EXT;
	return glMapBufferRange(type, 0, s_rendererData.buffers[bufferIdx].capacity, flags);
}

void* rgfx_mapBufferRange(rgfx_buffer buffer, int64_t offset, int64_t size)
{
	static const uint32_t mapBits = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT; // | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	assert((s_rendererData.buffers[bufferIdx].flags & mapBits) != 0);

	GLuint type;
	switch (s_rendererData.buffers[bufferIdx].type)
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
	glBindBuffer(type, s_rendererData.buffers[bufferIdx].name);
//	return glMapBufferRange(type, offset, size, s_rendererData.buffers[bufferIdx].flags);
	uint32_t flags = s_rendererData.buffers[bufferIdx].flags & ~GL_DYNAMIC_STORAGE_BIT_EXT;
	return glMapBufferRange(type, offset, size, flags);
}

void rgfx_unmapBuffer(rgfx_buffer buffer)
{
	int32_t bufferIdx = buffer.id - 1;
	assert(bufferIdx >= 0);
	GLuint type;
	switch (s_rendererData.buffers[bufferIdx].type)
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
	glBindBuffer(type, s_rendererData.buffers[bufferIdx].name);
	glUnmapBuffer(type);
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
	case kVertexBuffer:
		glBindBuffer(GL_ARRAY_BUFFER, s_rendererData.buffers[bufferIdx].name);
		return;
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

// - Vertex Buffers

rgfx_vertexBuffer rgfx_createVertexBuffer(const rgfx_vertexBufferDesc* desc)
{
	int32_t formatIdx = desc->format.id - 1;
	assert(formatIdx >= 0);
	rgfx_vertexFormatInfo* formatInfo = &s_rendererData.vertexFormats[formatIdx];
	int32_t vbIdx = -1; //rsys_hm_find(&s_rendererData.vertexBufferLookup, formatInfo->hash);
//	if (vbIdx < 0)
	{
		vbIdx = rgfx_allocResource(kResourceVertexBuffer);
		assert(vbIdx >= 0);
		assert(vbIdx < MAX_VERTEX_BUFFERS);

		rsys_hm_insert(&s_rendererData.vertexBufferLookup, formatInfo->hash, vbIdx);

		rgfx_vertexBufferInfo* info = &s_rendererData.vertexBufferInfo[vbIdx];

		static const uint32_t kDefaultSize = 2 * 1024 * 1024;
		info->format = desc->format;
		info->vertexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
			.capacity = desc->capacity > 0 ? desc->capacity : kDefaultSize,
			.stride = formatInfo->stride,
			.flags = kMapWriteBit | kDynamicStorageBit
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
		for (uint32_t i = 0; i < numVertexElements; ++i)
		{
			glDisableVertexAttribArray(i);
		}
		info->vao = vertexArrayName;
	}
	return (rgfx_vertexBuffer) { .id = vbIdx + 1 };
}

inline void rgfx_destroyVertexBuffer(rgfx_vertexBuffer vb)
{
	int32_t vbIdx = vb.id - 1;
	assert(vbIdx >= 0);
	rgfx_vertexBufferInfo* info = &s_rendererData.vertexBufferInfo[vbIdx];
	rgfx_destroyBuffer(info->vertexBuffer);
	info->vertexBuffer.id = 0;
	if (info->indexBuffer.id > 0)
	{
		rgfx_destroyBuffer(info->indexBuffer);
		info->indexBuffer.id = 0;
	}
	glDeleteVertexArrays(1, &info->vao);
	info->vao = 0;
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

inline GLenum rgfx_getNativePrimType(rgfx_primType primType)
{
	GLenum nativePrimType = { 0 };
	switch (primType)
	{
	case kPrimTypePoints:
		nativePrimType = GL_POINTS;
		break;
	case kPrimTypeLines:
		nativePrimType = GL_LINES;
		break;
	case kPrimTypeLineStrip:
		nativePrimType = GL_LINE_STRIP;
		break;
	case kPrimTypeTriangles:
		nativePrimType = GL_TRIANGLES;
		break;
	case kPrimTypeTriangleStrip:
		nativePrimType = GL_TRIANGLE_STRIP;
		break;
	case kPrimTypeTriangleFan:
		nativePrimType = GL_TRIANGLE_FAN;
		break;
	}
	return nativePrimType;
}

inline GLenum rgfx_getNativeTextureFormat(rgfx_format format)
{
	GLenum internalFormat = { 0 };
	switch (format)
	{
	case kFormatSRGBA8:
		internalFormat = GL_SRGB8_ALPHA8;
		break;
	case kFormatSRGBASTC4x4:
		internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4;
		break;
	case kFormatRGBAASTC4x4:
		internalFormat = GL_COMPRESSED_RGBA_ASTC_4x4;
		break;
	case kFormatSRGBASTC6x6:
		internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6;
		break;
	case kFormatRGBAASTC6x6:
		internalFormat = GL_COMPRESSED_RGBA_ASTC_6x6;
		break;
	default:
		assert(0);
	}
	return internalFormat;
}

rgfx_pass rgfx_createPass(const rgfx_passDesc* desc)
{
	assert(s_rendererData.numPasses < MAX_RENDER_PASSES);
	int32_t col0Idx = desc->colorAttachments[0].rt.id - 1;
	assert(col0Idx >= 0);
	int32_t depthIdx = desc->depthAttachment.rt.id - 1;
	assert(depthIdx >= 0);
	GLuint frameBuffer;
	glGenFramebuffers(1, &frameBuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer);
	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_rendererData.rtInfo[depthIdx].name);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_rendererData.rtInfo[col0Idx].name, 0);
	/* handle unsupported cases */
	GLint status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE); //, "Framebuffer Incomplete\n");
/*
	GLuint frameBuffer;
	glCreateFramebuffers(1, &frameBuffer);
	glNamedFramebufferTexture(frameBuffer, GL_COLOR_ATTACHMENT0, s_rendererData.rtInfo[desc->colorAttachments[0].rt.id].name, 0);
	glNamedFramebufferTexture(frameBuffer, GL_DEPTH_ATTACHMENT, s_rendererData.rtInfo[desc->depthAttachment.rt.id].name, 0);
	GLint status = glCheckNamedFramebufferStatus(frameBuffer, GL_DRAW_FRAMEBUFFER);
	assert(status == GL_FRAMEBUFFER_COMPLETE); //, "Framebuffer Incomplete\n");
*/
	int32_t passIdx = s_rendererData.numPasses++;
	s_rendererData.passes[passIdx].frameBufferId = frameBuffer;
	int32_t rtIdx = desc->colorAttachments[0].rt.id - 1;
	assert(rtIdx >= 0);
	s_rendererData.passes[passIdx].width = s_rendererData.rtInfo[rtIdx].width;
	s_rendererData.passes[passIdx].height = s_rendererData.rtInfo[rtIdx].height;
	s_rendererData.passes[passIdx].flags = s_rendererData.rtInfo[rtIdx].flags & kRTMultiSampledBit ? kPFNeedsResolve : 0;

	return (rgfx_pass) { passIdx + 1 };
}

extern GLint g_viewIdLoc;

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
	g_viewIdLoc = glGetUniformLocation(s_rendererData.shaders[vertIdx].name, "ViewID");
	glValidateProgramPipeline(pipeline);
	GLint infoLogLen = 0;
	glGetProgramPipelineiv(pipeline, GL_INFO_LOG_LENGTH, &infoLogLen);
	if (infoLogLen > 0)
	{
		printf("Break here...\n");
	}
	char* infoLog = (char*)malloc(sizeof(char) * infoLogLen);
	glGetProgramPipelineInfoLog(pipeline, infoLogLen, NULL, infoLog);
	glBindProgramPipeline(0);

	int32_t pipeIdx = s_rendererData.numPipelines++;
	s_rendererData.pipelines[pipeIdx].name = pipeline;
	s_rendererData.pipelines[pipeIdx].vertexProgramName = s_rendererData.shaders[vertIdx].name;
	s_rendererData.pipelines[pipeIdx].fragmentProgramName = s_rendererData.shaders[fragIdx].name;

	return (rgfx_pipeline) { pipeIdx + 1 };
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
	s_rendererData.eyeFbInfo[idx].width = width;
	s_rendererData.eyeFbInfo[idx].height = height;
	s_rendererData.eyeFbInfo[idx].multisamples = sampleCount;
	s_rendererData.eyeFbInfo[idx].useMultiview = (useMultiview && (glFramebufferTextureMultiviewOVR != NULL)) ? true : false;
	s_rendererData.eyeFbInfo[idx].textureSwapChainIndex = 0;

	rgfx_format colorFormat = kFormatSRGBA8;
	s_rendererData.eyeFbInfo[idx].colorTextureSwapChain = rvrs_createTextureSwapChain(useMultiview ? kVrTextureType2DArray : kVrTextureType2D, colorFormat, width, height, 1, 3);
	s_rendererData.eyeFbInfo[idx].textureSwapChainLength = rvrs_getTextureSwapChainLength(s_rendererData.eyeFbInfo[idx].colorTextureSwapChain);

	s_rendererData.eyeFbInfo[idx].depthBuffers = (GLuint*)malloc(s_rendererData.eyeFbInfo[idx].textureSwapChainLength * sizeof(GLuint));
	s_rendererData.eyeFbInfo[idx].colourBuffers = (GLuint*)malloc(s_rendererData.eyeFbInfo[idx].textureSwapChainLength * sizeof(GLuint));

	for (int32_t i = 0; i < s_rendererData.eyeFbInfo[idx].textureSwapChainLength; i++)
	{
		// Create the color buffer texture.
//		const GLuint colorTexture = vrapi->GetTextureSwapChainHandle(s_rendererData.eyeFbInfo[idx].colorTextureSwapChain, i);
		const GLuint colorTexture = rvrs_getTextureSwapChainBufferGL(s_rendererData.eyeFbInfo[idx].colorTextureSwapChain, i); // vrapi.GetTextureSwapChainHandle(s_rendererData.eyeFbInfo[idx].colorTextureSwapChain, i);
		GLenum colorTextureTarget = useMultiview ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
		glBindTexture(colorTextureTarget, colorTexture);
		if (s_rendererData.extensions.EXT_texture_border_clamp)
		{
			glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			GLfloat borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			glTexParameterfv(colorTextureTarget, GL_TEXTURE_BORDER_COLOR, borderColor);
		}
		else
		{
			// Just clamp to edge. However, this requires manually clearing the border
			// around the layer to clear the edge texels.
			glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		glTexParameteri(colorTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(colorTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(colorTextureTarget, 0);

		if (useMultiview)
		{
			// Create the depth buffer texture.
			glGenTextures(1, &s_rendererData.eyeFbInfo[idx].depthBuffers[i]);
			glBindTexture(GL_TEXTURE_2D_ARRAY, s_rendererData.eyeFbInfo[idx].depthBuffers[i]);
			glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT24, width, height, 2);
			glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

			// Create the frame buffer.
			glGenFramebuffers(1, &s_rendererData.eyeFbInfo[idx].colourBuffers[i]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_rendererData.eyeFbInfo[idx].colourBuffers[i]);
			if (sampleCount > 1 && (glFramebufferTextureMultisampleMultiviewOVR != NULL))
			{
				glFramebufferTextureMultisampleMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, s_rendererData.eyeFbInfo[idx].depthBuffers[i], 0 /* level */, sampleCount /* samples */, 0 /* baseViewIndex */, 2 /* numViews */);
				glFramebufferTextureMultisampleMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture, 0 /* level */, sampleCount /* samples */, 0 /* baseViewIndex */, 2 /* numViews */);
			}
			else
			{
				glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, s_rendererData.eyeFbInfo[idx].depthBuffers[i], 0 /* level */, 0 /* baseViewIndex */, 2 /* numViews */);
				glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture, 0 /* level */, 0 /* baseViewIndex */, 2 /* numViews */);
			}
			GLint colourEncoding = 0;
			glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &colourEncoding);
			GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE)
			{
				rsys_print("FrameBuffer Incomplete.\n");
				//				ALOGE("Incomplete frame buffer object: %s", GlFrameBufferStatusString(renderFramebufferStatus));
				//				return false;
			}
		}
		else
		{
			if (sampleCount > 1 && glRenderbufferStorageMultisampleEXT != NULL && glFramebufferTexture2DMultisampleEXT != NULL)
			{
				// Create multisampled depth buffer.
				glGenRenderbuffers(1, &s_rendererData.eyeFbInfo[idx].depthBuffers[i]);
				glBindRenderbuffer(GL_RENDERBUFFER, s_rendererData.eyeFbInfo[idx].depthBuffers[i]);
				glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, sampleCount, GL_DEPTH_COMPONENT24, width, height);
				glBindRenderbuffer(GL_RENDERBUFFER, 0);

				// Create the frame buffer.
				// NOTE: glFramebufferTexture2DMultisampleEXT only works with GL_FRAMEBUFFER.
				glGenFramebuffers(1, &s_rendererData.eyeFbInfo[idx].colourBuffers[i]);
				glBindFramebuffer(GL_FRAMEBUFFER, s_rendererData.eyeFbInfo[idx].colourBuffers[i]);
				glFramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0, sampleCount);
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_rendererData.eyeFbInfo[idx].depthBuffers[i]);
				GLint colourEncoding = 0;
				glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &colourEncoding);
				GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE)
				{
					rsys_print("FrameBuffer Incomplete.\n");
					//					ALOGE("Incomplete frame buffer object: %s", GlFrameBufferStatusString(renderFramebufferStatus));
					//					return false;
				}
			}
			else
			{
				// Create depth buffer.
				glGenRenderbuffers(1, &s_rendererData.eyeFbInfo[idx].depthBuffers[i]);
				glBindRenderbuffer(GL_RENDERBUFFER, s_rendererData.eyeFbInfo[idx].depthBuffers[i]);
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
				glBindRenderbuffer(GL_RENDERBUFFER, 0);

				// Create the frame buffer.
				glGenFramebuffers(1, &s_rendererData.eyeFbInfo[idx].colourBuffers[i]);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_rendererData.eyeFbInfo[idx].colourBuffers[i]);
				glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_rendererData.eyeFbInfo[idx].depthBuffers[i]);
				glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
				GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
				if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE)
				{
					rsys_print("FrameBuffer Incomplete.\n");
					//					ALOGE("Incomplete frame buffer object: %s", GlFrameBufferStatusString(renderFramebufferStatus));
				}
			}
		}
	}
}

void rgfx_destroyEyeFrameBuffers()
{
	for (int32_t eye = 0; eye < s_rendererData.numEyeBuffers; eye++)
	{
		glDeleteFramebuffers(s_rendererData.eyeFbInfo[eye].textureSwapChainLength, s_rendererData.eyeFbInfo[eye].colourBuffers);
		if (s_rendererData.eyeFbInfo[eye].useMultiview)
		{
			glDeleteTextures(s_rendererData.eyeFbInfo[eye].textureSwapChainLength, s_rendererData.eyeFbInfo[eye].depthBuffers);
		}
		else
		{
			glDeleteRenderbuffers(s_rendererData.eyeFbInfo[eye].textureSwapChainLength, s_rendererData.eyeFbInfo[eye].depthBuffers);
		}
		rvrs_destroyTextureSwapChain(s_rendererData.eyeFbInfo[eye].colorTextureSwapChain);
		free(s_rendererData.eyeFbInfo[eye].colourBuffers);
		free(s_rendererData.eyeFbInfo[eye].depthBuffers);
		memset(&s_rendererData.eyeFbInfo[eye], 0, sizeof(rgfx_eyeFbInfo));
	}
}

void rgfx_scaleEyeFrameBuffers(int32_t newScale)
{
	rvrs_eyeSize newEyeSize = rvrs_getScaledEyeSize(newScale);

	int32_t numBuffers = s_rendererData.numEyeBuffers;
	rgfx_destroyEyeFrameBuffers();
	// Create the frame buffers.
	for (int32_t eye = 0; eye < numBuffers; eye++)
	{
		rgfx_createEyeFrameBuffers(eye, newEyeSize.width, newEyeSize.height, kFormatSRGBA8, 4, numBuffers == 1 ? true : false);
	}
	s_rendererData.numEyeBuffers = numBuffers;
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

	GLenum bufferTarget; // = /*sampleCount > 1 ? GL_TEXTURE_2D_MULTISAMPLE :*/ GL_TEXTURE_2D;
	GLuint bufferName;

	if (format == kFormatD24S8 || format == kFormatD32F)
	{
		bufferTarget = GL_RENDERBUFFER;
		glGenRenderbuffers(1, &bufferName);
		glBindRenderbuffer(GL_RENDERBUFFER, bufferName);
	}
	else
	{
		bufferTarget = GL_TEXTURE_2D;
		glGenTextures(1, &bufferName);
		glBindTexture(bufferTarget, bufferName);
	}

#if 0
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
#endif
	{
		switch (format)
		{
		case kFormatARGB8888:
			glTexStorage2D(bufferTarget, 1, GL_RGBA8, width, height);
			break;
		case kFormatSRGBA8:
			glTexStorage2D(bufferTarget, 1, GL_SRGB8_ALPHA8, width, height);
			break;
		case kFormatD24S8:
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
			break;
		case kFormatD32F:
			//			glTexStorage2D(bufferTarget, 1, GL_DEPTH_COMPONENT32F, width, height);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
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
		if (desc->format == kFormatD24S8 || desc->format == kFormatD32F)
		{
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
		}
		else
		{
			glTexParameteri(bufferTarget, GL_TEXTURE_MAG_FILTER, glFilters[0]);
			glTexParameteri(bufferTarget, GL_TEXTURE_MIN_FILTER, glFilters[1]);
			glTexParameteri(bufferTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(bufferTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(bufferTarget, 0);
		}
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
	int32_t texIdx = rgfx_allocResource(kResourceTexture);
	assert(texIdx >= 0);
	assert(texIdx < MAX_TEXTURES);

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

	rgfx_filter srcFilters[2] = { desc->minFilter, desc->magFilter };
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
		}
		switch (srcWrap[i])
		{
		case kWrapModeClamp:
			glWrap[i] = GL_CLAMP_TO_EDGE;
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
		case kFormatSRGBASTC4x4:
			internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4;
			break;
		case kFormatRGBAASTC4x4:
			internalFormat = GL_COMPRESSED_RGBA_ASTC_4x4;
			break;
		case kFormatSRGBASTC6x6:
			internalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6;
			break;
		case kFormatRGBAASTC6x6:
			internalFormat = GL_COMPRESSED_RGBA_ASTC_6x6;
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
	glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy * desc->anisoFactor);
	glTexParameteri(target, GL_TEXTURE_SRGB_DECODE_EXT, GL_DECODE_EXT);
	//	glTexParameteri(target, GL_TEXTURE_SRGB_DECODE_EXT, GL_SKIP_DECODE_EXT);
	glBindTexture(target, 0);

	s_rendererData.textures[texIdx].name = texName;
	s_rendererData.textures[texIdx].target = target;
	s_rendererData.textures[texIdx].width = width;
	s_rendererData.textures[texIdx].height = height;
	s_rendererData.textures[texIdx].depth = depth;

	return (rgfx_texture) { .id = texIdx + 1 };
}

void rgfx_destroyTexture(rgfx_texture tex)
{
	int32_t texIdx = tex.id - 1;
	assert(texIdx >= 0);
	glDeleteTextures(1, &s_rendererData.textures[texIdx].name);
	s_rendererData.textures[texIdx].name = 0;
	s_rendererData.textures[texIdx].target = 0;
	s_rendererData.textures[texIdx].internalFormat = 0;
	s_rendererData.textures[texIdx].width = 0;
	s_rendererData.textures[texIdx].height = 0;
	s_rendererData.textures[texIdx].depth = 0;
	rgfx_freeResource(kResourceTexture, texIdx);
}
