#include "stdafx.h"
#include "math/mat4x4.h"
#include "resource.h"
#include "mesh.h"
#include "file.h"
#include "renderer.h"
#include "assert.h"
#include "hash.h"
#include "stb/stretchy_buffer.h"

#ifndef MAX_PATH
#define MAX_PATH (260)
#endif

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

static int32_t kVersionMajor = 0;
static int32_t kVersionMinor = 3;
static const uint32_t kFourCC_SCNE = ION_MAKEFOURCC('S', 'C', 'N', 'E');	// Scene Chunk FourCC
static const uint32_t kFourCC_TEXT = ION_MAKEFOURCC('T', 'E', 'X', 'T');	// Texture Chunk FourCC
static const uint32_t kFourCC_MATL = ION_MAKEFOURCC('M', 'A', 'T', 'L');	// Material Chunk FourCC
static const uint32_t kFourCC_SKEL = ION_MAKEFOURCC('S', 'K', 'E', 'L');	// Skeleton Chunk FourCC
static const uint32_t kFourCC_LGHT = ION_MAKEFOURCC('L', 'G', 'H', 'T');	// Light Chunk FourCC
static const uint32_t kFourCC_ANIM = ION_MAKEFOURCC('A', 'N', 'I', 'M');	// Anim Chunk FourCC
static const uint32_t kFourCC_MESH = ION_MAKEFOURCC('M', 'E', 'S', 'H');	// Mesh Chunk FourCC
static const uint32_t kFourCC_INST = ION_MAKEFOURCC('I', 'N', 'S', 'T');	// Instance Chunk FourCC
static const uint32_t kFourCC_ENDF = ION_MAKEFOURCC('E', 'N', 'D', 'F');	// End File Chunk FourCC

static const char* s_currentMeshName = NULL;
// Private structures.
/*
typedef struct ChunkId
{
	uint32_t fourCC;
	uint32_t size;
}ChunkId;
*/

// Private function prototypes.
void loadTextureChunk(struct ChunkId *chunk);
void loadMaterialChunk(struct ChunkId *chunk, struct Mesh *mesh);
void loadSkeletonChunk(struct ChunkId* chunk, struct Mesh* mesh);
void loadAnimationChunk(struct ChunkId* chunk, struct Mesh* mesh);
void loadLightChunk(struct ChunkId *chunk);
int32_t loadMeshChunk(struct ChunkId *chunk, struct Mesh *mesh);
uint8_t *loadMeshChunkRecursive(uint8_t* ptr, struct Mesh *mesh, uint32_t *renderableIndex, uint32_t *nodeIndex, int32_t *parentIndex); //, VertexBuffer& vertexBuffer, int64_t& vertexBufferOffset, IndexBuffer& indexBuffer, int64_t& indexBufferOffset)
void loadMeshInstanceChunk(struct ChunkId *chunk, struct Mesh *mesh);

void mesh_load(const char *meshName, Mesh *mesh)
{
	s_currentMeshName = meshName;
	char filename[MAX_PATH];
#ifdef RE_PLATFORM_WIN64
	strcpy(filename, "assets/Models/");
	strcat(filename, meshName);
#else
	strcpy(filename, "Models/");
	strcat(filename, meshName);
#endif

	rsys_file file = file_open(filename, "rb");
	if (!file_is_open(file))
	{
		printf("Error: Unable to open file %s\n", meshName);
		exit(1);
	}

	size_t fileLen = file_length(file);
	uint8_t *buffer = malloc(fileLen);
	memset(buffer, 0, fileLen);
	file_read(buffer, fileLen, file);
	file_close(file);

	struct ModelHeader *header = (struct ModelHeader *)buffer;
	mesh->versionMajor = header->versionMajor;
	mesh->versionMinor = header->versionMinor;
	bool bLoaded = false;
	uint8_t *ptr = (uint8_t *)(buffer + sizeof(struct ModelHeader));
	int32_t res = 0;
	char fourCC[5] = { "NULL" };
	while (!bLoaded)
	{
		struct ChunkId *chunk = (struct ChunkId *)ptr;
		strncpy(fourCC, (char *)(&chunk->fourCC), 4);
		switch (chunk->fourCC)
		{
		case kFourCC_TEXT:
			loadTextureChunk(chunk);
			break;
		case kFourCC_MATL:
			loadMaterialChunk(chunk, mesh);
			break;
		case kFourCC_SKEL:
			loadSkeletonChunk(chunk, mesh);
			break;
		case kFourCC_ANIM:
			loadAnimationChunk(chunk, mesh);
			break;
		case kFourCC_LGHT:
			loadLightChunk(chunk);
			break;
		case kFourCC_MESH:
			res = loadMeshChunk(chunk, mesh);
			if (mesh->versionMajor == 0 && mesh->versionMinor <= 2)
			{
				if (res >= 0)
					bLoaded = true;
				else
				{
					ptr = (uint8_t*)(buffer + sizeof(struct ModelHeader));
					ptr -= chunk->size;
				}
			}
			break;
		case kFourCC_INST:
			loadMeshInstanceChunk(chunk, mesh);
			bLoaded = true;
			break;
		default:
			printf("Warning: Skipping unknown Chunk '%s' in file %s\n", fourCC, meshName);
		}
		ptr = ptr + chunk->size;
	}
	free(buffer);
}

