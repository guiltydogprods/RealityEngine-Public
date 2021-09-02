#include "stdafx.h"
#include "scene.h"
#include "mesh.h"
#include "stb/stretchy_buffer.h"
#ifndef RE_PLATFORM_WIN64
#include <GLES2/gl2ext.h>

//typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTIVEXTPROC) (GLuint id, GLenum pname, GLint* params);
//typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTUI64VEXTPROC) (GLuint id, GLenum pname, GLuint64* params);
PFNGLGENQUERIESEXTPROC glGenQueriesEXT = NULL;
PFNGLDELETEQUERIESEXTPROC glDeleteQueriesEXT = NULL;
PFNGLISQUERYEXTPROC glIsQueryEXT = NULL;
PFNGLBEGINQUERYEXTPROC glBeginQueryEXT = NULL;
PFNGLENDQUERYEXTPROC glEndQueryEXT = NULL;
PFNGLQUERYCOUNTEREXTPROC glQueryCounterEXT = NULL;
PFNGLGETQUERYIVEXTPROC glGetQueryivEXT = NULL;

PFNGLGETQUERYOBJECTIVEXTPROC glGetQueryObjectivEXT = NULL;
PFNGLGETQUERYOBJECTUIVEXTPROC glGetQueryObjectuivEXT = NULL;
PFNGLGETQUERYOBJECTI64VEXTPROC glGetQueryObjecti64vEXT = NULL;
PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64vEXT = NULL;

#endif

#define BUFFER_OFFSET(i) ((char *)NULL + (i))
#define NUM_QUERIES (2)
#define ION_PI (3.14159265359f)
#define ION_180_OVER_PI (180.0f / ION_PI)
#define ION_PI_OVER_180 (ION_PI / 180.0f)

#define DEGREES(x) ((x) * ION_180_OVER_PI)
#define RADIANS(x) ((x) * ION_PI_OVER_180)

typedef struct rgfx_sceneData
{
	rgfx_light* lightArray;								// Array of Lights. CLR - Should we store different light types seperatly? 
	rgfx_meshInfo* meshArray;							// Array of Meshes.
	rgfx_meshInstanceInfo* meshInstanceArray;			// Array of Mesh Instances.
	rgfx_meshInstanceInfo* visibleMeshInstanceArray;	// Array of Visible Mesh Instance.
	rgfx_renderableInfo* renderableArray;				// Array of Renderables.
	rgfx_instancedMeshInfo* instancedMeshArray;			// Array of Instanced Meshes (used to locate instance batches per mesh).
	rgfx_instanceBatchInfo* batchArray;					// Array of Batches.
	rgfx_instanceBatchInfo* sortedBatchArray;			// Sorted Array of Batches.
	rgfx_renderStats renderStats;
	rsys_hash meshLookup;
	rsys_hash meshInstanceLookup;
	rsys_hash batchesLookup;					// CLR - Should the be renamed instancedMeshLookup as we are looking up the instanced mesh data?
	GLuint queries[NUM_QUERIES];
	GLint available[NUM_QUERIES];
	GLuint64 elapsed[NUM_QUERIES];
	int32_t queryIndex;
}rgfx_sceneData;

static rgfx_sceneData s_sceneData = { 0 };
extern rgfx_rendererData s_rendererData;

void rgfx_drawDebugBoundingBox(vec4* xPoints);

void rgfx_initializeScene(void)
{
	memset(&s_sceneData, 0, sizeof(s_sceneData));

#ifndef RE_PLATFORM_WIN64
	glGenQueriesEXT = (PFNGLGENQUERIESEXTPROC)eglGetProcAddress("glGenQueriesEXT");
	glDeleteQueriesEXT = (PFNGLDELETEQUERIESEXTPROC)eglGetProcAddress("glDeleteQueriesEXT");
	glIsQueryEXT = (PFNGLISQUERYEXTPROC)eglGetProcAddress("glIsQueryEXT");
	glBeginQueryEXT = (PFNGLBEGINQUERYEXTPROC)eglGetProcAddress("glBeginQueryEXT");
	glEndQueryEXT = (PFNGLENDQUERYEXTPROC)eglGetProcAddress("glEndQueryEXT");
	glQueryCounterEXT = (PFNGLQUERYCOUNTEREXTPROC)eglGetProcAddress("glQueryCounterEXT");
	glGetQueryivEXT = (PFNGLGETQUERYIVEXTPROC)eglGetProcAddress("glGetQueryivEXT");

	glGetQueryObjectivEXT = (PFNGLGETQUERYOBJECTIVEXTPROC)eglGetProcAddress("glGetQueryObjectivEXT");
	glGetQueryObjectuivEXT = (PFNGLGETQUERYOBJECTUIVEXTPROC)eglGetProcAddress("glGetQueryObjectuivEXT");
	glGetQueryObjecti64vEXT = (PFNGLGETQUERYOBJECTI64VEXTPROC)eglGetProcAddress("glGetQueryObjecti64vEXT");
	glGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC)eglGetProcAddress("glGetQueryObjectui64vEXT");
#endif
	glGenQueries(NUM_QUERIES, s_sceneData.queries);
}

void rgfx_addLight(vec4 position, vec4 color)
{
	rgfx_light newLight = {
		.position = position,
		.color = color
	};
	sb_push(s_sceneData.lightArray, newLight);

	rgfx_buffer lightBuffer = rgfx_getLightsBuffer();
	rgfx_light* mappedPtr = rgfx_mapBufferRange(lightBuffer, sizeof(rgfx_light) * s_rendererData.lightCount++, sizeof(rgfx_light));
	if (mappedPtr)
	{
		*mappedPtr = newLight;
		rgfx_unmapBuffer(lightBuffer);
	}

/*
	int32_t idx = lightBuffer.id - 1;
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
*/
}

rgfx_mesh rgfx_addMesh(const rgfx_meshDesc* meshDesc)
{
	rgfx_meshInfo newMesh = {
		.meshNameHash = meshDesc->meshHash,
		.numRenderables = meshDesc->numRenderables,
		.firstRenderable = meshDesc->firstRenderable,
		.aabbMin = (vec4) { meshDesc->aabbMin[0], meshDesc->aabbMin[1], meshDesc->aabbMin[2], 1.0f},
		.aabbMax = (vec4) { meshDesc->aabbMax[0], meshDesc->aabbMax[1], meshDesc->aabbMax[2], 1.0f},
	};
	sb_push(s_sceneData.meshArray, newMesh);
	int32_t meshIdx = sb_count(s_sceneData.meshArray) - 1;

/*
	rgfx_meshInfo* meshInfo = sb_add(s_sceneData.meshArray, 1);
	meshInfo->meshNameHash = meshDesc->meshHash;
	meshInfo->numRenderables = meshDesc->numRenderables;
	meshInfo->firstRenderable = meshDesc->firstRenderable;
	int32_t meshIdx = meshInfo - s_sceneData.meshArray;
*/
	if (meshDesc->meshHash)
	{
		rsys_hm_insert(&s_sceneData.meshLookup, meshDesc->meshHash, meshIdx);
	}
	return (rgfx_mesh) { .id = meshIdx + 1 };
}

void rgfx_popMesh(int32_t numToPop)
{
	int32_t count = sb_count(s_sceneData.meshArray);
	rgfx_meshInfo* meshInfo = &s_sceneData.meshArray[count - 1];

	sb_pop(s_sceneData.renderableArray, meshInfo->numRenderables);
	sb_pop(s_sceneData.meshArray, numToPop);
}

rgfx_mesh rgfx_findMesh(uint32_t meshHash)
{
	int32_t meshIdx = rsys_hm_find(&s_sceneData.meshLookup, meshHash);

	return (rgfx_mesh) { .id = meshIdx + 1 };
}

