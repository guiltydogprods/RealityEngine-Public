#pragma once
#include "math/mat4x4.h"
#include "hash.h"
#include "debugRender.h"

#ifdef RE_PLATFORM_WIN64
#include <GL/glew.h>
//#include <GL/wglew.h>
#include <GL/gl.h>
//#include <GLFW/glfw3.h>
//#include "vrsystem.h"
#else
//#include "vrsystem.h"
#endif

#define USE_SEPARATE_SHADERS_OBJECTS
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#define MAX_VERTEX_ELEMENTS	(16)		// CLR - These are all just temporary for now.
#define MAX_VERTEX_FORMATS	(16)
#define MAX_VERTEX_BUFFERS	(64)
#define MAX_BUFFERS			(256)
#define MAX_EYE_BUFFERS		(2)
#define MAX_RENDER_TARGETS	(16)
#define MAX_RENDER_PASSES	(16)
#define MAX_PIPELINES		(16)
#define MAX_SHADERS			(32)
#define MAX_TEXTURES		(128)
#define MAX_CAMERAS			(4)

typedef struct rvrs_textureSwapChain { int32_t id; } rvrs_textureSwapChain;

typedef struct DrawElementsIndirectCommand
{
	uint32_t	count;
	uint32_t	instanceCount;
	uint32_t	firstIndex;
	uint32_t	baseVertex;
	uint32_t	baseInstance;
}DrawElementsIndirectCommand;

typedef struct rgfx_buffer { int32_t id; } rgfx_buffer;
typedef struct rgfx_mesh { int32_t id; } rgfx_mesh;
typedef struct rgfx_meshInstance { int32_t id; } rgfx_meshInstance;
typedef struct rgfx_pass { int32_t id; } rgfx_pass;
typedef struct rgfx_pipeline { int32_t id; } rgfx_pipeline;
typedef struct rgfx_renderable { int32_t id;  } rgfx_renderable;
typedef struct rgfx_renderTarget { int32_t id; } rgfx_renderTarget;
typedef struct rgfx_shader { int32_t id; } rgfx_shader;
typedef struct rgfx_texture { int32_t id; } rgfx_texture;
typedef struct rgfx_vertexBuffer { int32_t id; } rgfx_vertexBuffer;
typedef struct rgfx_vertexFormat { int32_t id; } rgfx_vertexFormat;

typedef enum rgfx_bufferType
{
	kUniformBuffer = 0,					// CLR: Default Buffer Type?
	kStorageBuffer,
	kIndexBuffer,
	kVertexBuffer,
	kIndirectBuffer,
}rgfx_bufferType;

typedef enum rgfx_bufferFlagBits
{
	kMapReadBit = 1,
	kMapWriteBit = 2,
	kMapPersistentBit = 4,
	kMapCoherentBit = 8,
	kDynamicStorageBit = 0x10,
}rgfx_bufferFlagBits;

typedef struct rgfx_resourceHeader
{
	int32_t next;
}rgfx_resourceHeader;

typedef struct rgfx_bufferInfo			// 32 bytes
{
	rgfx_resourceHeader header;			// 4 bytes
	rgfx_bufferType	type;				// 4 bytes
	uint8_t*		mappedPtr;			// 8 bytes
	uint32_t		capacity;			// 4 bytes
	uint32_t		writeIndex;			// 4 bytes
	uint32_t		stride;				// 4 bytes
	uint32_t		flags;				// 4 bytes
	GLuint			name;				// 4 bytes
}rgfx_bufferInfo;

typedef struct rgfx_instancedMeshInfo
{
	int32_t instanceCount;
	int32_t batchCount;
	int32_t firstBatch;
}rgfx_instancedMeshInfo;