void mesh_unload(Mesh *mesh)
{
	free(mesh->materials);
	free(mesh->renderables);
	free(mesh->transforms);
	free(mesh->hierarchy);
}

void loadTextureChunk(struct ChunkId *chunk)
{
	struct TextureChunk* textureChunk = (struct TextureChunk*)chunk;
	uint32_t numTextures = textureChunk->numTextures;
	uint8_t* ptr = (uint8_t*)chunk + sizeof(struct TextureChunk);

	for (uint32_t i = 0; i < numTextures; ++i)
	{
		uint32_t len = *(uint32_t*)ptr;
		ptr += sizeof(uint32_t);
		char strBuffer[1024];
		strncpy(strBuffer, (char*)ptr, len);
		strBuffer[len] = '\0';
		const char *filename = strBuffer;
		rres_registerResource((rres_resourceDesc) {
			.filename = filename,
			.type = kRTTexture
		});
		uint32_t alignedLen = len + 4 & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes
		ptr += alignedLen;
	}
}

void loadMaterialChunk(struct ChunkId *chunk, Mesh *mesh)
{
	struct MaterialChunk *materialChunk = (struct MaterialChunk*)chunk;
	uint32_t numMaterials = materialChunk->numMaterials;

	assert(numMaterials > 0); //, "Mesh has no materials!");
	//		m_numMaterials = numMaterials;
	//		Material *materialPtr = m_materials = static_cast<Material *>(MemMgr::Instance().alloc(sizeof(Material) * numMaterials));
	memset(&mesh->materialLookup, 0, sizeof(rsys_hash));
	Material *materials = (Material *)malloc(sizeof(Material) * numMaterials);
	uint8_t* ptr = (uint8_t*)chunk + sizeof(struct MaterialChunk);

//	Texture* textures = resourceManager.getTextures();
//	TextureResourceInfo* texInfo = resourceManager.getTextureResources();
	for (uint32_t i = 0; i < numMaterials; ++i) //, materialPtr++)
	{
		uint32_t len = *(uint32_t*)ptr;
		ptr += sizeof(uint32_t);
		char strBuffer[1024];
		strncpy(strBuffer, (char*)ptr, len);
		strBuffer[len] = '\0';
		uint32_t nameHash = hashString(strBuffer);
		rsys_hm_insert(&mesh->materialLookup, nameHash, i);
//		std::string materialName(strBuffer);
		uint32_t alignedLen = len + 4 & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes
		ptr += alignedLen;
		Material *pMaterial = &materials[i];	//ion::MaterialManager::Get()->Find(materialName);
		if (pMaterial)
		{
			if (mesh->versionMajor > 0 || (mesh->versionMajor == 0 && mesh->versionMinor > 2))
			{
				// File version 0.3 or greater.
				uint32_t flags = *(uint32_t*)ptr;
				if (flags & kMFPhysicallyBased)
				{
					struct PBRMaterialInfoV3* info = (struct PBRMaterialInfoV3*)ptr;
					materials[i].nameHash = nameHash;
					materials[i].albedo[0] = (float)((info->diffuse >> 24) & 0xff) * 255.0f;
					materials[i].albedo[1] = (float)((info->diffuse >> 16) & 0xff) * 255.0f;
					materials[i].albedo[2] = (float)((info->diffuse >> 8) & 0xff) * 255.0f;
					materials[i].albedo[3] = (float)((info->diffuse >> 0) & 0xff) * 255.0f;
					materials[i].albedoMap = rres_findTexture(info->diffuseMapHash);
					materials[i].normalMap = rres_findTexture(info->normalMapHash);
					if (info->flags & kMFMetallicMap)
						materials[i].metallicMap = rres_findTexture(info->metallicMapHash);
					else
						materials[i].metallic = info->metallic;
					if (info->flags & kMFRoughnessMap) {
						materials[i].roughnessMap = rres_findTexture(info->roughnessMapHash);
					}
					else {
						materials[i].roughness = max(info->roughness, 0.05f);
					}
					materials[i].emissiveMap = rres_findTexture(info->emissiveMapHash);
					materials[i].ambientMap = rres_findTexture(info->aoMapHash);
					if (materials[i].ambientMap.id |= 0)
					{
						rsys_print("Found AO Material\n");
					}
					materials[i].flags = (uint16_t)(info->flags & 0xffff);
					ptr += sizeof(struct PBRMaterialInfoV3);
				}
				else
				{
					struct MaterialInfoV3* info = (struct MaterialInfoV3*)ptr;
					materials[i].nameHash = nameHash;
					materials[i].albedoMap = rres_findTexture(info->diffuseMapHash);
					materials[i].normalMap = rres_findTexture(info->normalMapHash);
					materials[i].emissiveMap = rres_findTexture(info->emissiveMapHash);
					materials[i].ambientMap.id = 0;
					float specularPower = info->specular[3] > 1.0f ? info->specular[3] : 128.0f * info->specular[3];
					materials[i].albedo[0] = info->diffuse[0]; // powf(info->diffuse[0], 2.2f);
					materials[i].albedo[1] = info->diffuse[1]; // powf(info->diffuse[1], 2.2f);
					materials[i].albedo[2] = info->diffuse[2]; // powf(info->diffuse[2], 2.2f);
					materials[i].albedo[3] = info->alpha;
					materials[i].roughness = max(1.0f - (specularPower / 100.0f), 0.05f);
					materials[i].flags = (uint16_t)(info->flags & 0xffff);
					ptr += sizeof(struct MaterialInfoV3);
				}
			}
			else
			{
				if (mesh->versionMajor == 0 && mesh->versionMinor == 2)
				{
					struct MaterialInfoV2* info = (struct MaterialInfoV2*)ptr;
					materials[i].nameHash = nameHash;
					materials[i].albedoMap = rres_findTexture(info->diffuseMapHash);
					materials[i].normalMap = rres_findTexture(info->normalMapHash);
					materials[i].emissiveMap = rres_findTexture(info->emissiveMapHash);
					float specularPower = info->specular[3] > 1.0f ? info->specular[3] : 128.0f * info->specular[3];
					materials[i].albedo[0] = info->diffuse[0]; // powf(info->diffuse[0], 2.2f);
					materials[i].albedo[1] = info->diffuse[1]; // powf(info->diffuse[1], 2.2f);
					materials[i].albedo[2] = info->diffuse[2]; // powf(info->diffuse[2], 2.2f);
					materials[i].albedo[3] = info->alpha;
					materials[i].roughness = max(1.0f - (specularPower / 100.0f), 0.05f);
					materials[i].flags = (uint16_t)info->flags;
					ptr += sizeof(struct MaterialInfoV2);
				}
				else
				{
					assert(0);	// CLR - Do we need to handle V1 materials still?
				}
			}
/*
			pMaterial = new (materialPtr) Material();
			pMaterial->SetName(materialName);
			pMaterial->m_diffuse[0] = info->diffuse[0];
			pMaterial->m_diffuse[1] = info->diffuse[1];
			pMaterial->m_diffuse[2] = info->diffuse[2];
			pMaterial->m_ambient[0] = info->ambient[0];
			pMaterial->m_ambient[1] = info->ambient[1];
			pMaterial->m_ambient[2] = info->ambient[2];
			pMaterial->m_specular[0] = info->specular[0];
			pMaterial->m_specular[1] = info->specular[1];
			pMaterial->m_specular[2] = info->specular[2];
			pMaterial->m_specular[3] = info->specular[3] > 1.0f ? info->specular[3] : 128.0f * info->specular[3];
			pMaterial->m_alpha = info->alpha;
			pMaterial->m_flags = info->flags;
*/
		}
/*
		if (mesh->versionMajor > 0 || (mesh->versionMajor == 0 && mesh->versionMinor >= 2))
			ptr += sizeof(struct MaterialInfo);
		else
			ptr += (sizeof(struct MaterialInfo)+sizeof(uint32_t));
*/
	}
	mesh->materials = materials;
}