rgfx_meshInstance rgfx_findMeshInstance(uint32_t instanceHash)
{
	int32_t instanceIdx = rsys_hm_find(&s_sceneData.meshInstanceLookup, instanceHash);

	return (rgfx_meshInstance) { .id = instanceIdx + 1 };
}

rgfx_meshInstance rgfx_addMeshInstance(const rgfx_meshInstanceDesc* meshInstance)
{
	rgfx_instancedMeshInfo* meshInfo = NULL;
	assert(meshInstance->meshName || meshInstance->mesh.id > 0 || meshInstance->meshHash != 0);
	uint32_t meshNameHash = 0;
	if (meshInstance->mesh.id > 0) {
		assert(0);							// CLR - Do we actually use this path? Do we need it?
	}
	else if (meshInstance->meshHash > 0)
	{
		meshNameHash = meshInstance->meshHash;
	}
	else
	{
		meshNameHash = hashString(meshInstance->meshName);
	}

	int32_t index = rsys_hm_find(&s_sceneData.batchesLookup, meshNameHash);
	if (index < 0)
	{
		index = sb_count(s_sceneData.instancedMeshArray);
		rsys_hm_insert(&s_sceneData.batchesLookup, meshNameHash, index);

		Mesh* mesh = NULL;
		if (meshInstance->meshName > 0) {
			mesh = rres_findMesh(meshNameHash);	// CLR - Only search for a mesh, if we have a meshName (i.e. loaded from file).
		}
		if (mesh)
		{
			if (/*mesh->numRenderables > 1 &&*/ (mesh->versionMajor > 0 || (mesh->versionMajor == 0 && mesh->versionMinor > 2))) {
				rgfx_meshInstance alreadyLoaded = {.id = 0 };
				if (meshInstance->instanceName)
				{
					alreadyLoaded = rgfx_findMeshInstance(hashString(meshInstance->instanceName));
				}
				return alreadyLoaded;
			}

			//			mesh->numRenderables = 1;
			meshInfo = sb_add(s_sceneData.instancedMeshArray, 1);
			meshInfo->instanceCount = 0;
			meshInfo->batchCount = mesh->numRenderables;
			meshInfo->firstBatch = s_sceneData.batchArray != NULL ? sb_count(s_sceneData.batchArray) : 0;
			for (uint32_t i = 0; i < mesh->numRenderables; ++i)
			{
				rgfx_instanceBatchInfo newBatch = { 0 };
				newBatch.command = (DrawElementsIndirectCommand){
					.count = mesh->renderables[i].indexCount,
					.firstIndex = mesh->renderables[i].firstIndex * 2,
					.baseVertex = mesh->renderables[i].firstVertex,
				};
				newBatch.matricesBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.capacity = 16384,
					.stride = sizeof(mat4x4),
					.flags = kMapWriteBit
				});
				newBatch.materialsBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.capacity = 16384,
					.stride = sizeof(MaterialProperties),
					.flags = kMapWriteBit
				});
				rgfx_materialFlags materialFlags = mesh->materials[mesh->renderables[i].materialIndex].flags;
				if (materialFlags & kMFSkinned)
				{
					newBatch.skinningBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
						.capacity = sizeof(mat4x4) * 20,
						.stride = sizeof(mat4x4),
						.flags = kMapWriteBit
					});
				}
				newBatch.albedoMap = mesh->materials[mesh->renderables[i].materialIndex].albedoMap;
				newBatch.normalMap = mesh->materials[mesh->renderables[i].materialIndex].normalMap;
				newBatch.metallicMap = mesh->materials[mesh->renderables[i].materialIndex].metallicMap;
				newBatch.roughnessMap = mesh->materials[mesh->renderables[i].materialIndex].roughnessMap;
				newBatch.emissiveMap = mesh->materials[mesh->renderables[i].materialIndex].emissiveMap;
				newBatch.aoMap = mesh->materials[mesh->renderables[i].materialIndex].ambientMap;
				newBatch.albedo[0] = mesh->materials[mesh->renderables[i].materialIndex].albedo[0];
				newBatch.albedo[1] = mesh->materials[mesh->renderables[i].materialIndex].albedo[1];
				newBatch.albedo[2] = mesh->materials[mesh->renderables[i].materialIndex].albedo[2];
				newBatch.albedo[3] = mesh->materials[mesh->renderables[i].materialIndex].albedo[3];
				newBatch.roughness = mesh->materials[mesh->renderables[i].materialIndex].roughness;
				newBatch.materialFlags = mesh->materials[mesh->renderables[i].materialIndex].flags;
				newBatch.materialNameHash = mesh->materials[mesh->renderables[i].materialIndex].nameHash;
				sb_push(s_sceneData.batchArray, newBatch);
			}
		}
		else
		{
			int32_t meshIdx = rsys_hm_find(&s_sceneData.meshLookup, meshNameHash);
			assert(meshIdx >= 0);
			rgfx_meshInfo* mesh = &s_sceneData.meshArray[meshIdx];

			meshInfo = sb_add(s_sceneData.instancedMeshArray, 1);
			meshInfo->instanceCount = 0;
			meshInfo->batchCount = mesh->numRenderables;
			meshInfo->firstBatch = s_sceneData.batchArray != NULL ? sb_count(s_sceneData.batchArray) : 0;
			int32_t renderableIdx = mesh->firstRenderable.id - 1;
			assert(renderableIdx >= 0);
			for (uint32_t i = 0; i < mesh->numRenderables; ++i)
			{
				int32_t currentRenderableIdx = renderableIdx + i;
				rgfx_instanceBatchInfo newBatch;
				int32_t materialIndex = s_sceneData.renderableArray[currentRenderableIdx].materialIndex;
				newBatch.command = (DrawElementsIndirectCommand){
					.count = s_sceneData.renderableArray[currentRenderableIdx].indexCount,
					.firstIndex = s_sceneData.renderableArray[currentRenderableIdx].firstIndex * 2,
					.baseVertex = s_sceneData.renderableArray[currentRenderableIdx].firstVertex,
				};
				newBatch.matricesBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.capacity = 16384,
					.stride = sizeof(mat4x4),
					.flags = kMapWriteBit
				});
				newBatch.materialsBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.capacity = 16384,
					.stride = sizeof(MaterialProperties),
					.flags = kMapWriteBit
				});
				Mesh* meshHack = s_sceneData.renderableArray[currentRenderableIdx].mesh;
				rgfx_materialFlags materialFlags = meshHack->materials[materialIndex].flags;
				if (materialFlags & kMFSkinned)
				{
					newBatch.skinningBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
						.capacity = sizeof(mat4x4) * 20,
						.stride = sizeof(mat4x4),
						.flags = kMapWriteBit
					});
				}

				newBatch.albedoMap = meshHack->materials[materialIndex].albedoMap;
				newBatch.normalMap = meshHack->materials[materialIndex].normalMap;
				newBatch.metallicMap = meshHack->materials[materialIndex].metallicMap;
				newBatch.roughnessMap = meshHack->materials[materialIndex].roughnessMap;
				newBatch.emissiveMap = meshHack->materials[materialIndex].emissiveMap;
				newBatch.aoMap = meshHack->materials[materialIndex].ambientMap;
				newBatch.albedo[0] = meshHack->materials[materialIndex].albedo[0];
				newBatch.albedo[1] = meshHack->materials[materialIndex].albedo[1];
				newBatch.albedo[2] = meshHack->materials[materialIndex].albedo[2];
				newBatch.albedo[3] = meshHack->materials[materialIndex].albedo[3];
				newBatch.roughness = meshHack->materials[materialIndex].roughness;
				newBatch.materialFlags = materialFlags;
				newBatch.materialNameHash = meshHack->materials[materialIndex].nameHash;
				sb_push(s_sceneData.batchArray, newBatch);
			}
		}
	}

	bool xAxisZero = vec4_isZero(meshInstance->transform.xAxis);
	bool yAxisZero = vec4_isZero(meshInstance->transform.yAxis);
	bool zAxisZero = vec4_isZero(meshInstance->transform.zAxis);
	bool wAxisZero = vec4_isZero(meshInstance->transform.wAxis);

	rgfx_meshInstanceInfo* instance = sb_add(s_sceneData.meshInstanceArray, 1);
	instance->instanceMatrix.xAxis = xAxisZero ? (vec4) { 1.0f, 0.0f, 0.0f, 0.0f } : meshInstance->transform.xAxis;
	instance->instanceMatrix.yAxis = yAxisZero ? (vec4) { 0.0f, 1.0f, 0.0f, 0.0f } : meshInstance->transform.yAxis;
	instance->instanceMatrix.zAxis = zAxisZero ? (vec4) { 0.0f, 0.0f, 1.0f, 0.0f } : meshInstance->transform.zAxis;
	instance->instanceMatrix.wAxis = wAxisZero ? (vec4) { 0.0f, 0.0f, 0.0f, 1.0f } : meshInstance->transform.wAxis;
	memset(&instance->material, 0, sizeof(MaterialProperties));
	instance->meshNameHash = meshNameHash;

	int32_t instanceIdx = sb_count(s_sceneData.meshInstanceArray) - 1;
	rsys_hm_insert(&s_sceneData.meshInstanceLookup, hashString(meshInstance->instanceName), instanceIdx);
	meshInfo = &s_sceneData.instancedMeshArray[index];
	int32_t instanceTextureIndex = meshInfo->instanceCount++;
	int32_t batchIndex = meshInfo->firstBatch;
	s_sceneData.meshInstanceArray[instanceIdx].material.diffuse[0] = s_sceneData.batchArray[batchIndex].albedo[0];
	s_sceneData.meshInstanceArray[instanceIdx].material.diffuse[1] = s_sceneData.batchArray[batchIndex].albedo[1];
	s_sceneData.meshInstanceArray[instanceIdx].material.diffuse[2] = s_sceneData.batchArray[batchIndex].albedo[2];
	s_sceneData.meshInstanceArray[instanceIdx].material.diffuse[3] = s_sceneData.batchArray[batchIndex].albedo[3];
	s_sceneData.meshInstanceArray[instanceIdx].material.specular[0] = instanceTextureIndex;
	s_sceneData.meshInstanceArray[instanceIdx].material.specularPower = s_sceneData.batchArray[batchIndex].roughness;

	int32_t instanceIndex = instance - s_sceneData.meshInstanceArray;
	return (rgfx_meshInstance) { .id = instanceIndex + 1 };
}