typedef enum rgfx_materialFlagBits
{
	kMFNone = 0,
	kMFDiffuseMap = 1,
	kMFNormalMap = 2,
	kMFHeightMap = 4,
	kMFEmissiveMap = 8,
	kMFAmbientMap = 0x10,
	kMFMetallicMap = 0x20,
	kMFRoughnessMap = 0x40,
	kMFInstancedAO = 0x80,
	kMFInstancedAlbedo = 0x100,
	kMFSkinned = 0x200,
	kMFFog = 0x400,
	kMFTransparent = 0x800,
	kMFMaxFlags = 0xffff,
	kMFPhysicallyBased = 0x80000000,			//CLR - Using MSB of material flags? Should this be a flag?
}rgfx_materialFlagBits;

typedef uint16_t rgfx_materialFlags;
typedef struct rgfx_instanceBatchInfo
{
	DrawElementsIndirectCommand command; //size = 5 * sizeof(uint32_t)
	rgfx_buffer matricesBuffer;
	rgfx_buffer materialsBuffer;
	rgfx_buffer skinningBuffer;
	rgfx_texture albedoMap;
	rgfx_texture normalMap;
	rgfx_texture metallicMap;
	rgfx_texture roughnessMap;
	rgfx_texture emissiveMap;
	rgfx_texture aoMap;
	float albedo[4];
	float roughness;
	float averageZ;
	rgfx_materialFlags materialFlags;
	uint32_t materialNameHash;
}rgfx_instanceBatchInfo;

 
typedef struct rgfx_light
{
	vec4 position;
	vec4 color;
}rgfx_light;

typedef enum rgfx_passFlagBits
{
	kPFNeedsResolve = 1,
}rgfx_passFlagsBits;

typedef uint32_t rgfx_passFlags;

typedef struct rgfx_passInfo
{
	GLuint frameBufferId;
	int32_t width;
	int32_t height;
	rgfx_passFlags flags;
}rgfx_passInfo;

typedef struct rgfx_pipelineInfo
{
	GLuint name;
	GLuint vertexProgramName;
	GLuint fragmentProgramName;
}rgfx_pipelineInfo;

typedef enum rgfx_rtFlagBits
{
	kRTMultiSampledBit = 1,
}rgfx_rtFlagsBits;

typedef uint32_t rgfx_rtFlags;
#ifdef VRSYSTEM_OCULUS_MOBILE
typedef struct ovrApp ovrApp;
typedef struct ovrTracking2_ ovrTracking2;
typedef struct ovrTextureSwapChain ovrTextureSwapChain;
#endif

typedef struct rgfx_eyeFbInfo
{
	int32_t width;
	int32_t	height;
	int32_t	multisamples;
	int32_t	textureSwapChainLength;
	int32_t	textureSwapChainIndex;
	bool useMultiview;
//#ifdef VRSYSTEM_OCULUS_MOBILE
	rvrs_textureSwapChain colorTextureSwapChain;
//#endif
	GLuint  *depthBuffers;
	GLuint  *colourBuffers;
} rgfx_eyeFbInfo;

typedef struct rgfx_rtInfo
{
//	int32_t width;
//	int32_t height;
//	rgfx_format format;
	GLuint name;						// 4 bytes
	GLenum target;
	int32_t width;
	int32_t height;
	rgfx_rtFlags flags;
}rgfx_rtInfo;

typedef struct rgfx_shaderInfo
{
	GLuint name;
	GLenum type;
}rgfx_shaderInfo;

typedef struct rgfx_textureInfo
{
	rgfx_resourceHeader header;			// 4 bytes
	GLuint name;
	GLenum target;
	GLenum internalFormat;
	int32_t width;
	int32_t height;
	int32_t depth;
}rgfx_textureInfo;

typedef struct rgfx_cameraTransform
{
	mat4x4	viewProjectionMatrix;
	vec4	position;
}rgfx_cameraTransform;

typedef struct rgfx_cameraParams
{
	rgfx_cameraTransform camera[2]; 
	vec4				 fogPlane;
}rgfx_cameraParams;

typedef struct rgfx_cameraInfo
{
	mat4x4 viewMatrix;
	mat4x4 projectionMatrix;
	mat4x4 viewProjectionMatrix;
	vec4 position;
}rgfx_cameraInfo;