void loadSkeletonChunk(struct ChunkId *chunk, Mesh *mesh)
{
	struct SkeletonChunk* skeletonChunk = (struct SkeletonChunk*)chunk;
	uint32_t numBones = skeletonChunk->numBones;
	mesh->numBones = numBones;
	uint8_t* ptr = (uint8_t*)chunk + sizeof(struct SkeletonChunk);
	for (uint32_t i = 0; i < numBones; ++i)
	{
		uint32_t len = *(uint32_t*)ptr;
		ptr += sizeof(uint32_t);
		char strBuffer[1024];
		strncpy(strBuffer, (char*)ptr, len);
		strBuffer[len] = '\0';
		rsys_hm_insert(&mesh->boneLookup, hashString(strBuffer), i);
		uint32_t alignedLen = len + 4 & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes
		ptr += alignedLen;
		mat4x4 *matrix = (mat4x4 *)ptr;
//		sb_push(mesh->)
//		memcpy(&mesh->bones[i], matrix++, sizeof(mat4x4));
		matrix++;
		sb_push(mesh->inverseBindBose, *matrix++);
//		memcpy(&mesh->inverseBindBose[i], matrix++, sizeof(mat4x4));
		ptr = (uint8_t *)matrix;
	}
}

void loadAnimationChunk(ChunkId *chunk, Mesh *mesh)
{
	struct AnimationChunk* animationChunk = (struct AnimationChunk*)chunk;
	uint32_t numTakes = animationChunk->numTakes;
	struct TakeInfo* info = (struct TakeInfo*)((uint8_t*)chunk + sizeof(struct AnimationChunk));
	mat4x4 *bones = (mat4x4 *)((uint8_t *)info + sizeof(struct TakeInfo) * numTakes);
	uint32_t totalFrames = 0;
	for (uint32_t i = 0; i < numTakes; ++i)
	{
		uint32_t numFrames = info->numFrames;
		(void)numFrames;
		//			for (uint32_t frame = 0; frame < numFrames; ++frame)
		//			{
//		int32_t destIdx = sb_count(mesh->bones);
		mat4x4* destPtr = sb_add(mesh->bones, (int)(mesh->numBones * numFrames));
//		mat4x4* destPtr = mesh->bones + destIdx;
//		memcpy(mesh->bones, bones, sizeof(mat4x4) * mesh->numBones * mesh->numFrames);
		memcpy(destPtr, bones, sizeof(mat4x4) * mesh->numBones * numFrames);
		//				bones = bones + m_numBones;
		//			}
		info++;
		totalFrames += numFrames;
	}
	mesh->numFrames = totalFrames;
}