#if 0
rgfx_meshInstance rgfx_addMeshInstanceFromS3D(const char* instanceName, uint32_t meshHash, mat4x4* worldMatrix)
{
	rgfx_meshInstance retVal = { .id = 0 };
	int32_t meshIndex = rsys_hm_find(&s_sceneData.meshLookup, meshHash);
	if (meshIndex < 0)
	{
		assert(0);	// No mesh, theoretically this should never happen, so assert if it does.
		return retVal;
	}

	rgfx_meshInstanceInfo newInstance;
	newInstance.instanceMatrix.xAxis = worldMatrix->xAxis;
	newInstance.instanceMatrix.yAxis = worldMatrix->yAxis;
	newInstance.instanceMatrix.zAxis = worldMatrix->zAxis;
	newInstance.instanceMatrix.wAxis = worldMatrix->wAxis;
	newInstance.meshNameHash = meshHash;
	sb_push(s_sceneData.meshInstanceArray, newInstance);
	retVal.id = sb_count(s_sceneData.meshInstanceArray);
	int32_t instanceIdx = retVal.id - 1;
	rsys_hm_insert(&s_sceneData.meshInstanceLookup, hashString(instanceName), instanceIdx);

	rgfx_instancedMeshInfo* meshInfo = NULL;
	rgfx_instanceBatchInfo* batch = NULL;

	int32_t index = rsys_hm_find(&s_sceneData.batchesLookup, meshHash);
	if (index < 0)
	{
		index = sb_count(s_sceneData.instancedMeshArray);
		rsys_hm_insert(&s_sceneData.batchesLookup, meshHash, index);

		int32_t numRenderables = s_sceneData.meshArray[meshIndex].numRenderables;
		rgfx_renderable firstRenderable = s_sceneData.meshArray[meshIndex].firstRenderable;

		meshInfo = sb_add(s_sceneData.instancedMeshArray, 1);
		meshInfo->instanceCount = 0;
		meshInfo->batchCount = numRenderables;
		meshInfo->firstBatch = s_sceneData.batchArray != NULL ? sb_count(s_sceneData.batchArray) : 0;

		int32_t rendIdx = firstRenderable.id - 1;
		assert(rendIdx >= 0);
		for (int32_t rend = 0; rend < numRenderables; ++rend)
		{
			int32_t index = rendIdx + rend;
			uint32_t firstVertex = s_sceneData.renderableArray[index].firstVertex;
			uint32_t firstIndex = s_sceneData.renderableArray[index].firstIndex;
			uint32_t indexCount = s_sceneData.renderableArray[index].indexCount;
			int32_t materialIndex = s_sceneData.renderableArray[index].materialIndex;

			batch = sb_add(s_sceneData.batchArray, 1);
			batch->command = (DrawElementsIndirectCommand){
				.count = indexCount,
				.firstIndex = firstIndex * 2,
				.baseVertex = firstVertex,
			};
			batch->matricesBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
				.capacity = 16384,
				.stride = sizeof(mat4x4),
				.flags = kMapWriteBit
			});
			batch->materialsBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
				.capacity = 16384,
				.stride = sizeof(MaterialProperties),
				.flags = kMapWriteBit
			});
			Mesh* mesh = s_sceneData.renderableArray[index].mesh;
			rgfx_materialFlags materialFlags = mesh->materials[materialIndex].flags;
			if (materialFlags & kMFSkinned)
			{
				batch->skinningBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.capacity = sizeof(mat4x4) * 20,
					.stride = sizeof(mat4x4),
					.flags = kMapWriteBit
				});
			}

			batch->albedoMap = mesh->materials[materialIndex].albedoMap;
			batch->normalMap = mesh->materials[materialIndex].normalMap;
			batch->metallicMap = mesh->materials[materialIndex].metallicMap;
			batch->roughnessMap = mesh->materials[materialIndex].roughnessMap;
			batch->emissiveMap = mesh->materials[materialIndex].emissiveMap;
			batch->aoMap = mesh->materials[materialIndex].ambientMap;
			if (batch->aoMap.id |= 0)
			{
				rsys_print("Found AO Material\n");
			}
			batch->albedo[0] = mesh->materials[materialIndex].albedo[0];
			batch->albedo[1] = mesh->materials[materialIndex].albedo[1];
			batch->albedo[2] = mesh->materials[materialIndex].albedo[2];
			batch->albedo[3] = mesh->materials[materialIndex].albedo[3];
			batch->roughness = mesh->materials[materialIndex].roughness;
			batch->materialFlags = materialFlags;
			batch->materialNameHash = mesh->materials[materialIndex].nameHash;
		}
	}

	meshInfo = &s_sceneData.instancedMeshArray[index];
	int32_t instanceTextureIndex = meshInfo->instanceCount++;
	int32_t batchIndex = meshInfo->firstBatch;
	s_sceneData.meshInstanceArray[instanceIdx].material.diffuse[0] = s_sceneData.batchArray[batchIndex].albedo[0];
	s_sceneData.meshInstanceArray[instanceIdx].material.diffuse[1] = s_sceneData.batchArray[batchIndex].albedo[1];
	s_sceneData.meshInstanceArray[instanceIdx].material.diffuse[2] = s_sceneData.batchArray[batchIndex].albedo[2];
	s_sceneData.meshInstanceArray[instanceIdx].material.diffuse[3] = s_sceneData.batchArray[batchIndex].albedo[3];
	s_sceneData.meshInstanceArray[instanceIdx].material.specular[0] = instanceTextureIndex;
	s_sceneData.meshInstanceArray[instanceIdx].material.specularPower = s_sceneData.batchArray[batchIndex].roughness;
	return retVal;
}
#endif 