typedef struct rgfx_gameCameraDesc
{
	mat4x4 worldMatrix;
	mat4x4 viewMatrix;
}rgfx_gameCameraDesc;

typedef struct rgfx_cameraDesc
{
	struct {
		mat4x4 viewMatrix;
		mat4x4 projectionMatrix;
		vec4 position;
	} camera[MAX_CAMERAS];
	int32_t count;
}rgfx_cameraDesc;

typedef struct MaterialProperties
{
	float diffuse[4];
	float specular[3];
	float specularPower;
}MaterialProperties;

typedef struct rgfx_fogParams
{
	vec4 fogPlaneOS;
	vec4 camPosOS;
}rgfx_fogParams;

typedef enum rgfx_dataType
{
	kTypeInvalid = 0,
	kUnsignedInt,
	kUnsignedShort,
	kUnsignedByte,
	kUnsignedInt8888,
	kFloat,
	kHalf,
	kInt2_10_10_10_Rev,
	kSignedInt,
	kSignedShort,
	kSignedByte,
	kDataTypeMax = 0x7fff
}rgfx_dataType;

typedef enum rgfx_shaderType
{
	kUnknown,
	kVertexShader,
	kFragmentShader,
}rgfx_shaderType;

typedef enum rgfx_action 
{
//	_SG_ACTION_DEFAULT,
	kActionNone,
	kActionClear,
	kActionLoad,
	kAcationDontCare,
	kActiomNum,
//	_SG_ACTION_FORCE_U32 = 0x7FFFFFFF
}rgfx_action;

typedef enum rgfx_format
{
	kFormatInvalid = 0,
	kFormatARGB8888,
	kFormatSRGBA8,
	kFormatAlpha,
//	kFormatPVRTC4,
//	kFormatPVRTC2,
	kFormatD24S8,
	kFormatD32F,
	kFormatRGBDXT1,
	kFormatRGBADXT1,
	kFormatRGBADXT3,
	kFormatRGBADXT5,
	kFormatSRGBDXT1,
	kFormatSRGBDXT3,
	kFormatSRGBDXT5,
	kFormatSRGBASTC4x4,
	kFormatRGBAASTC4x4,
	kFormatSRGBASTC6x6,
	kFormatRGBAASTC6x6,
}rgfx_format;

typedef enum rgfx_filter 
{
//	_SG_FILTER_DEFAULT, /* value 0 reserved for default-init */
	kFilterNearest,
	kFilterLinear,
	kFilterNearestMipmapNearest,
	kFilterNearestMipmapLinear,
	kFilterLinearMipmapNearest,
	kFilterLinearMipmapLinear,
	kFilterNum,
//	_SG_FILTER_FORCE_U32 = 0x7FFFFFFF
}rgfx_filter;

typedef enum rgfx_wrapMode
{
	kWrapModeDefault,
	kWrapModeClamp,
	kWrapModeRepeat,
}rgfx_wrapMode;

typedef enum rgfx_imageType
{
	//	_SG_FILTER_DEFAULT, /* value 0 reserved for default-init */
	kTextureInvalid = 0,
	kTexture2D,
	kTexture2DArray,
	kTexture3D,
	kTextureCube,
	kTextureCubeArray
	//	_SG_FILTER_FORCE_U32 = 0x7FFFFFFF
}rgfx_imageType;

typedef enum rgfx_primType
{
	kPrimTypePoints = 0,					// CLR: Default Buffer Type?
	kPrimTypeLines,
	kPrimTypeLineStrip,
	kPrimTypeTriangles,
	kPrimTypeTriangleStrip,
	kPrimTypeTriangleFan,
}rgfx_primType;

typedef struct rgfx_vertexElement			// 4 bytes
{
	//	uint8_t			m_index;
	//	int8_t			m_size;
	//	uint16_t		m_type;
	//	uint8_t			m_normalized;
	//	uint8_t			m_offset;
	uint16_t		m_type;
	int8_t			m_size;
	uint8_t			m_normalized;
}rgfx_vertexElement;