void loadLightChunk(struct ChunkId *chunk)
{
	struct LightChunk *lightChunk = (struct LightChunk*)chunk;
	uint32_t numLights = lightChunk->numLights;

	uint8_t* ptr = (uint8_t*)chunk + sizeof(struct LightChunk);
	for (uint32_t i = 0; i < numLights; ++i)
	{
		uint32_t len = *(uint32_t*)ptr;
		ptr += sizeof(uint32_t);
		char strBuffer[1024];
		strncpy(strBuffer, (char*)ptr, len);
		strBuffer[len] = '\0';
//		std::string filename(strBuffer);								//CLR - Obviously don't need this, but what was filename for?
		uint32_t alignedLen = len + 4 & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes
		ptr += alignedLen;
		struct LightInfo *info = (struct LightInfo *)ptr;
		(void)info;
		vec4 lightPos = (vec4){ info->m_transform[12], info->m_transform[13], info->m_transform[14], info->m_transform[15] };
#ifdef RE_PLATFORM_ANDROID
		const float kHackScale = 1.0f; // / 23.5f;
#else
		const float kHackScale = 1.0f; // / 23.5f;
#endif
		vec4 lightCol = (vec4){ info->m_color[0] * kHackScale, info->m_color[1] * kHackScale, info->m_color[2] * kHackScale, info->m_color[3] };
		rgfx_addLight(lightPos, lightCol);
		ptr += sizeof(struct LightInfo);
	}
}