void rgfx_updateLight(int32_t lightIdx, const rgfx_lightUpdateDesc* lightUpdateDesc)
{
	int32_t lightCount = sb_count(s_sceneData.lightArray);
	if (lightCount == 0)
		return;

	assert(lightIdx >= 0 && lightIdx < lightCount);

	rgfx_buffer lightBuffer = rgfx_getLightsBuffer();
	rgfx_light* mappedPtr = (rgfx_light*)rgfx_mapBufferRange(lightBuffer, sizeof(rgfx_light) * lightIdx, sizeof(rgfx_light));
	if (mappedPtr)
	{
		vec4 lightScale = { lightUpdateDesc->colorScalar, lightUpdateDesc->colorScalar, lightUpdateDesc->colorScalar, 1.0f };
		mappedPtr->position = s_sceneData.lightArray[lightIdx].position;
		mappedPtr->color = s_sceneData.lightArray[lightIdx].color * lightScale;
		rgfx_unmapBuffer(lightBuffer);
	}
}

void rgfx_updateMeshInstance(rgfx_meshInstance instance, const rgfx_meshInstanceDesc* meshInstanceDesc)
{
	(void)instance;
	(void)meshInstanceDesc;
	int32_t instanceIdx = instance.id - 1;
	assert(instanceIdx >= 0);

	rgfx_meshInstanceInfo* instanceInfo = &s_sceneData.meshInstanceArray[instanceIdx];
	int32_t index = rsys_hm_find(&s_sceneData.batchesLookup, instanceInfo->meshNameHash);
	if (index >= 0)
	{
		bool xAxisZero = vec4_isZero(meshInstanceDesc->transform.xAxis);
		bool yAxisZero = vec4_isZero(meshInstanceDesc->transform.yAxis);
		bool zAxisZero = vec4_isZero(meshInstanceDesc->transform.zAxis);
		bool wAxisZero = vec4_isZero(meshInstanceDesc->transform.wAxis);

		instanceInfo->instanceMatrix.xAxis = xAxisZero ? instanceInfo->instanceMatrix.xAxis : meshInstanceDesc->transform.xAxis;
		instanceInfo->instanceMatrix.yAxis = yAxisZero ? instanceInfo->instanceMatrix.yAxis : meshInstanceDesc->transform.yAxis;
		instanceInfo->instanceMatrix.zAxis = zAxisZero ? instanceInfo->instanceMatrix.zAxis : meshInstanceDesc->transform.zAxis;
		instanceInfo->instanceMatrix.wAxis = wAxisZero ? instanceInfo->instanceMatrix.wAxis : meshInstanceDesc->transform.wAxis;

		int32_t index = rsys_hm_find(&s_sceneData.batchesLookup, instanceInfo->meshNameHash);
		rgfx_instancedMeshInfo* meshInfo = &s_sceneData.instancedMeshArray[index];
		for (int32_t i = 0; i < meshInfo->batchCount; ++i)
		{
			int32_t batchIndex = meshInfo->firstBatch + i;
			rgfx_instanceBatchInfo currentBatch = s_sceneData.batchArray[batchIndex];

			mat4x4* mappedPtr = (mat4x4*)rgfx_mapBufferRange(currentBatch.matricesBuffer, 0, sizeof(mat4x4));
			if (mappedPtr)
			{
				mappedPtr->xAxis = instanceInfo->instanceMatrix.xAxis;
				mappedPtr->yAxis = instanceInfo->instanceMatrix.yAxis;
				mappedPtr->zAxis = instanceInfo->instanceMatrix.zAxis;
				mappedPtr->wAxis = instanceInfo->instanceMatrix.wAxis;
				rgfx_unmapBuffer(currentBatch.matricesBuffer);
			}

			if (meshInstanceDesc->animationFrame & 0x80000000 && currentBatch.materialFlags & kMFSkinned)
			{
				int32_t frameNumber = meshInstanceDesc->animationFrame & 0x7fffffff;
				Mesh* mesh = rres_findMesh(instanceInfo->meshNameHash);
				assert(mesh);
				mat4x4* mappedPtr = (mat4x4*)rgfx_mapBufferRange(currentBatch.skinningBuffer, 0, sizeof(mat4x4) * mesh->numBones);
				if (mappedPtr)
				{
					for (uint32_t i = 0; i < mesh->numBones; ++i)
					{
						mat4x4 bone = mesh->bones[i + (mesh->numBones * frameNumber)];
						mat4x4 invBindMat = mesh->inverseBindBose[i];
						*mappedPtr++ = mat4x4_mul(bone, invBindMat);

					}
					rgfx_unmapBuffer(currentBatch.skinningBuffer);
				}
			}

			if (meshInstanceDesc->changeMaterialFlags != 0 && meshInstanceDesc->changeMaterialNameHash == currentBatch.materialNameHash)
			{
				if (meshInstanceDesc->changeMaterialFlags & currentBatch.materialFlags & kMFEmissiveMap)
				{
					currentBatch.emissiveMap = meshInstanceDesc->changeMaterialTexture;
				}
			}
		}
	}
}

void rgfx_updateMeshMaterial(uint32_t meshNameHash, const rgfx_meshInstanceDesc* meshInstanceDesc) //CLR - Should probably use a materialDesc or something.
{
	int32_t index = rsys_hm_find(&s_sceneData.batchesLookup, meshNameHash);
	if (index >= 0)
	{
		rgfx_instancedMeshInfo* meshInfo = &s_sceneData.instancedMeshArray[index];
		for (int32_t i = 0; i < meshInfo->batchCount; ++i)
		{
			int32_t batchIndex = meshInfo->firstBatch + i;
			rgfx_instanceBatchInfo* batch = &s_sceneData.batchArray[batchIndex];

			if (meshInstanceDesc->changeMaterialFlags != 0 && meshInstanceDesc->changeMaterialNameHash == batch->materialNameHash)
			{
				if (meshInstanceDesc->changeMaterialFlags & batch->materialFlags & kMFDiffuseMap)
				{
					batch->albedoMap = meshInstanceDesc->changeMaterialTexture;
					if (meshInstanceDesc->newMaterialFlags != 0)
					{
						batch->materialFlags = meshInstanceDesc->newMaterialFlags;
					}
				}
				if (meshInstanceDesc->changeMaterialFlags & kMFAmbientMap) // && (batch->materialFlags & kMFAmbientMap))
				{
					batch->aoMap = meshInstanceDesc->changeMaterialTexture;
					if (meshInstanceDesc->newMaterialFlags != 0)
					{
						batch->materialFlags = meshInstanceDesc->newMaterialFlags;
					}
				}
				if (meshInstanceDesc->changeMaterialFlags & batch->materialFlags & kMFEmissiveMap)
				{
					batch->emissiveMap = meshInstanceDesc->changeMaterialTexture;
				}
			}
		}
	}
}