typedef struct rgfx_vertexFormatInfo
{
	rgfx_vertexElement elements[MAX_VERTEX_ELEMENTS];
	int32_t numElements;
	int32_t stride;
	uint32_t hash;
	uint32_t pad;
}rgfx_vertexFormatInfo;

typedef struct rgfx_colorAttachmentAction
{
	rgfx_action action;
	float val[4];
}rgfx_colorAttachmentAction;

typedef struct rgfx_depthAttachmentAction 
{
	rgfx_action action;
	float val;
}rgfx_depthAttachmentAction;

typedef struct rgfx_stencilAttachmentAction {
	rgfx_action action;
	uint8_t val;
} rgfx_stencilAttachmentAction;

typedef struct rgfx_passAction
{
	uint32_t _start_canary;
	rgfx_colorAttachmentAction colors[2];
	rgfx_depthAttachmentAction depth;
//	rgfx_stencil_attachment_action stencil;
	uint32_t _end_canary;
}rgfx_passAction;

typedef struct {
	rgfx_renderTarget rt;
//	int mip_level;
//	int slice;
} rgfx_attachment_t;

typedef struct
{
//	_sg_slot_t slot;
	int32_t numColorAttachments;
	rgfx_attachment_t colorAttachments[2];
	rgfx_attachment_t depthAttachment;
}rgfx_pass_t;

typedef struct rgfx_extensions
{
	bool multi_view;					// GL_OVR_multiview, GL_OVR_multiview2
	bool EXT_texture_border_clamp;		// GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
	bool EXT_texture_filter_anisotropic;
} rgfx_extensions;

typedef struct rgfx_initParams
{
	int32_t width;
	int32_t height;
	int32_t eyeWidth;
	int32_t eyeHeight;
	rgfx_extensions extensions;
}rgfx_initParams;

typedef struct rgfx_lightUpdateDesc
{
	vec4 position;
	vec4 color;
	float colorScalar;
}rgfx_lightUpdateDesc;

typedef struct rgfx_meshDesc
{
	uint32_t meshHash;
	uint32_t numRenderables;
	rgfx_renderable firstRenderable;
	float aabbMin[3];
	float aabbMax[3];
}rgfx_meshDesc;

typedef struct rgfx_meshInstanceDesc
{
	const char *instanceName;
	const char *meshName;
	uint32_t meshHash;
	rgfx_mesh mesh;
	union
	{
		mat4x4 transform;
		struct
		{
			struct 
			{
				vec4 xAxis;
				vec4 yAxis;
				vec4 zAxis;
			} rotation;
			vec4 position;
		};
	};
	uint32_t animationFrame;
	struct 
	{
		uint32_t changeMaterialFlags;
		uint32_t changeMaterialNameHash;
		rgfx_texture changeMaterialTexture;
		uint32_t newMaterialFlags;
	};
}rgfx_meshInstanceDesc;

typedef struct rgfx_meshInfo
{
	vec4 aabbMin;
	vec4 aabbMax;
	uint32_t meshNameHash;
//	uint32_t	numNodes;				// CLR - Do we need both numNodes and numRenderasbles
	int32_t	numRenderables;
//	uint32_t	numBones;
//	uint32_t	numFrames;
	rgfx_renderable firstRenderable;
	uint32_t pad;
//	MeshNode* hierarchy;
//	mat4x4* transforms;
//	Renderable* renderables;
//	rsys_hash	materialLookup;
//	Material* materials;
//	rsys_hash	boneLookup;
//	mat4x4* bones;
//	mat4x4* inverseBindBose;
}rgfx_meshInfo;

typedef struct rgfx_meshInstanceInfo
{
	mat4x4 instanceMatrix;
	MaterialProperties material;		// CLR - Do we need to store a material here.  Maybe a Material index?
	rgfx_mesh mesh;
	uint32_t meshNameHash;
	float averageZ;
	uint32_t flags;
}rgfx_meshInstanceInfo;

typedef struct Mesh Mesh;				// CLR - Very very temporary, just to lookup material for now.