uint8_t* calcMeshSizeRecursive(uint8_t *ptr, uint32_t *renderableCount, struct Mesh *mesh);

int32_t loadMeshChunk(struct ChunkId *chunk, struct Mesh *mesh)
{
	struct MeshChunk *meshChunk = (struct MeshChunk *)chunk;
	uint8_t *ptr = (uint8_t *)chunk + sizeof(struct MeshChunk);

// Caluculate buffer sizes.
	uint8_t *tempPtr = ptr;
	uint32_t renderableCount = 0;
	for (uint32_t i = 0; i < meshChunk->numMeshes; ++i)
	{
		tempPtr = calcMeshSizeRecursive(tempPtr, &renderableCount, mesh); //, renderableIndex, nodeIndex, parentIndex, vertexBuffer, vertexBufferOffset, indexBuffer, indexBufferOffset);
	}
	assert(renderableCount > 0);
	mesh->hierarchy = (MeshNode*)malloc(sizeof(MeshNode) * renderableCount);
	mesh->transforms = (mat4x4*)malloc(sizeof(mat4x4) * renderableCount);
	mesh->renderables = (Renderable*)malloc(sizeof(Renderable) * renderableCount);
	mesh->numRenderables = renderableCount;
	uint32_t renderableIndex = 0;
	uint32_t nodeIndex = 0;
	int32_t parentIndex = -1;
	for (uint32_t i = 0; i < meshChunk->numMeshes; ++i)
	{
		ptr = loadMeshChunkRecursive(ptr, mesh, &renderableIndex, &nodeIndex, &parentIndex); //, renderableIndex, nodeIndex, parentIndex, vertexBuffer, vertexBufferOffset, indexBuffer, indexBufferOffset);
		if (ptr == NULL)
			return -1;
	}
	return 0;
}

uint8_t* calcMeshSizeRecursive(uint8_t *ptr, uint32_t *renderableCount, struct Mesh *mesh)
{
	struct MeshInfo *info = (struct MeshInfo *)ptr;
	if (mesh->versionMajor > 0 || (mesh->versionMajor == 0 && mesh->versionMinor > 2))
	{
		ptr += sizeof(uint32_t);
	}
	struct RenderableInfo *rendInfo = (struct RenderableInfo *)(ptr + sizeof(struct MeshInfo));

	uint32_t numRenderables = info->numRenderables;
	*renderableCount += numRenderables;

	uint8_t *meshData = (uint8_t *)((uint8_t *)rendInfo + sizeof(struct RenderableInfo) * numRenderables);
	for (uint32_t rend = 0; rend < numRenderables; ++rend)
	{
		struct Import_VertexBuffer *srcVertexBuffer = (struct Import_VertexBuffer *)meshData;
		uint32_t numVertices = rendInfo[rend].numVertices;
		uint32_t verticesSize = sizeof(struct Import_VertexBuffer) + srcVertexBuffer->m_streams[0].m_stride * numVertices;
		uint32_t numIndices = rendInfo[rend].numIndices;
		uint32_t indicesSize = sizeof(uint32_t) * numIndices;

		meshData = meshData + verticesSize + indicesSize;
	}
	ptr = (uint8_t*)meshData;
	uint32_t numChildren = info->numChildren;
	for (uint32_t i = 0; i < numChildren; ++i) {
		ptr = calcMeshSizeRecursive(ptr, renderableCount, mesh);
	}
	return ptr;
}