rgfx_renderable rgfx_addRenderable(const rgfx_renderableInfo* renderableDesc)
{
	rgfx_renderableInfo* rendInfo = sb_add(s_sceneData.renderableArray, 1);
	rendInfo->firstVertex = renderableDesc->firstVertex;
	rendInfo->firstIndex = renderableDesc->firstIndex;
	rendInfo->indexCount = renderableDesc->indexCount;
	rendInfo->materialIndex = renderableDesc->materialIndex;
	rendInfo->mesh = renderableDesc->mesh;
	rendInfo->vertexBuffer = renderableDesc->vertexBuffer;
	rendInfo->pipeline = renderableDesc->pipeline;
	int32_t rendIdx = rendInfo - s_sceneData.renderableArray;

	return (rgfx_renderable) { .id = rendIdx + 1 };
}

int32_t cullInstances(bool bHmdMounted)
{
	int32_t culledCount = 0;

	int32_t numBatches = sb_count(s_sceneData.batchArray);
	for (int32_t i = 0; i < numBatches; ++i)
	{
		s_sceneData.batchArray[i].command.instanceCount = 0;
	}

	//		mat4x4 viewProjMatrixLeft = rgfx_getCameraViewProjMatrix(0);
	mat4x4 viewMatrixLeft = rgfx_getCameraViewMatrix(0);
	mat4x4 projectionMatrixLeft = rgfx_getCameraProjectionMatrix(0);
	//		mat4x4 viewProjMatrixRight;
	mat4x4 viewMatrixRight;
	mat4x4 projectionMatrixRight;
	if (bHmdMounted)
	{
		//			viewProjMatrixRight = rgfx_getCameraViewProjMatrix(1);
		viewMatrixRight = rgfx_getCameraViewMatrix(1);
		projectionMatrixRight = rgfx_getCameraProjectionMatrix(1);
	}

	int32_t meshInstanceCount = sb_count(s_sceneData.meshInstanceArray);
	for (int32_t inst = 0; inst < meshInstanceCount; ++inst)
	{
		mat4x4 worldMatrix = s_sceneData.meshInstanceArray[inst].instanceMatrix;
		mat4x4 modelViewLeft = mat4x4_mul(viewMatrixLeft, worldMatrix);
		mat4x4 modelViewRight;
		if (bHmdMounted)
		{
			modelViewRight = mat4x4_mul(viewMatrixRight, worldMatrix);
		}
		uint32_t hash = s_sceneData.meshInstanceArray[inst].meshNameHash;
		int32_t meshIndex = rsys_hm_find(&s_sceneData.meshLookup, hash);
		vec4 aabbMin = s_sceneData.meshArray[meshIndex].aabbMin;
		vec4 aabbMax = s_sceneData.meshArray[meshIndex].aabbMax;
		vec4 points[8];
		points[0] = (vec4){ aabbMin.x, aabbMin.y, aabbMin.z, 1.0f };
		points[1] = (vec4){ aabbMax.x, aabbMin.y, aabbMin.z, 1.0f };
		points[2] = (vec4){ aabbMin.x, aabbMax.y, aabbMin.z, 1.0f };
		points[3] = (vec4){ aabbMax.x, aabbMax.y, aabbMin.z, 1.0f };
		points[4] = (vec4){ aabbMin.x, aabbMin.y, aabbMax.z, 1.0f };
		points[5] = (vec4){ aabbMax.x, aabbMin.y, aabbMax.z, 1.0f };
		points[6] = (vec4){ aabbMin.x, aabbMax.y, aabbMax.z, 1.0f };
		points[7] = (vec4){ aabbMax.x, aabbMax.y, aabbMax.z, 1.0f };
		bool visible = false;
		uint32_t clipFlags = 0x3f;
		float averageZ = 0.0f;
//		vec4 xPoints[8];
		for (int32_t point = 0; point < 8; ++point)
		{
//			xPoints[point] = vec4_transform(worldMatrix, points[point]);
			uint32_t vertexFlags = 0;
			vec4 viewSpaceL = vec4_transform(modelViewLeft, points[point]);
			vec4 xPosL = vec4_transform(projectionMatrixLeft, viewSpaceL);
			float xL = xPosL.x;
			float yL = xPosL.y;
			float zL = xPosL.z;
			float wL = xPosL.w;
			float zValue = zL;
			vertexFlags |= xL > wL ? 0x01 : 0;
			vertexFlags |= xL < -wL ? 0x02 : 0;
			vertexFlags |= yL > wL ? 0x04 : 0;
			vertexFlags |= yL < -wL ? 0x08 : 0;
			vertexFlags |= zL > wL ? 0x10 : 0;
			vertexFlags |= zL < -wL ? 0x20 : 0;
			clipFlags &= vertexFlags;
			if (bHmdMounted)
			{
				//					vec4 xPosR = vec4_transform(viewProjMatrixRight, xPoints[point]);
				vec4 viewSpaceR = vec4_transform(modelViewRight, points[point]);
				vec4 xPosR = vec4_transform(projectionMatrixRight, viewSpaceR);
				float xR = xPosR.x;
				float yR = xPosR.y;
				float zR = xPosR.z;
				float wR = xPosR.w;
				zValue = (zValue + zR) * 0.5f;
				vertexFlags = xR > wR ? 0x01 : 0;
				vertexFlags |= xR < -wR ? 0x02 : 0;
				vertexFlags |= yR > wR ? 0x04 : 0;
				vertexFlags |= yR < -wR ? 0x08 : 0;
				vertexFlags |= zR > wR ? 0x10 : 0;
				vertexFlags |= zR < -wR ? 0x20 : 0;
				clipFlags &= vertexFlags;
			}
			averageZ += zValue;
			if (vertexFlags == 0)
			{
				visible = true;
//				break;
			}
		}
//		rgfx_drawDebugBoundingBox(xPoints);
		if (visible == false && clipFlags != 0)
		{
			culledCount++;
			continue;
		}
		averageZ = averageZ / 8.0f;

		rgfx_meshInstanceInfo* visibleInstance = sb_add(s_sceneData.visibleMeshInstanceArray, 1);
		memcpy(visibleInstance, &s_sceneData.meshInstanceArray[inst], sizeof(rgfx_meshInstanceInfo));
		visibleInstance->mesh.id = meshIndex + 1;
		visibleInstance->averageZ = averageZ;
	}
//	rgfx_drawDebugSphere(0.5f, 12, 12, kDebugColorGreen);

	return culledCount;
}