typedef struct rgfx_renderableInfo
{
	uint32_t	firstVertex;
	uint32_t	firstIndex;
	uint32_t	indexCount;
	int32_t		materialIndex;			// Should probably be a material hash.. Need to consider.
	rgfx_pipeline pipeline;
	rgfx_vertexBuffer vertexBuffer;
	Mesh*		mesh;					// Very temporary, just to lookup material for now.  Please remove... Please...
} rgfx_renderableInfo;

typedef struct rgfx_bufferDesc			// 24 bytes
{
	rgfx_bufferType	type;				// 4 bytes
	uint32_t		capacity;			// 4 bytes
	uint32_t		stride;				// 4 bytes
	uint32_t		flags;				// 4 bytes
	const void*		data;				// 8 bytes
}rgfx_bufferDesc;

typedef struct rgfx_attachmentDesc 
{
	rgfx_renderTarget rt;
/*
	int mip_level;
	union {
		int face;
		int layer;
		int slice;
	};
*/
} rgfx_attachmentDesc;

typedef struct rgfx_passDesc
{
	rgfx_attachmentDesc colorAttachments[2];
	rgfx_attachmentDesc depthAttachment;
}rgfx_passDesc;

typedef struct rgfx_pipelineDesc
{
	rgfx_shader vertexShader;
	rgfx_shader fragmentShader;
}rgfx_pipelineDesc;

typedef struct rgfx_renderTargetDesc	//28 bytes
{
	int32_t		width;					// 4 bytes
	int32_t		height;					// 4 bytes
	uint32_t	texture;
	uint32_t	target;
	rgfx_format format;					// 4 bytes
	rgfx_filter	minFilter;				// 4 bytes
	rgfx_filter magFilter;				// 4 bytes
	int8_t		sampleCount;			// 1 byte
	bool		stereo;					// 1 byte
	int16_t		pad;					// 2 bytes
}rgfx_renderTargetDesc;

typedef struct rgfx_renderStats
{
	uint64_t cullUSecs;
	uint64_t sortUSecs;
	uint64_t sceneUSecs;
	uint64_t renderUSecs;
	uint64_t gpuUSecs;
}rgfx_renderStats;

typedef struct rgfx_textureDesc			//32 bytes
{
	int32_t			width;				// 4 bytes
	int32_t			height;				// 4 bytes
	int32_t			depth;				// 4 bytes
	int32_t			mipCount;			// 4 bytes
	rgfx_format		format;				// 4 bytes
	rgfx_filter		minFilter;			// 4 bytes
	rgfx_filter		magFilter;			// 4 bytes
	rgfx_wrapMode	wrapS;				// 4 bytes
	rgfx_wrapMode	wrapT;				// 4 bytes
	float			anisoFactor;		// 4 bytes
	rgfx_imageType	type;				// 4 bytes
//	int8_t		sampleCount;			// 1 byte
//	bool		stereo;					// 1 byte
}rgfx_textureDesc;

typedef struct rgfx_textureLoadDesc
{
	rgfx_texture texture;
	int32_t slice;
}rgfx_textureLoadDesc;

typedef struct rgfx_vertexBufferDesc
{
	rgfx_vertexFormat format;
	uint32_t capacity;					// 4 bytes
	rgfx_buffer	indexBuffer;
//	uint32_t stride;					// 4 bytes
//	uint32_t flags;						// 4 bytes
}rgfx_vertexBufferDesc;

typedef struct rgfx_vertexBufferInfo
{
	rgfx_resourceHeader header;
	rgfx_vertexFormat format;
	rgfx_buffer vertexBuffer;
	rgfx_buffer indexBuffer;
	GLuint vao;					// CLR - Should this be removed and kept separate (private) as not relevant for Vulkan etc.
}rgfx_vertexBufferInfo;

typedef struct rgfx_batchHashmap { char* key; int32_t value; } rgfx_batchHashmap;

typedef enum rgfx_resourceType
{
	kResourceUnknown = 0,
	kResourceBuffer,
	kResourceTexture,
	kResourceVertexBuffer,
	kResourceNum
}rgfx_resourceType;