uint8_t* loadMeshChunkRecursive(uint8_t* ptr, struct Mesh *mesh, uint32_t *renderableIndex, uint32_t *nodeIndex, int32_t *parentIndex) //, VertexBuffer& vertexBuffer, int64_t& vertexBufferOffset, IndexBuffer& indexBuffer, int64_t& indexBufferOffset)
{
	struct MeshInfo *info = (struct MeshInfo *)ptr;
	ptr += sizeof(struct MeshInfo);
	uint32_t meshHash = 0;
	if (mesh->versionMajor > 0 || (mesh->versionMajor == 0 && mesh->versionMinor > 2))
	{
		meshHash = *(uint32_t*)ptr;
		ptr += sizeof(uint32_t);
	}
	else
	{
		meshHash = hashString(s_currentMeshName);
	}
	mesh->numRenderables = 0;
	struct RenderableInfo* rendInfo = (struct RenderableInfo*)ptr; // (ptr + sizeof(struct MeshInfo));

	mesh->hierarchy[*nodeIndex].parentId = *parentIndex;
	mesh->hierarchy[*nodeIndex].numRenderables = info->numRenderables;
	mesh->hierarchy[*nodeIndex].numChildren = info->numChildren;
	uint32_t numRenderables = info->numRenderables;

	uint8_t* meshData = (uint8_t*)((uint8_t*)rendInfo + sizeof(struct RenderableInfo) * numRenderables);

	rgfx_renderable firstRenderable = { 0 };
	for (uint32_t rend = 0; rend < numRenderables; ++rend)
	{
		struct Import_VertexBuffer* srcVertexBuffer = (struct Import_VertexBuffer*)meshData;
		uint32_t numVertices = rendInfo[rend].numVertices;
		uint32_t stride = srcVertexBuffer->m_streams[0].m_stride;
		uint32_t verticesSize = sizeof(struct Import_VertexBuffer) + stride * numVertices;
		uint32_t numIndices = rendInfo[rend].numIndices;
		uint32_t indicesSize = sizeof(uint32_t) * numIndices;

		mesh->aabbMin[0] = rendInfo->aabbMinX;
		mesh->aabbMin[1] = rendInfo->aabbMinY;
		mesh->aabbMin[2] = rendInfo->aabbMinZ;
		mesh->aabbMax[0] = rendInfo->aabbMaxX;
		mesh->aabbMax[1] = rendInfo->aabbMaxY;
		mesh->aabbMax[2] = rendInfo->aabbMaxZ;
/*			
		int32_t materialIndex = FindMaterial(rendInfo->materialHash);
		if (materialIndex == -1)
		{
			return nullptr;
			materialIndex = FindMaterial(rendInfo->materialHash);
		}
*/		
//CLR - Temp
		int32_t materialIndex = rsys_hm_find(&mesh->materialLookup, rendInfo[rend].materialHash);
//CLR
		int32_t vbIdx = mesh->materials[materialIndex].flags & kMFSkinned ? 1 : 0 ;
		rgfx_vertexBuffer vb = { .id = vbIdx + 1 };
		rgfx_bufferInfo* vbInfo = rgfx_getVertexBufferInfo(vb);
		int64_t vertexBufferOffset = vbInfo->writeIndex;
		rgfx_bufferInfo* ibInfo = rgfx_getIndexBufferInfo(vb);
		int64_t indexBufferOffset = ibInfo->writeIndex;
		uint32_t firstVertex = (uint32_t)(vertexBufferOffset / stride);
		uint32_t firstIndex = (uint32_t)(indexBufferOffset / ibInfo->stride); // sizeof(uint32_t));
		uint32_t indexCount = numIndices;

		Renderable *thisRenderable = mesh->renderables + *renderableIndex;
		thisRenderable->firstVertex = firstVertex;
		thisRenderable->firstIndex = firstIndex;
		thisRenderable->indexCount = indexCount;
		thisRenderable->materialIndex = materialIndex;

		rgfx_renderable renderable = rgfx_addRenderable(&(rgfx_renderableInfo) {
			.firstVertex = firstVertex,
			.firstIndex = firstIndex,
			.indexCount = indexCount, 
			.materialIndex = materialIndex,
			.mesh = mesh,
		});
		firstRenderable = firstRenderable.id <= 0 ? renderable : firstRenderable;
//		MaterialPtr pMaterial = MaterialManager::Get()->Find(rendInfo[rend].materialHash);
		uint32_t numStreams = srcVertexBuffer->m_numStreams;

//		OddJob::dispatchAsync(OddJob::getMainQueue(), [=, &vertexBuffer, &indexBuffer]() {
			for (uint32_t i = 0; i < numStreams; ++i)
			{
				uint8_t* vertexData = (uint8_t*)srcVertexBuffer + sizeof(struct Import_VertexBuffer);
//CLR - PortMe.		
//				vertexBuffer.writeData(i, vertexBufferOffset, numVertices * stride, vertexData);
				rgfx_writeVertexData(vb, numVertices * stride, vertexData);
			}
			uint8_t* indices = (uint8_t*)srcVertexBuffer + verticesSize;
//CLR - PortMe.				
//			indexBuffer.writeData(indexBufferOffset, numIndices * sizeof(uint32_t), indices);
			rgfx_writeIndexData(vb, numIndices * sizeof(uint32_t), indices, sizeof(uint32_t)); // CLR - Need to handle different index sizes.
//		});
		vertexBufferOffset += (numVertices * stride);
		indexBufferOffset += (numIndices * ibInfo->stride); // sizeof(uint32_t));
		meshData = meshData + verticesSize + indicesSize;
		(*renderableIndex)++;
	}
	rgfx_addMesh(&(rgfx_meshDesc) {
		.meshHash = meshHash,
		.numRenderables = numRenderables,
		.firstRenderable = firstRenderable,
		.aabbMin[0] = mesh->aabbMin[0],
		.aabbMin[1] = mesh->aabbMin[1],
		.aabbMin[2] = mesh->aabbMin[2],
		.aabbMax[0] = mesh->aabbMax[0],
		.aabbMax[1] = mesh->aabbMax[1],
		.aabbMax[2] = mesh->aabbMax[2],
	});

	mesh->numRenderables += numRenderables;
	ptr = (uint8_t*)meshData;
//	(uint32_t*)&info->worldMatrix, 16;
	memcpy(&mesh->transforms[*nodeIndex], &info->worldMatrix, sizeof(mat4x4));
	*parentIndex = (*nodeIndex)++;
	uint32_t numChildren = info->numChildren;
	for (uint32_t i = 0; i < numChildren; ++i) {
		ptr = loadMeshChunkRecursive(ptr, mesh, renderableIndex, nodeIndex, parentIndex); // , vertexBuffer, vertexBufferOffset, indexBuffer, indexBufferOffset);
//		renderableIndex++;
	}
	return ptr;
}