void prepareBatches()
{
	int32_t visibleInstanceCount = sb_count(s_sceneData.visibleMeshInstanceArray);

	for (int32_t inst = 0; inst < visibleInstanceCount; ++inst)
	{
		mat4x4 worldMatrix = s_sceneData.visibleMeshInstanceArray[inst].instanceMatrix;
//		mat4x4 worldMatrixI = mat4x4_orthoInverse(worldMatrix);
//		mat4x4 worldMatrixIT = mat4x4_orthoInverse(mat4x4_transpose(worldMatrix));

		int32_t meshIndex = s_sceneData.visibleMeshInstanceArray[inst].mesh.id - 1;
		assert(meshIndex >= 0);
		float averageZ = s_sceneData.visibleMeshInstanceArray[inst].averageZ;
		int32_t firstBatch = s_sceneData.instancedMeshArray[meshIndex].firstBatch;
		int32_t batchCount = s_sceneData.instancedMeshArray[meshIndex].batchCount;
		for (int32_t i = 0; i < batchCount; ++i)
		{
			int32_t batchIndex = firstBatch + i;
			rgfx_instanceBatchInfo currentBatch = s_sceneData.batchArray[batchIndex];

			mat4x4* mappedPtr = (mat4x4*)rgfx_mapBufferRange(currentBatch.matricesBuffer, sizeof(mat4x4) * currentBatch.command.instanceCount, sizeof(mat4x4));
			if (mappedPtr)
			{
				mappedPtr->xAxis = worldMatrix.xAxis;
				mappedPtr->yAxis = worldMatrix.yAxis;
				mappedPtr->zAxis = worldMatrix.zAxis;
				mappedPtr->wAxis = worldMatrix.wAxis;
				rgfx_unmapBuffer(currentBatch.matricesBuffer);
			}

			MaterialProperties* material = (MaterialProperties*)rgfx_mapBufferRange(currentBatch.materialsBuffer, sizeof(MaterialProperties) * currentBatch.command.instanceCount, sizeof(MaterialProperties));
			if (material)
			{
				material->diffuse[0] = s_sceneData.visibleMeshInstanceArray[inst].material.diffuse[0];
				material->diffuse[1] = s_sceneData.visibleMeshInstanceArray[inst].material.diffuse[1];
				material->diffuse[2] = s_sceneData.visibleMeshInstanceArray[inst].material.diffuse[2];
				material->diffuse[3] = s_sceneData.visibleMeshInstanceArray[inst].material.diffuse[3];
				material->specular[0] = s_sceneData.visibleMeshInstanceArray[inst].material.specular[0];
				material->specularPower = s_sceneData.visibleMeshInstanceArray[inst].material.specularPower;
				rgfx_unmapBuffer(currentBatch.materialsBuffer);
			}
			s_sceneData.batchArray[batchIndex].command.instanceCount++;
			s_sceneData.batchArray[batchIndex].averageZ = averageZ;
		}
	}
	int32_t batchCount = sb_count(s_sceneData.batchArray);
	for (int32_t batch = 0; batch < batchCount; ++batch)
	{
		float instanceCount = (float)s_sceneData.batchArray[batch].command.instanceCount;
		s_sceneData.batchArray[batch].averageZ = s_sceneData.batchArray[batch].averageZ / instanceCount;
	}
}

int compareBatches(const void* a, const void* b)
{
	const rgfx_instanceBatchInfo* batchA = (const rgfx_instanceBatchInfo*)a;
	const rgfx_instanceBatchInfo* batchB = (const rgfx_instanceBatchInfo*)b;
	//	if (batchA->materialFlags < batchB->materialFlags) return -1;
	//	if (batchA->materialFlags > batchB->materialFlags) return 1;
	//	return 0;

	uint32_t avgZA = *(uint32_t*)&batchA->averageZ;
	uint32_t avgZB = *(uint32_t*)&batchB->averageZ;
	uint64_t sortValueA = ((uint64_t)avgZA << 32) | (uint64_t)batchA->materialFlags;
	uint64_t sortValueB = ((uint64_t)avgZB << 32) | (uint64_t)batchB->materialFlags;
	return (sortValueA > sortValueB) - (sortValueA < sortValueB); // possible shortcut
//	return (batchA->averageZ > batchB->averageZ) - (batchA->averageZ < batchB->averageZ); // possible shortcut
//	return (batchA->materialFlags > batchB->materialFlags) - (batchA->materialFlags < batchB->materialFlags); // possible shortcut
	// return arg1 - arg2; // erroneous shortcut (fails if INT_MIN is present)
}

int compareInstances(const void* a, const void* b)
{
	const rgfx_meshInstanceInfo* instanceA = (const rgfx_meshInstanceInfo*)a;
	const rgfx_meshInstanceInfo* instanceB = (const rgfx_meshInstanceInfo*)b;
	//	if (batchA->materialFlags < batchB->materialFlags) return -1;
	//	if (batchA->materialFlags > batchB->materialFlags) return 1;
	//	return 0;
	return (instanceA->averageZ > instanceB->averageZ) - (instanceA->averageZ < instanceB->averageZ); // possible shortcut
	// return arg1 - arg2; // erroneous shortcut (fails if INT_MIN is present)
}

int32_t sortBatches(void)
{
	int32_t batchCount = sb_count(s_sceneData.batchArray);
	sb_add(s_sceneData.sortedBatchArray, batchCount);
	memcpy(s_sceneData.sortedBatchArray, s_sceneData.batchArray, sizeof(rgfx_instanceBatchInfo) * batchCount);
	qsort(s_sceneData.sortedBatchArray, batchCount, sizeof(s_sceneData.sortedBatchArray[0]), compareBatches);
/* // CLR - This doesn't work yet, but worth investigation at some point.
	insertion_sort(s_rendererData.sortedBatches, batchCount, sizeof(s_rendererData.sortedBatches[0]), ^(const void* a, const void* b) {
		const rgfx_instanceBatchInfo* batchA = (const rgfx_instanceBatchInfo*)a;
		const rgfx_instanceBatchInfo* batchB = (const rgfx_instanceBatchInfo*)b;
		//	if (batchA->materialFlags < batchB->materialFlags) return -1;
		//	if (batchA->materialFlags > batchB->materialFlags) return 1;
		//	return 0;
		return (batchA->materialFlags > batchB->materialFlags) - (batchA->materialFlags < batchB->materialFlags); // possible shortcut
		// return arg1 - arg2; // erroneous shortcut (fails if INT_MIN is present)
	});
*/
	return sb_count(s_sceneData.sortedBatchArray);
}

extern GLint g_viewIdLoc;