typedef struct rgfx_resource {
	void* resourceBuffer;
	int32_t elementSize;
	int32_t capacity;
	int32_t count;
	int32_t freeList;
}rgfx_resource;

typedef struct rgfx_rendererData
{
	rgfx_resource resourceInfo[kResourceNum];
	rgfx_vertexBufferInfo vertexBufferInfo[MAX_VERTEX_BUFFERS];
//	rgfx_bufferInfo ibInfo[MAX_VERTEX_BUFFERS];
	rgfx_bufferInfo buffers[MAX_BUFFERS];
	rgfx_eyeFbInfo eyeFbInfo[MAX_EYE_BUFFERS];
	rgfx_rtInfo rtInfo[MAX_RENDER_TARGETS];
	rgfx_passInfo  passes[MAX_RENDER_PASSES];
	rgfx_pipelineInfo pipelines[MAX_PIPELINES];
	rgfx_shaderInfo shaders[MAX_SHADERS];
	rgfx_textureInfo textures[MAX_TEXTURES];
	rgfx_vertexFormatInfo vertexFormats[MAX_VERTEX_FORMATS];
	rgfx_cameraInfo cameras[MAX_CAMERAS];
	rgfx_gameCameraDesc gameCamera;
	rgfx_passInfo currentPassInfo;
	rsys_hash shaderLookup;
	rsys_hash meshInstancesLookup;
	rsys_hash vertexBufferLookup;
	rsys_hash vertexFormatLookup;
	rgfx_buffer cameraTransforms;
	rgfx_buffer lights;
//	rgfx_buffer	instanceDrawCandidates;
//	rgfx_buffer	instanceMatrices;
//	rgfx_buffer	instanceMaterials;
//	rgfx_buffer	visibleInstanceDrawCommands;
//	rgfx_buffer	visibleInstanceMatrices;
	rgfx_buffer	parameterBuffer;
	rgfx_pipeline fsQuadPipeline;
	rgfx_extensions extensions;
	int32_t numVertexFormats;
	int32_t numEyeBuffers;
	int32_t numRenderTargets;
	int32_t numPasses;
	int32_t numPipelines;
	int32_t numShaders;
	int32_t numMeshInstances;
	int32_t lightCount;
	int32_t uniformBufferAlignSize;
}rgfx_rendererData;

void rgfx_initialize(const rgfx_initParams *params);
void rgfx_terminate();
void rgfx_initializeExtensions(const rgfx_extensions extensions);
void rgfx_initializeScene(void);
void rgfx_initPostEffects(void);
void rgfx_initializeResourceManager(void);

int32_t rgfx_allocResource(rgfx_resourceType type);
void rgfx_freeResource(rgfx_resourceType type, int32_t idx);

rgfx_vertexFormat rgfx_registerVertexFormat(int32_t numVertexElements, rgfx_vertexElement* vertexElements);
rgfx_vertexFormatInfo* rgfx_getVertexFormatInfo(rgfx_vertexFormat format);
rgfx_bufferInfo *rgfx_getVertexBufferInfo(rgfx_vertexBuffer vb);
rgfx_buffer rgfx_getVertexBufferBuffer(rgfx_vertexBuffer vb, rgfx_bufferType type);
uint32_t rgfx_writeVertexData(rgfx_vertexBuffer vb, size_t sizeInBytes, uint8_t* vertexData);
void rgfx_bindVertexBuffer(rgfx_vertexBuffer vb);
void rgfx_unbindVertexBuffer();

rgfx_bufferInfo* rgfx_getIndexBufferInfo(rgfx_vertexBuffer vbIdx);
uint32_t rgfx_writeIndexData(rgfx_vertexBuffer vb, size_t sizeInBytes, uint8_t* indexData, size_t indexSize);
uint32_t rgfx_bindIndexBuffer(rgfx_buffer vbIdx);
void rgfx_writeCompressedMipImageData(rgfx_texture tex, int32_t mipLevel, rgfx_format format, int32_t width, int32_t height, int32_t imageSize, const void *data);
void rgfx_writeCompressedTexSubImage3D(rgfx_texture tex, int32_t miplevel, int32_t xoffset, int32_t yoffset, int32_t zoffset, int32_t width, int32_t height, int32_t depth, rgfx_format format, int32_t imageSizemipSize, const void* data);

