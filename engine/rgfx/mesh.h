#pragma once
#include "renderer.h"
#include "math/mat4x4.h"

#define MAX_VERTEX_ELEMENTS (16)
#define MAX_STREAM_COUNT (2)

// Public structures.

typedef struct ChunkId
{
	uint32_t fourCC;
	uint32_t size;
}ChunkId;

struct ModelHeader
{
	ChunkId		chunkId;
	uint32_t	versionMajor;
	uint32_t	versionMinor;
};

struct RenderableInfo
{
	uint32_t	numVertices;
	uint32_t	numIndices;
	uint32_t	materialHash;
	float		aabbMinX;
	float		aabbMinY;
	float		aabbMinZ;
	float		aabbMaxX;
	float		aabbMaxY;
	float		aabbMaxZ;
	uint32_t	verticesOffset;
	uint32_t	indicesOffset;
};

struct MeshInfo
{
	float		worldMatrix[16];	//CLR - Changed to array of float instead of mat4x4/vec4 due to Clang alignment packing?
	float		aabbMinX;
	float		aabbMinY;
	float		aabbMinZ;
	float		aabbMaxX;
	float		aabbMaxY;
	float		aabbMaxZ;
	uint32_t	numRenderables;
	uint32_t	numChildren;
};

struct MeshChunk
{
	ChunkId		chunkId;
	uint32_t	numMeshes;
};

struct TextureInfo
{
	uint32_t	width;
	uint32_t	height;
	uint32_t	textureOffset;
};

struct TextureChunk
{
	ChunkId		chunkId;
	uint32_t	numTextures;
};

struct MaterialInfo
{
	float		diffuse[3];
	float		ambient[3];
	float		specular[4];
	float		alpha;
	uint32_t	flags;
	uint32_t	emissiveMapHash;
	uint32_t	diffuseMapHash;
	uint32_t	normalMapHash;
	uint32_t	heightMapHash;
};

struct PBRMaterialInfoV3
{
	uint32_t	flags;
	uint32_t	diffuse;
	uint32_t	diffuseMapHash;
	uint32_t	normalMapHash;
	uint32_t	heightMapHash;
	union {
		uint32_t	metallicMapHash;
		float		metallic;
	};
	union {
		uint32_t	roughnessMapHash;
		float		roughness;
	};
	union {
		uint32_t	emissiveMapHash;
		uint32_t	emissive;
	};
	uint32_t	aoMapHash;
};

struct MaterialInfoV3
{
	uint32_t	flags;
	float		diffuse[3];
	float		alpha;
	float		ambient[3];
	float		specular[4];
	uint32_t	emissiveMapHash;
	uint32_t	diffuseMapHash;
	uint32_t	normalMapHash;
	uint32_t	heightMapHash;
};

struct MaterialInfoV2
{
	float		diffuse[3];
	float		ambient[3];
	float		specular[4];
	float		alpha;
	uint32_t	flags;
	uint32_t	emissiveMapHash;
	uint32_t	diffuseMapHash;
	uint32_t	normalMapHash;
	uint32_t	heightMapHash;
};

struct MaterialInfoV1
{
	float		diffuse[3];
	float		ambient[3];
	float		specular[4];
	float		alpha;
	uint32_t	flags;
	float		reflectVal;
	uint32_t	diffuseMapHash;
	uint32_t	normalMapHash;
};

struct MaterialChunk
{
	ChunkId		chunkId;
	uint32_t	numMaterials;
};

struct SkeletonChunk
{
	ChunkId		chunkId;
	uint32_t	numBones;
};

struct AnimationChunk
{
	ChunkId chunkId;
	uint32_t numTakes;
};

struct TakeInfo
{
	uint32_t numFrames;
	float startTime;
	float endTime;
	float frameRate;
};

struct __attribute__((packed)) LightInfo
{
	uint32_t		m_nameHash;
	float			m_transform[16]; //CLR - Changed to array of float instead of mat4x4/vec4 due to Clang alignment packing?
	float			m_color[4];
};

struct LightChunk
{
	ChunkId chunkId;
	uint32_t numLights;
};

struct MeshInstanceInfo
{
	uint32_t	meshHash;
	float		worldMatrix[16]; //CLR - As above, using an array of floats instead of mat4x4/vec4 due to Clang alignment packing?
};

struct MeshInstanceChunk
{
	ChunkId chunkId;
	uint32_t numMeshInstances;
};

struct Import_VertexElement
{
	uint8_t			m_index;
	int8_t			m_size;
	uint16_t		m_type;
	uint8_t			m_normalized;
	uint8_t			m_offset;			//CLR - is this big enough for MultiDrawIndirect?
};

struct Import_VertexStream
{
	uint32_t				m_glBufferId;
	uint8_t					m_bufferType;
	uint8_t					m_numElements;
	uint16_t				m_stride;
	uint32_t				m_dataOffset;
	struct Import_VertexElement	m_elements[MAX_VERTEX_ELEMENTS];
};

struct Import_VertexBuffer
{
	uint32_t			m_numStreams;
	struct Import_VertexStream	m_streams[MAX_STREAM_COUNT];
};


typedef struct MeshNode
{
	int32_t	 parentId;
	uint32_t numRenderables;
	uint32_t numChildren;
} MeshNode;

typedef struct Renderable
{
	uint32_t	firstVertex;
	uint32_t	firstIndex;
	uint32_t	indexCount;
	int32_t		materialIndex;
} Renderable;

typedef struct Material
{
	uint32_t			nameHash;
	float				albedo[4];
	float				metallic;
	float				roughness;
	rgfx_texture		albedoMap;
	rgfx_texture		normalMap;
	rgfx_texture		metallicMap;
	rgfx_texture		roughnessMap;
	rgfx_texture		emissiveMap;
	rgfx_texture		ambientMap;
	rgfx_materialFlags	flags;
}Material;

typedef struct Mesh
{
	float		aabbMin[3];
	float		aabbMax[3];
	uint32_t	numNodes;
	uint32_t	numRenderables;
	uint32_t	numBones;
	uint32_t	numFrames;
	int32_t		versionMajor;
	int32_t		versionMinor;

	rsys_hash	materialLookup;
	rsys_hash	boneLookup;

	MeshNode	*hierarchy;
	mat4x4		*transforms;
	Renderable	*renderables;
	Material	*materials;
	mat4x4		*bones;
	mat4x4		*inverseBindBose;
} Mesh;

// Public function prototypes.
void mesh_load(const char* meshName, Mesh* mesh);