void rgfx_renderWorld(bool bScreenshotRequested)
{
	rsys_perfTimer timer;
	rsys_perfTimerStart(&timer);

	int32_t numRenderables = sb_count(s_sceneData.renderableArray);
	int32_t numMeshes = sb_count(s_sceneData.meshArray);

	bool bVrInitialized = rvrs_isInitialized();
#ifdef RE_PLATFORM_WIN64
	bool bHmdMounted = rvrs_isHmdMounted();
#else
	bool bHmdMounted = true;		// CLR - For now, we need to assume the HMD is being worn on Quest at this point.	
#endif
	if (bScreenshotRequested)
	{
		vec4 camEye = { 0.0f, 0.0f, 1.5f, 1.0f };
		vec4 camAt = { 0.0f, 0.0f, 0.0f, 1.0f };
		vec4 camUp = { 0.0f, 1.0f, 0.0f, 0.0f };
		mat4x4 camViewMatrix = mat4x4_lookAt(camEye, camAt, camUp);

		mat4x4 localHeadMatrix = rvrs_getHeadMatrix();
		mat4x4 hmdViewMatrix = mat4x4_orthoInverse(localHeadMatrix);
		mat4x4 viewMatrix = mat4x4_mul(hmdViewMatrix, camViewMatrix);
		mat4x4 camMatrixWS = mat4x4_orthoInverse(viewMatrix);
		const float fov = RADIANS(90.0f);
		const float aspectRatio = 9.0f / 16.0f;
		const float nearZ = 0.1f;
		const float farZ = 100.0f;
		const float focalLength = 1.0f / tanf(fov * 0.5f);

		float left = -nearZ / focalLength;
		float right = nearZ / focalLength;
		float bottom = -aspectRatio * nearZ / focalLength;
		float top = aspectRatio * nearZ / focalLength;

		mat4x4 projectionMatrix = mat4x4_frustum(left, right, bottom, top, nearZ, farZ);
		rgfx_updateTransforms(&(rgfx_cameraDesc) {
			.camera[0].position = camMatrixWS.wAxis,
			.camera[0].viewMatrix = viewMatrix,
			.camera[0].projectionMatrix = projectionMatrix,
			.count = 1,
		});
		bHmdMounted = false;
	}
	if (bHmdMounted)
	{
		vec4   camPos[2];
		mat4x4 viewMat[2];
		mat4x4 projMat[2];
		for (int32_t eye = 0; eye < 2; ++eye)
		{
			mat4x4 hmdViewMatrix = rvrs_getViewMatrixForEye(eye);
			mat4x4 viewMatrix = mat4x4_mul(hmdViewMatrix, s_rendererData.gameCamera.viewMatrix);
			mat4x4 camMatrixWS = mat4x4_orthoInverse(viewMatrix);
			viewMat[eye] = viewMatrix;
			projMat[eye] = rvrs_getProjectionMatrixForEye(eye);
			camPos[eye] = camMatrixWS.wAxis;
		}

		rgfx_updateTransforms(&(rgfx_cameraDesc) {
			.camera[0].position = camPos[0],
			.camera[0].viewMatrix = viewMat[0],
			.camera[0].projectionMatrix = projMat[0],
			.camera[1].position = camPos[1],
			.camera[1].viewMatrix = viewMat[1],
			.camera[1].projectionMatrix = projMat[1],
			.count = 2,
		});
	}

	rsys_perfTimerGetSplit(&timer);

	int32_t culledCount = cullInstances(bHmdMounted);

	s_sceneData.renderStats.cullUSecs = rsys_perfTimerGetSplit(&timer);

	int32_t visibleCount = sb_count(s_sceneData.visibleMeshInstanceArray);
	qsort(s_sceneData.visibleMeshInstanceArray, visibleCount, sizeof(s_sceneData.visibleMeshInstanceArray[0]), compareInstances);

	prepareBatches();
	int32_t batchCount = sortBatches();

	s_sceneData.renderStats.sortUSecs = rsys_perfTimerGetSplit(&timer);
/*
	if (bHmdMounted)
	{
		rvrs_beginFrame();
	}
*/
	glBeginQuery(GL_TIME_ELAPSED_EXT, s_sceneData.queries[s_sceneData.queryIndex]);
	s_sceneData.queryIndex = (s_sceneData.queryIndex + 1) & (NUM_QUERIES - 1);
	// Render the eye images.
	int32_t numEyeBuffers = bHmdMounted ? s_rendererData.numEyeBuffers : 1;
	for (int32_t eye = 0; eye < numEyeBuffers; eye++)
	{
		// NOTE: In the non-mv case, latency can be further reduced by updating the sensor prediction
		// for each eye (updates orientation, not position)
		rgfx_eyeFbInfo* frameBuffer;
		if (bHmdMounted)
		{
			frameBuffer = rgfx_getEyeFrameBuffer(eye); // &s_rendererData.eyeFbInfo[eye];
#ifdef RENDER_DIRECT_TO_SWAPCHAIN
			rvrs_bindEyeFramebuffer(eye);
#else
#ifdef VRSYSTEM_OCULUS_WIN
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->colourBuffers[0]);
#else
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->colourBuffers[frameBuffer->textureSwapChainIndex]);
#endif
#endif
			glEnable(GL_SCISSOR_TEST);
			glDepthMask(GL_TRUE);
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			glViewport(0, 0, frameBuffer->width, frameBuffer->height);
			glScissor(0, 0, frameBuffer->width, frameBuffer->height);
			glClearColor(0.125f, 0.0f, 0.125f, 1.0f);
// CLR - Invalidating color buffer seems faster on both PC and Quest.  Will keep this comment and glClear around for a while as I'll need
// to investigate a more robust clearing solution when in edit mode.  Depth clearing is always needed.
#ifdef RE_PLATFORM_WIN64
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#else
			glClear(GL_DEPTH_BUFFER_BIT);
			const GLenum attachments[1] = { GL_COLOR_ATTACHMENT0 };
			glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, attachments);
#endif
		}
		rgfx_bindBuffer(1, rgfx_getTransforms());
		rgfx_bindBuffer(3, rgfx_getLightsBuffer());

		bool transparencyEnabled = false;
		int32_t currentVbIdx = -1;
		int32_t currentShaderIdx = -1;
		int32_t skippedBatchCount = 0;
		for (int32_t batchIdx = 0; batchIdx < batchCount; ++batchIdx)
		{
			rgfx_instanceBatchInfo* batch = &s_sceneData.sortedBatchArray[batchIdx];
			if (batch->command.instanceCount == 0)
			{
				skippedBatchCount++;
				continue;
			}
			if (batch->materialFlags & kMFTransparent && transparencyEnabled == false)
			{
#ifdef VRSYSTEM_OCULUS_WIN
				if (bVrInitialized) {
					rvrs_renderAvatar(eye); // CLR - Temp render avatar before transparent meshes.
					currentVbIdx = -1;
					currentShaderIdx = -1;
				}
#endif
			}
			int32_t vbIdx = batch->materialFlags & kMFSkinned ? 1 : batch->materialFlags & kMFEmissiveMap ? 2 : 0; // CLR passing as .id so need to add 1.
			int32_t shaderIdx = 0;
			if (batch->materialFlags & kMFMetallicMap)
				shaderIdx = 4;
			else if (batch->materialFlags & kMFInstancedAO)
				shaderIdx = 7;
			else if (batch->materialFlags & kMFAmbientMap)
				shaderIdx = 6;
			else if (batch->materialFlags & kMFNormalMap)
				shaderIdx = 1;
			else if (batch->materialFlags & kMFSkinned)
				shaderIdx = 2;
			else if (batch->materialFlags & kMFEmissiveMap)
				shaderIdx = 3;
			else if (batch->materialFlags & kMFInstancedAlbedo)
				shaderIdx = 5;

			// Bind Vertex and Index Buffers.
			if (vbIdx != currentVbIdx || shaderIdx != currentShaderIdx)
			{
				rgfx_bindVertexBuffer((rgfx_vertexBuffer) { .id = (vbIdx & 0x1) + 1 });
				//				/*uint32_t indexCount =*/ rgfx_bindIndexBuffer(vbIdx & 0x1);
				currentVbIdx = vbIdx;
				currentShaderIdx = shaderIdx;
				rgfx_pipeline pipe = { .id = shaderIdx + 1 };
				rgfx_bindPipeline(pipe);
				GLuint program = rgfx_getPipelineProgram(pipe, kVertexShader);
				if (g_viewIdLoc >= 0)  // NOTE: will not be present when multiview path is enabled.
				{
					glProgramUniform1i(program, g_viewIdLoc, eye);
				}
			}
			if ((vbIdx & 0x1) == 1)
			{
				rgfx_bindBuffer(4, batch->skinningBuffer);
			}
			else
			{
				glBindBufferBase(GL_UNIFORM_BUFFER, 4, 0);
			}

			int32_t texIdx = batch->albedoMap.id - 1;
			if (texIdx >= 0)
			{
				rgfx_bindTexture(0, batch->albedoMap);
			}
			texIdx = batch->normalMap.id - 1;
			if (batch->materialFlags & kMFNormalMap && texIdx >= 0)
			{
				rgfx_bindTexture(1, batch->normalMap);
			}
			texIdx = batch->metallicMap.id - 1;
			if (batch->materialFlags & kMFMetallicMap && texIdx >= 0)
			{
				rgfx_bindTexture(2, batch->metallicMap);
			}
			texIdx = batch->roughnessMap.id - 1;
			if (batch->materialFlags & kMFRoughnessMap && texIdx >= 0)
			{
				rgfx_bindTexture(3, batch->roughnessMap);
			}
			texIdx = batch->emissiveMap.id - 1;
			if (texIdx >= 0)
			{
				rgfx_bindTexture(4, batch->emissiveMap);
			}
			texIdx = batch->aoMap.id - 1;
			if (batch->materialFlags & kMFAmbientMap && texIdx >= 0)
			{
				rgfx_bindTexture(5, batch->aoMap);
			}

			if (batch->materialFlags & kMFTransparent && transparencyEnabled == false)
			{
				glEnable(GL_BLEND);
//				glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);	// CLR - This works but looks terrible.  Needs investigating.
				transparencyEnabled = true;
				//				break;
			}
			rgfx_bindBuffer(0, batch->matricesBuffer);
			rgfx_bindBuffer(2, batch->materialsBuffer);
			// CLR - Switch to using 16 Bit Indices.  This should come through via batch (from model file).
//			if (!transparencyEnabled) {
			if (batch->command.instanceCount > 1)
			{
				glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch->command.count, GL_UNSIGNED_SHORT, BUFFER_OFFSET(batch->command.firstIndex), batch->command.instanceCount, batch->command.baseVertex);
			}
			else
			{
				glDrawElementsBaseVertex(GL_TRIANGLES, batch->command.count, GL_UNSIGNED_SHORT, BUFFER_OFFSET(batch->command.firstIndex), batch->command.baseVertex);
			}
			//			}
			batch->averageZ = 0.0f;
		}
		rgfx_unbindVertexBuffer();

		if (skippedBatchCount > 0)
		{
//			printf("%d instances (%d batches) of %d culled\n", culledCount, skippedBatchCount, meshInstanceCount);
		}