void rgfx_setGameCamera(const rgfx_gameCameraDesc* gameCameraDesc);
void rgfx_updateTransforms(const rgfx_cameraDesc *cameraDesc);
rgfx_buffer rgfx_getTransforms();
rgfx_buffer rgfx_getLightsBuffer();
mat4x4 rgfx_getCameraViewMatrix(int32_t cam);
mat4x4 rgfx_getCameraProjectionMatrix(int32_t cam);
mat4x4 rgfx_getCameraViewProjMatrix(int32_t cam);

rgfx_buffer rgfx_createBuffer(const rgfx_bufferDesc *desc);
void* rgfx_mapBuffer(rgfx_buffer buffer);
void* rgfx_mapBufferRange(rgfx_buffer buffer, int64_t offset, int64_t size);
void rgfx_unmapBuffer(rgfx_buffer buffer);
void rgfx_bindBuffer(int32_t index, rgfx_buffer buffer);
void rgfx_bindBufferRange(int32_t index, rgfx_buffer buffer, int64_t offset, int64_t size);

rgfx_bufferInfo *rgfx_getBufferInfo(rgfx_buffer buffer);

rgfx_vertexBuffer rgfx_createVertexBuffer(const rgfx_vertexBufferDesc* desc);

void rgfx_createEyeFrameBuffers(int32_t idx, int32_t width, int32_t height, rgfx_format format, int32_t sampleCount, bool useMultiview);
void rgfx_destroyEyeFrameBuffers();
void rgfx_scaleEyeFrameBuffers(int32_t newScale);

void rgfx_bindEyeFrameBuffer(int32_t eye);

rgfx_renderTarget rgfx_createRenderTarget(const rgfx_renderTargetDesc *desc);
rgfx_pass rgfx_createPass(const rgfx_passDesc *desc);
rgfx_pipeline rgfx_createPipeline(const rgfx_pipelineDesc *desc);
rgfx_texture rgfx_createTexture(const rgfx_textureDesc* desc);
void rgfx_destroyTexture(rgfx_texture tex);
rgfx_texture rgfx_createTextureArray(const char** textureNames, int32_t numTextures);
void rgfx_bindTexture(int32_t slot, rgfx_texture tex);

rgfx_eyeFbInfo* rgfx_getEyeFrameBuffer(int32_t eye);

rgfx_shader rgfx_loadShader(const char *shaderName, rgfx_shaderType type, rgfx_materialFlags flags);

void rgfx_addLight(vec4 position, vec4 color);
rgfx_mesh rgfx_addMesh(const rgfx_meshDesc* meshDesc);
void rgfx_popMesh(int32_t count);
rgfx_mesh rgfx_findMesh(uint32_t meshHash);
rgfx_meshInstance rgfx_findMeshInstance(uint32_t meshHash);

rgfx_meshInstance rgfx_addMeshInstanceFromS3D(const char* instanceName, uint32_t meshHash, mat4x4* worldMatrix);
rgfx_meshInstance rgfx_addMeshInstance(const rgfx_meshInstanceDesc *meshInstance);
rgfx_renderable rgfx_addRenderable(const rgfx_renderableInfo* renderableDesc);
void rgfx_updateLight(int32_t lightIdx, const rgfx_lightUpdateDesc* lightUpdateDesc);
void rgfx_updateMeshInstance(rgfx_meshInstance instance, const rgfx_meshInstanceDesc* meshInstanceDesc);
void rgfx_updateMeshMaterial(uint32_t meshHash, const rgfx_meshInstanceDesc* desc); //CLR - Should probably use a materialDesc or something.

void rgfx_removeAllMeshInstances();