void loadMeshInstanceChunk(struct ChunkId* chunk, struct Mesh* mesh)
{
//	if (mesh->numRenderables == 1)
//		return;						//CLR - If we only have 1 renderable then we have no need for this (I think!).

	struct MeshInstanceChunk* meshInstChunk = (struct MeshInstanceChunk*)chunk;
	uint32_t numMeshInstances = meshInstChunk->numMeshInstances;

	uint8_t* ptr = (uint8_t*)chunk + sizeof(struct MeshInstanceChunk);
	for (uint32_t i = 0; i < numMeshInstances; ++i)
	{
		uint32_t len = *(uint32_t*)ptr;
		ptr += sizeof(uint32_t);
		char strBuffer[1024];
		strncpy(strBuffer, (char*)ptr, len);
		strBuffer[len] = '\0';
//		std::string filename(strBuffer);								//CLR - Obviously don't need this, but what was filename for?
		uint32_t alignedLen = len + 4 & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes
		ptr += alignedLen;
		struct MeshInstanceInfo* info = (struct MeshInstanceInfo*)ptr;
		(void)info;
		uint32_t meshHash = info->meshHash;
		AssertMsg(meshHash != 0, "Mesh hash is 0 which could be a problem. Does this ever occur?\n");
		(void)meshHash;
		mat4x4 worldMatrix;
		memcpy(&worldMatrix, &info->worldMatrix, sizeof(mat4x4));
		ptr += sizeof(struct MeshInstanceInfo);
//		rgfx_addMeshInstanceFromS3D(strBuffer, meshHash, &worldMatrix);
		rgfx_addMeshInstance(&(rgfx_meshInstanceDesc) {
			.instanceName = strBuffer,
			.meshHash = meshHash,
			.transform.xAxis = worldMatrix.xAxis,
			.transform.yAxis = worldMatrix.yAxis,
			.transform.zAxis = worldMatrix.zAxis,
			.transform.wAxis = worldMatrix.wAxis,
		});
	}
}