#if 0 //def RE_PLATFORM_ANDROID
				// Explicitly clear the border texels to black when GL_CLAMP_TO_BORDER is not available.
		if (s_rendererData.extensions.EXT_texture_border_clamp == false)
		{
			// Clear to fully opaque black.
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			// bottom
			glScissor(0, 0, frameBuffer->width, 1);
			glClear(GL_COLOR_BUFFER_BIT);
			// top
			glScissor(0, frameBuffer->height - 1, frameBuffer->width, 1);
			glClear(GL_COLOR_BUFFER_BIT);
			// left
			glScissor(0, 0, 1, frameBuffer->height);
			glClear(GL_COLOR_BUFFER_BIT);
			// right
			glScissor(frameBuffer->width - 1, 0, 1, frameBuffer->height);
			glClear(GL_COLOR_BUFFER_BIT);
		}
#endif
		/*
				if (bVrInitialized) {
					rvrs_renderAvatar(eye);
				}
		*/
		if (transparencyEnabled)
		{
			glDisable(GL_BLEND);
//			glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		}
//#ifdef VRSYSTEM_OCULUS_WIN
		rvrs_renderAvatar(eye);					// CLR - Temp, moved avatar render here as removed transparent glass.
//#endif
		rgui_render(eye, bHmdMounted);
		rgfx_drawDebugLines(eye, bHmdMounted);

#ifdef VRSYSTEM_OCULUS_WIN
#ifndef  RENDER_DIRECT_TO_SWAPCHAIN
		if (bHmdMounted)
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, frameBuffer->colourBuffers[0]);
			rvrs_bindEyeFramebuffer(eye);
			glBlitFramebuffer(0, 0, frameBuffer->width, frameBuffer->height, 0, 0, frameBuffer->width, frameBuffer->height,
				GL_COLOR_BUFFER_BIT, // | GL_DEPTH_BUFFER_BIT,
				GL_NEAREST);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		}
#endif
#else
		// Discard the depth buffer, so the tiler won't need to write it back out to memory.
		const GLenum depthAttachment[1] = { GL_DEPTH_ATTACHMENT };
		glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, depthAttachment);

		// Flush this frame worth of commands.
		glFlush();
#endif
//		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}

	glEndQuery(GL_TIME_ELAPSED_EXT);

	sb_free(s_sceneData.visibleMeshInstanceArray);
	s_sceneData.visibleMeshInstanceArray = NULL;
	sb_free(s_sceneData.sortedBatchArray);
	s_sceneData.sortedBatchArray = NULL;

	s_sceneData.renderStats.sceneUSecs = rsys_perfTimerStop(&timer);
	s_sceneData.renderStats.renderUSecs = timer.elapsedUsecs;

	glGetQueryObjectuiv(s_sceneData.queries[s_sceneData.queryIndex], GL_QUERY_RESULT_AVAILABLE, &s_sceneData.available[s_sceneData.queryIndex]);
	if (s_sceneData.available[s_sceneData.queryIndex])
	{
		uint64_t gpuNanos;
#ifdef RE_PLATFORM_WIN64
		glGetQueryObjectui64v(s_sceneData.queries[s_sceneData.queryIndex], GL_QUERY_RESULT, &gpuNanos);
#else
		GLint disjointOccurred;
		glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjointOccurred);
		if (!disjointOccurred) {
			glGetQueryObjectui64vEXT(s_sceneData.queries[s_sceneData.queryIndex], GL_QUERY_RESULT_EXT, &gpuNanos);
		}
#endif
		s_sceneData.renderStats.gpuUSecs = gpuNanos / 1000;
	}

	if (bHmdMounted)
	{
		rvrs_submit();
	}
}

void rgfx_drawDebugBoundingBox(vec4* xPoints)
{
//	assert(0);
//	vec4 xPoints[8];
	rgfx_drawDebugLine(xPoints[0].xyz, kDebugColorGrey, xPoints[1].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(xPoints[2].xyz, kDebugColorGrey, xPoints[3].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(xPoints[4].xyz, kDebugColorGrey, xPoints[5].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(xPoints[6].xyz, kDebugColorGrey, xPoints[7].xyz, kDebugColorGrey);

	rgfx_drawDebugLine(xPoints[0].xyz, kDebugColorGrey, xPoints[2].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(xPoints[1].xyz, kDebugColorGrey, xPoints[3].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(xPoints[4].xyz, kDebugColorGrey, xPoints[6].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(xPoints[5].xyz, kDebugColorGrey, xPoints[7].xyz, kDebugColorGrey);

	rgfx_drawDebugLine(xPoints[0].xyz, kDebugColorGrey, xPoints[4].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(xPoints[1].xyz, kDebugColorGrey, xPoints[5].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(xPoints[2].xyz, kDebugColorGrey, xPoints[6].xyz, kDebugColorGrey);
	rgfx_drawDebugLine(xPoints[3].xyz, kDebugColorGrey, xPoints[7].xyz, kDebugColorGrey);
}

rgfx_renderStats rgfx_getRenderStats(void)
{
	return s_sceneData.renderStats;
}

void rgfx_drawIndexedPrims(rgfx_primType type, int32_t indexCount, int32_t firstIndex)
{
	if (indexCount >= 3 && firstIndex >= 0)
	{
		GLenum nativeType = rgfx_getNativePrimType(type);
		glDrawElements(nativeType, indexCount, GL_UNSIGNED_SHORT, BUFFER_OFFSET(firstIndex));
	}
}

void rgfx_drawRenderableInstanced(rgfx_renderable rend, int32_t instanceCount)
{
	int32_t rendIdx = rend.id - 1;
	assert(rendIdx >= 0);
	rgfx_renderableInfo* ri = &s_sceneData.renderableArray[rendIdx];
	rgfx_bindVertexBuffer(ri->vertexBuffer);
	if (instanceCount > 1)
		glDrawElementsInstancedBaseVertex(GL_TRIANGLES, ri->indexCount, GL_UNSIGNED_SHORT, BUFFER_OFFSET(ri->firstIndex), instanceCount, ri->firstVertex);
	else
		glDrawElementsBaseVertex(GL_TRIANGLES, ri->indexCount, GL_UNSIGNED_SHORT, BUFFER_OFFSET(ri->firstIndex), ri->firstVertex);
}