void rgfx_beginDefaultPass(const rgfx_passAction *passAction, int width, int height);
void rgfx_beginPass(rgfx_pass, const rgfx_passAction *passAction);
void rgfx_endPass(bool bScreenshotRequested);

void rgfx_bindPipeline(rgfx_pipeline pipeline);
uint32_t rgfx_getPipelineProgram(rgfx_pipeline pipeline, rgfx_shaderType type);

mat4x4 *rgfx_mapModelMatricesBuffer(int32_t start, int32_t end);
void rgfx_unmapModelMatricesBuffer();
void rgfx_renderWorld(bool bScreenshotRequested);
rgfx_renderStats rgfx_getRenderStats(void);

void rgfx_drawIndexedPrims(rgfx_primType type, int32_t indexCount, int32_t firstIndex);
void rgfx_drawRenderableInstanced(rgfx_renderable rend, int32_t instanceCount);
void rgfx_renderFullscreenQuad(rgfx_texture tex);
//void rgfx_drawDebugLine(vec4 p1, vec4 p2);
//void rgfx_drawDebugLines();

typedef struct rgfxApi
{
	rgfx_mesh (*findMesh)(uint32_t meshHash);
	rgfx_meshInstance (*findMeshInstance)(uint32_t meshHash);

	rgfx_meshInstance(*addMeshInstance)(const rgfx_meshInstanceDesc* meshInstance);
	void (*updateLight)(int32_t lightIdx, const rgfx_lightUpdateDesc* lightUpdateDesc);
	void (*updateMeshInstance)(rgfx_meshInstance instance, const rgfx_meshInstanceDesc* desc);
	void (*updateMeshMaterial)(uint32_t meshHash, const rgfx_meshInstanceDesc* desc); //CLR - Should probably use a materialDesc or something.
	void (*removeAllMeshInstances)();

	mat4x4 (*getCameraViewMatrix)(int32_t cam);
	void (*setGameCamera)(const rgfx_gameCameraDesc* gameCameraDesc);
	void (*updateTransforms)(const rgfx_cameraDesc* cameras);
	struct mat4x4* (*mapModelMatricesBuffer)(int32_t start, int32_t end);
	void (*unmapModelMatricesBuffer)();

	rgfx_buffer(*createBuffer)(const rgfx_bufferDesc* desc);
	void* (*mapBuffer)(rgfx_buffer buffer);
	void (*unmapBuffer)(rgfx_buffer buf);
	rgfx_bufferInfo* (*getBufferInfo)(rgfx_buffer buffer);

	rgfx_renderTarget(*createRenderTarget)(const rgfx_renderTargetDesc* desc);
	rgfx_texture(*createTextureArray)(const char** textureNames, int32_t numTextures);
	void (*scaleEyeFrameBuffers)(int32_t newScale);

	rgfx_pass(*createPass)(const rgfx_passDesc* desc);
	void (*beginDefaultPass)(const rgfx_passAction* passAction, int width, int height);
	void (*beginPass)(rgfx_pass pass, const rgfx_passAction* passAction);
	void (*endPass)(bool bScreenshotRequested);

	rgfx_pipeline(*createPipeline)(const rgfx_pipelineDesc* desc);
	void (*bindPipeline)(rgfx_pipeline pipeline);
	void (*renderWorld)(bool bScreenshotRequested);
	rgfx_renderStats (*getRenderStats)(void);
	rgfx_buffer (*getLightsBuffer)(void);

	rgfx_shader(*loadShader)(const char* shaderName, rgfx_shaderType shaderType, rgfx_materialFlags flags);

	void (*initPostEffects)(void);
	void (*renderFullscreenQuad)(rgfx_texture tex);
	void (*debugRenderInitialize)(const rgfx_debugRenderInitParams* params);
	void (*drawDebugLine)(vec3 p1, rgfx_debugColor col1, vec3 p2, rgfx_debugColor col2);
	void (*drawDebugAABB)(vec3 aabbMin, vec3 aabbMax);
}rgfxApi;
