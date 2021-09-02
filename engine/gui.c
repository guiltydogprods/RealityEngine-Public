#include "stdafx.h"
#include "gui.h"
#include "resource.h"
#include "stb/stretchy_buffer.h"
#include "rgfx/mesh.h"
#include "font.h"
#include "math/plane.h"

#ifndef RE_PLATFORM_WIN64
#include "Block.h"
#else
extern void* (*_Block_copy)(const void* aBlock);
extern void (*_Block_release)(const void* aBlock);

#define Block_copy(...) ((__typeof(__VA_ARGS__))_Block_copy((const void *)(__VA_ARGS__)))
#define Block_release(...) _Block_release((const void *)(__VA_ARGS__))
#endif

#ifndef RE_PLATFORM_WIN64
static const int GL_FRAMEBUFFER_SRGB = 0x8DB9;		//CLR - Need to replace state manipulation with an rgfx system, so this is fine until then.
#endif

//#define TRIANGLE_STRIP_QUADS
#define MAX_UNTEXTURED_QUADS 512
#define MAX_TEXTURED_QUADS 512
#define MAX_TEXT_QUADS 2560
#define VERTICES_PER_QUAD 4
#ifndef TRIANGLE_STRIP_QUADS
#define INDICES_PER_QUAD 6
#else
#define INDICES_PER_QUAD 5
#endif

struct rgui_data
{
//	rgfx_instanceBatchInfo* batchArray;
//	rgfx_instancedMeshInfo* instancedMeshArray;
	rgui_frameDesc frameParams;
	rgui_buttonDesc* buttonDescArray;
	rgui_collisionQuad* collisionQuadArray;
	rgui_panelDesc* panelArray;
	rgui_quadDesc* untexturedQuadArray;
	rgui_quadDesc* texturedQuadArray;
	rgui_pointerDesc* pointerDescArray;
	rgui_sliderDesc* sliderDescArray;
	rgui_textDesc* textDescArray;
	rsys_hash batchesLookup;
	rgui_textureSetInfo textureSet;
	rgfx_pipeline panelPipeline;
	rgfx_pipeline texturedQuadPipeline;
	rgfx_pipeline textPipeline;
	rgfx_vertexBuffer panelVertexBuffer;
	rgfx_vertexBuffer texturedQuadVertexBuffer;
	rgfx_vertexBuffer textVertexBuffer;
	rgfx_buffer texturedQuadIndexStream;
	rgui_widgetType selectedWidgetType;
	int32_t totalNumChars;
	int32_t selectedWidgetIndex;
	uint32_t selectedWidgetPanelHash;
	vec4 prevPointerPos;
	vec4 mouseRayP1;
	vec4 mouseRayP2;
};

void rgui_updateSlider(int32_t sliderIndex, uint32_t panelHash, vec4 sliderPos, bool triggerPressed);
void rgui_renderQuads(int32_t eye, bool bHmdMounted);
void rgui_renderText(int32_t eye, bool bHmdMounted);
bool pointInTriangle(vec4 p, vec4 a, vec4 b, vec4 c);

static struct rgui_data s_guiData = { 0 };
const float kDefaultPointerLength = 2.0f;
const float kSliderAspectRatio = 1.0f;

rguiApi rgui_initialize(void)
{
	memset(&s_guiData, 0, sizeof(struct rgui_data));
	s_guiData.selectedWidgetIndex = -1;
	s_guiData.selectedWidgetType = kInvalidWidget;
	return (rguiApi) {
		.button = rgui_button,
		.beginPanel = rgui_beginPanel,
		.endPanel = rgui_endPanel,
		.findTexture = rgui_findTexture,
		.pointer = rgui_pointer,
		.mouseRay = rgui_mouseRay,
		.registerTextureSet = rgui_registerTextureSet,
		.slider = rgui_slider,
		.text3D = rgui_text3D,
		.update = rgui_update,
		.newFrame = rgui_newFrame,
		.endFrame = rgui_endFrame,
	};
}

void rgui_newFrame(rgui_frameDesc* desc)
{
	s_guiData.frameParams = *desc;
	if (vec4_isZero(s_guiData.frameParams.pointerRayStartPosition))
	{
		s_guiData.frameParams.pointerRayStartPosition = s_guiData.frameParams.controllerTransform.wAxis;
	}
	if (vec4_isZero(s_guiData.frameParams.pointerRayEndPosition))
	{
		if (s_guiData.frameParams.pointerLength > 0)
		{
			s_guiData.frameParams.pointerRayEndPosition = s_guiData.frameParams.pointerRayStartPosition + s_guiData.frameParams.controllerTransform.zAxis * s_guiData.frameParams.pointerLength;
		}
		else
		{
			s_guiData.frameParams.pointerRayEndPosition = s_guiData.frameParams.pointerRayStartPosition - s_guiData.frameParams.controllerTransform.zAxis * kDefaultPointerLength;
		}
	}
	s_guiData.frameParams.pointerRayVec = s_guiData.frameParams.pointerRayEndPosition - s_guiData.frameParams.pointerRayStartPosition;
}

//#include "Block.h"

bool rgui_button(rgui_buttonDesc* desc)
{
	int32_t buttonIndex = -1;
	bool pressed = false;

	if (desc == NULL || desc->panel.id == 0)
		goto skip;

	if (desc->width > 0.0f && desc->height > 0.0f)
	{
		//		vec4 xAxis = desc->rotation.xAxis;
		//		vec4 yAxis = desc->rotation.yAxis;
		//		vec4 zAxis = desc->rotation.zAxis;
		//		vec4 wAxis = desc->position;
		int32_t panelIndex = desc->panel.id - 1;
		assert(panelIndex >= 0);
		vec4 xAxis = s_guiData.panelArray[panelIndex].rotation.xAxis;
		vec4 yAxis = s_guiData.panelArray[panelIndex].rotation.yAxis;
		vec4 zAxis = s_guiData.panelArray[panelIndex].rotation.zAxis;
		vec4 panelPos = s_guiData.panelArray[panelIndex].position;
		uint32_t panelHash = s_guiData.panelArray[panelIndex].panelHash;		// CLR - we can use this hash as a means to distinguish widgets with the same index on different panels.
		float sliderPosX = 0.0f; // (desc->width - (desc->height * kSliderAspectRatio))* desc->internalValue;
		float sliderPosZ = 2.0f;
		//		vec4 sliderPos = desc->position + xAxis * sliderPosX + zAxis * 0.001f * sliderPosZ;
		vec4 sliderPos = panelPos + xAxis * (desc->x + sliderPosX) + yAxis * desc->y + zAxis * 0.001f * sliderPosZ;

		int32_t buttonQuad = desc->buttonQuad = rgui_quad(&(rgui_quadDesc) {
			.x = desc->x + sliderPosX,
			.y = desc->y,
			.z = sliderPosZ,
			.color = (vec4){ 0.9f, 0.9f, 0.9f, 1.0f },
			.collisionColor = (vec4){ 1.0f, 1.0f, 1.0f, 1.0f },
			.width = desc->width, // *kSliderAspectRatio,
			.height = desc->height,
			.texture = desc->texture,
			.panel = desc->panel,
			.layoutFlags = desc->layoutFlags,
			.flags = desc->bDisabled == true ? kGreyedOut : 0,
		});
		sb_push(s_guiData.buttonDescArray, *desc);
		buttonIndex = sb_count(s_guiData.buttonDescArray) - 1;

		//		if (desc->flags & kCollidable)  //CLR - Why is this here?
		{
			if (desc->layoutFlags & kAlignHCenter) {
				sliderPos -= xAxis * desc->width * 0.5f;
			}
			float halfHeight = desc->height * 0.5f;
			float hackHeight = halfHeight * 0.4f;						// CLR - Temporarily hack button collider height as image isn't square but texture is.
			vec4 p0 = sliderPos + yAxis * (halfHeight - hackHeight);
			vec4 p1 = sliderPos + yAxis * (halfHeight - hackHeight) + xAxis * desc->width;
			vec4 p2 = sliderPos + yAxis * (halfHeight + hackHeight);
			vec4 p3 = sliderPos + yAxis * (halfHeight + hackHeight) + xAxis * desc->width;
/*
			plane quadPlane = plane_init(p0, p1, p2);
			vec4 rayP1 = s_guiData.frameParams.pointerRayStartPosition;
			vec4 rayP2 = s_guiData.frameParams.pointerRayEndPosition;
			vec4 rayVec = s_guiData.frameParams.pointerRayVec;
			vec4 planeNormal = quadPlane * (vec4) { 1.0f, 1.0f, 1.0f, 0.0f };
			float p1DotN = vec4_dot(rayP1, planeNormal).x;
			float numer = quadPlane.w - p1DotN;
			float denom = vec4_dot(rayVec, planeNormal).x;
			float t = numer / denom;
			vec4 xPos = vec4_lerp(rayP1, rayP2, t);

			bool bAlreadySelected = false;
			if (bAlreadySelected || pointInTriangle(xPos, p0, p1, p2) || pointInTriangle(xPos, p2, p1, p3))
			{
				s_guiData.texturedQuadArray[buttonQuad].color = (vec4){ 1.0f, 1.0f, 1.0f, 1.0f };
				if (s_guiData.frameParams.controllerTrigger > 0.1f)
				{
//					desc->action();
					pressed = true;
				}
			}
*/
			rgui_collisionQuad quad = {
				.points[0] = { p0.x, p0.y, p0.z },
				.points[1] = { p1.x, p1.y, p1.z },
				.points[2] = { p2.x, p2.y, p2.z },
				.points[3] = { p3.x, p3.y, p3.z },
				.action = Block_copy(desc->action),
				.widgetType = kButtonWidget,
				.widgetIndex = buttonIndex,
				.panelHash = panelHash,
				.bDisabled = desc->bDisabled,
			};
			sb_push(s_guiData.collisionQuadArray, quad);

		}
//		sb_push(s_guiData.buttonDescArray, *desc);
	}
skip:
	return pressed; // buttonIndex;
#if 0
	rgfx_instancedMeshInfo* meshInfo = NULL;
	rgfx_instanceBatchInfo* batch = NULL;

	int32_t index = rsys_hm_find(&s_guiData.batchesLookup, desc->meshNameHash);
	if (index < 0)
	{
		index = sb_count(s_guiData.instancedMeshArray);
		rsys_hm_insert(&s_guiData.batchesLookup, desc->meshNameHash, index);
		Mesh* mesh = NULL;
		if (desc->meshNameHash) {
			mesh = rres_findMesh(desc->meshNameHash);	// CLR - Only search for a mesh, if we have a meshName (i.e. loaded from file).
		}
		if (mesh)
		{
			if (mesh->numRenderables > 1 && (mesh->versionMajor > 0 || (mesh->versionMajor == 0 && mesh->versionMinor > 2)))
				assert(0); // return (rgfx_meshInstance) { 0 };

			//			mesh->numRenderables = 1;
			meshInfo = sb_add(s_guiData.instancedMeshArray, 1);
			meshInfo->batchCount = mesh->numRenderables;
			meshInfo->firstBatch = s_guiData.batchArray != NULL ? sb_count(s_guiData.batchArray) : 0;
			for (uint32_t i = 0; i < mesh->numRenderables; ++i)
			{
				batch = sb_add(s_guiData.batchArray, 1);
				batch->command = (DrawElementsIndirectCommand){
					.count = mesh->renderables[i].indexCount,
					.firstIndex = mesh->renderables[i].firstIndex * 2,
					.baseVertex = mesh->renderables[i].firstVertex,
				};
				//				batch->instanceMatrices = NULL;
				batch->matricesBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.capacity = 1024,
					.stride = sizeof(mat4x4),
					.flags = kMapWriteBit
				});
				batch->materialsBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.capacity = 512,
					.stride = sizeof(MaterialProperties),
					.flags = kMapWriteBit
				});
				rgfx_materialFlags materialFlags = mesh->materials[mesh->renderables[i].materialIndex].flags;
				if (materialFlags & kMFSkinned)
				{
					assert(0);
				}
				batch->albedoMap = mesh->materials[mesh->renderables[i].materialIndex].albedoMap;
				batch->normalMap = mesh->materials[mesh->renderables[i].materialIndex].normalMap;
/*
				batch->metallicMap = mesh->materials[mesh->renderables[i].materialIndex].metallicMap;
				batch->roughnessMap = mesh->materials[mesh->renderables[i].materialIndex].roughnessMap;
				batch->emissiveMap = mesh->materials[mesh->renderables[i].materialIndex].emissiveMap;
*/
				batch->albedo[0] = mesh->materials[mesh->renderables[i].materialIndex].albedo[0];
				batch->albedo[1] = mesh->materials[mesh->renderables[i].materialIndex].albedo[1];
				batch->albedo[2] = mesh->materials[mesh->renderables[i].materialIndex].albedo[2];
				batch->albedo[3] = mesh->materials[mesh->renderables[i].materialIndex].albedo[3];
				batch->roughness = mesh->materials[mesh->renderables[i].materialIndex].roughness;
				batch->materialFlags = mesh->materials[mesh->renderables[i].materialIndex].flags;
				batch->materialNameHash = mesh->materials[mesh->renderables[i].materialIndex].nameHash;
			}
		}
		else
		{
#if 0
			//
			int32_t meshIdx = rsys_hm_find(&s_rendererData.meshLookup, meshNameHash);
			assert(meshIdx >= 0);
			rgfx_meshInfo* mesh = &s_rendererData.meshArray[meshIdx];

			meshInfo = sb_add(s_rendererData.instanceMeshes, 1);
			meshInfo->batchCount = mesh->numRenderables;
			meshInfo->firstBatch = s_rendererData.batches != NULL ? sb_count(s_rendererData.batches) : 0;
			int32_t renderableIdx = mesh->firstRenderable.id - 1;
			assert(renderableIdx >= 0);
			for (uint32_t i = 0; i < mesh->numRenderables; ++i)
			{
				int32_t currentRenderableIdx = renderableIdx + i;
				batch = sb_add(s_rendererData.batches, 1);
				batch->command = (DrawElementsIndirectCommand){
					.count = s_rendererData.renderableArray[currentRenderableIdx].indexCount,
					.firstIndex = s_rendererData.renderableArray[currentRenderableIdx].firstIndex * 2,
					.baseVertex = s_rendererData.renderableArray[currentRenderableIdx].firstVertex,
				};
				//				batch->instanceMatrices = NULL;
				batch->matricesBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.capacity = 16384,
					.stride = sizeof(mat4x4),
					.flags = kMapWriteBit
				});
#if 0
				batch->materialsBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.capacity = 16384,
					.stride = sizeof(MaterialProperties),
					.flags = kMapWriteBit
				});
				rgfx_materialFlags materialFlags = mesh->materials[mesh->renderables[i].materialIndex].flags;
				if (materialFlags & kMFSkinned)
				{
					batch->skinningBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
						.capacity = sizeof(mat4x4) * 20,
						.stride = sizeof(mat4x4),
						.flags = kMapWriteBit
					});
				}
				batch->albedoMap = mesh->materials[mesh->renderables[i].materialIndex].albedoMap;
				batch->normalMap = mesh->materials[mesh->renderables[i].materialIndex].normalMap;
				batch->metallicMap = mesh->materials[mesh->renderables[i].materialIndex].metallicMap;
				batch->roughnessMap = mesh->materials[mesh->renderables[i].materialIndex].roughnessMap;
				batch->emissiveMap = mesh->materials[mesh->renderables[i].materialIndex].emissiveMap;
				batch->albedo[0] = mesh->materials[mesh->renderables[i].materialIndex].albedo[0];
				batch->albedo[1] = mesh->materials[mesh->renderables[i].materialIndex].albedo[1];
				batch->albedo[2] = mesh->materials[mesh->renderables[i].materialIndex].albedo[2];
				batch->albedo[3] = mesh->materials[mesh->renderables[i].materialIndex].albedo[3];
				batch->roughness = mesh->materials[mesh->renderables[i].materialIndex].roughness;
				batch->materialFlags = mesh->materials[mesh->renderables[i].materialIndex].flags;
				batch->materialNameHash = mesh->materials[mesh->renderables[i].materialIndex].nameHash;
#endif
			}
#endif
		}
	}
	meshInfo = &s_guiData.instancedMeshArray[index];

//	mat4x4 transformMatrix = mat4x4_create(desc->rotation, desc->position);
//	mat4x4 scaleMatrix = mat4x4_scale(desc->scale);
	mat4x4 instanceMatrix = desc->transform; // mat4x4_mul2(&transformMatrix, &scaleMatrix);

	bool xAxisZero = vec4_isZero(instanceMatrix.xAxis);
	bool yAxisZero = vec4_isZero(instanceMatrix.yAxis);
	bool zAxisZero = vec4_isZero(instanceMatrix.zAxis);
	bool wAxisZero = vec4_isZero(instanceMatrix.wAxis);

	for (int32_t i = 0; i < meshInfo->batchCount; ++i)
	{
		int32_t batchIndex = meshInfo->firstBatch + i;
		batch = &s_guiData.batchArray[batchIndex];

		mat4x4* mappedPtr = (mat4x4*)rgfx_mapBufferRange(batch->matricesBuffer, sizeof(mat4x4)* batch->command.instanceCount, sizeof(mat4x4));
		if (mappedPtr)
		{
			mappedPtr->xAxis = xAxisZero ? (vec4) { 1.0f, 0.0f, 0.0f, 0.0f } : instanceMatrix.xAxis;
			mappedPtr->yAxis = yAxisZero ? (vec4) { 0.0f, 1.0f, 0.0f, 0.0f } : instanceMatrix.yAxis;
			mappedPtr->zAxis = zAxisZero ? (vec4) { 0.0f, 0.0f, 1.0f, 0.0f } : instanceMatrix.zAxis;
			mappedPtr->wAxis = wAxisZero ? (vec4) { 0.0f, 0.0f, 0.0f, 1.0f } : instanceMatrix.wAxis;
			rgfx_unmapBuffer(batch->matricesBuffer);
		}

		MaterialProperties* material = (MaterialProperties*)rgfx_mapBufferRange(batch->materialsBuffer, sizeof(MaterialProperties) * batch->command.instanceCount, sizeof(MaterialProperties));
		if (material)
		{
			memset(material, 0, sizeof(MaterialProperties));
			material->diffuse[0] = batch->albedo[0];
			material->diffuse[1] = batch->albedo[1];
			material->diffuse[2] = batch->albedo[2];
			material->diffuse[3] = batch->albedo[3];
			material->specularPower = batch->roughness;
			rgfx_unmapBuffer(batch->materialsBuffer);
		}
		batch->command.instanceCount++;
	}
#endif
	return false;
}

rgui_panel rgui_beginPanel(const rgui_panelDesc* desc)
{
	rgui_panel panel = { .id = 0 };

	if (desc == NULL)
		goto skip;

	if (desc->width > 0.0f && desc->height > 0.0f)
	{
		sb_push(s_guiData.panelArray, *desc);
		panel.id = sb_count(s_guiData.panelArray);
		s_guiData.panelArray[panel.id-1].panelHash = hashString(desc->bannerText);	//CLR - (Hack) Due to const, we're not able to modify 'desc' directly to modify the panelDsec after it's been pushed.
// Main Quad
		rgui_quad(&(rgui_quadDesc) {
			.color = desc->color,
			.width = desc->width,
			.height = desc->height - desc->bannerHeight,
			.texture = desc->texture,
			.panel = panel,
			.flags = desc->flags,
			.layoutFlags = desc->layoutFlags,
		});

// Banner Quad
		rgui_quad(&(rgui_quadDesc) {
			.color = desc->bannerColor,
			.x = 0.0f,
			.y = desc->height - desc->bannerHeight,
			.width = desc->width,
			.height = desc->bannerHeight,
			.panel = panel,
			.layoutFlags = desc->layoutFlags,
		});

		rgui_text3D(&(rgui_textDesc) {
//			.rotation = { s_gameData.guiTransformMain.xAxis, s_gameData.guiTransformMain.yAxis, s_gameData.guiTransformMain.zAxis },
//			.position = s_gameData.guiTransformMain.wAxis + s_gameData.guiTransformMain.yAxis * menuBannerPosY + s_gameData.guiTransformMain.zAxis * 0.002f,
			.x = 0.0f,
			.y = desc->height - desc->bannerHeight,
			.z = 1.0f,
			.scale = desc->bannerHeight * 1000.0f,
			.text = desc->bannerText,
			.font = desc->bannerFont,
			.panel = panel,
			.layoutFlags = desc->layoutFlags | kAlignVCenterHeading,
		});

		if (desc->flags & kCollidable)
		{
			vec4 curPos = desc->position;
			vec4 xDelta = desc->rotation.xAxis;
			vec4 yDelta = desc->rotation.yAxis;
			float width = desc->width;
			float height = desc->height;
			if (desc->layoutFlags & kAlignHCenter) {
				curPos -= xDelta * width * 0.5f;
			}

			vec4 p0 = curPos;
			vec4 p1 = curPos + xDelta * width;
			vec4 p2 = curPos + yDelta * height;
			vec4 p3 = curPos + yDelta * height + xDelta * width;

			rgui_collisionQuad quad = {
				.points[0] = { p0.x, p0.y, p0.z },
				.points[1] = { p1.x, p1.y, p1.z },
				.points[2] = { p2.x, p2.y, p2.z },
				.points[3] = { p3.x, p3.y, p3.z },
				.widgetType = kPanelWidget,
				.widgetIndex = panel.id - 1,
			};
			sb_push(s_guiData.collisionQuadArray, quad);
		}
	}
skip:
	return panel;
}

void rgui_endPanel(void)
{
}

rgui_texture rgui_findTexture(const char* textureName)
{
	int32_t hash = hashString(textureName);
	int32_t index = rsys_hm_find(&s_guiData.textureSet.textureLookup, hash);

	return (rgui_texture) { .id = index + 1 };
}

int32_t rgui_quad(const rgui_quadDesc* desc)
{
	int32_t quadIndex = -1;

	if (desc == NULL)
		goto skip;

	if (desc->width > 0.0f && desc->height > 0.0f)
	{
		if (desc->texture.id == 0) 
		{
			sb_push(s_guiData.untexturedQuadArray, *desc);
			quadIndex = sb_count(s_guiData.untexturedQuadArray) - 1;
		} else
		{
			sb_push(s_guiData.texturedQuadArray, *desc);
			quadIndex = sb_count(s_guiData.texturedQuadArray) - 1;
		}
	}
skip:
	return quadIndex;
}

int32_t rgui_pointer(const rgui_pointerDesc* desc)
{
	int32_t pointerIndex = -1;

	if (desc == NULL)
		goto skip;

	if (desc->thickness > 0.0f)
	{
		sb_push(s_guiData.pointerDescArray, *desc);
		pointerIndex = sb_count(s_guiData.pointerDescArray) - 1;
	}
skip:
	return pointerIndex;
}

void rgui_mouseRay(vec4 pos0, vec4 pos1)
{
	s_guiData.mouseRayP1 = pos0;
	s_guiData.mouseRayP2 = pos1;
}

void rgui_registerTextureSet(const char** textureNames, int32_t numTextures)
{
	s_guiData.textureSet.texture = rgfx_createTextureArray(textureNames, numTextures);
	for (int32_t tex = 0; tex < numTextures; ++tex)
	{
		uint32_t hash = hashString(textureNames[tex]);
		rsys_hm_insert(&s_guiData.textureSet.textureLookup, hash, tex);
	}
}

int32_t rgui_slider(rgui_sliderDesc* desc)
{
	int32_t sliderIndex = -1;

	if (desc == NULL || desc->panel.id == 0)
		goto skip;

	if (desc->action != NULL)
	{
		desc->action = Block_copy(desc->action);
	}
	if (desc->width > 0.0f && desc->height > 0.0f)
	{
//		vec4 xAxis = desc->rotation.xAxis;
//		vec4 yAxis = desc->rotation.yAxis;
//		vec4 zAxis = desc->rotation.zAxis;
//		vec4 wAxis = desc->position;
		int32_t panelIndex = desc->panel.id - 1;
		assert(panelIndex >= 0);
		vec4 xAxis = s_guiData.panelArray[panelIndex].rotation.xAxis;
		vec4 yAxis = s_guiData.panelArray[panelIndex].rotation.yAxis;
		vec4 zAxis = s_guiData.panelArray[panelIndex].rotation.zAxis;
		vec4 panelPos = s_guiData.panelArray[panelIndex].position;
		uint32_t panelHash = s_guiData.panelArray[panelIndex].panelHash;

		if (desc->minValue == 0.0f && desc->maxValue == 0.0f)
		{
			assert(*desc->value >= 0.0f && *desc->value <= 1.0f);
			desc->maxValue = 1.0f;
		}
		desc->internalValue = (*desc->value - desc->minValue) / (desc->maxValue - desc->minValue);
		rgui_quad(&(rgui_quadDesc) {
			.x = desc->x,
			.y = desc->y + ((desc->height - desc->thickness) * 0.5f),
			.z = 1.0f,
			.color = (vec4){ 0.4f, 0.4f, 0.7f, 1.0f },
			.width = desc->width,
			.height = desc->thickness,
			.panel = desc->panel,
			.layoutFlags = desc->layoutFlags,
		});
//		float sliderPosX = (desc->position.x + (desc->width - (desc->height * kSliderAspectRatio)) - desc->position.x) * desc->internalValue;
		float sliderPosX = (desc->width - (desc->height * kSliderAspectRatio)) * desc->internalValue;
		float sliderPosZ = 2.0f;
//		vec4 sliderPos = desc->position + xAxis * sliderPosX + zAxis * 0.001f * sliderPosZ;
		vec4 sliderPos = panelPos + xAxis * (desc->x + sliderPosX) + yAxis * desc->y + zAxis * 0.001f * sliderPosZ;

		desc->sliderPanel = rgui_quad(&(rgui_quadDesc) {
			.x = desc->x + sliderPosX,
			.y = desc->y,
			.z = sliderPosZ,
			.color = (vec4){ 0.9f, 0.9f, 0.9f, 1.0f },
			.collisionColor = (vec4){ 1.0f, 1.0f, 1.0f, 1.0f },
			.width = desc->height * kSliderAspectRatio,
			.height = desc->height,
			.texture = desc->sliderTexture,
			.panel = desc->panel,
			.layoutFlags = desc->layoutFlags,
//			.flags = kCollidable,
		});
		sb_push(s_guiData.sliderDescArray, *desc);
		sliderIndex = sb_count(s_guiData.sliderDescArray) - 1;

//		if (desc->flags & kCollidable)  //CLR - Why is this here?
		{
			vec4 p0 = sliderPos;
			vec4 p1 = sliderPos + xAxis * desc->height * kSliderAspectRatio;
			vec4 p2 = sliderPos + yAxis * desc->height;
			vec4 p3 = sliderPos + yAxis * desc->height + xAxis * desc->height * kSliderAspectRatio;

			rgui_collisionQuad quad = {
				.points[0] = { p0.x, p0.y, p0.z },
				.points[1] = { p1.x, p1.y, p1.z },
				.points[2] = { p2.x, p2.y, p2.z },
				.points[3] = { p3.x, p3.y, p3.z },
				.widgetType = kSliderWidget,
				.widgetIndex = sliderIndex,
				.panelHash = panelHash,
			};
			sb_push(s_guiData.collisionQuadArray, quad);
		}
	}
skip:
	return sliderIndex;
}

void rgui_updateSlider(int32_t sliderIndex, uint32_t panelHash, vec4 pointerPos, bool triggerPressed)
{
	if (triggerPressed)
	{
		if (s_guiData.selectedWidgetIndex == -1)
		{
//			rsys_print("Slider %d selected.\n", sliderIndex);
			s_guiData.selectedWidgetIndex = sliderIndex;
			s_guiData.selectedWidgetType = kSliderWidget;
			s_guiData.selectedWidgetPanelHash = panelHash;
			s_guiData.prevPointerPos = pointerPos;
		}
		else
		{
//			rsys_print("Slider %d startX = %.3f, curPosX = %.3f.\n", sliderIndex, s_guiData.prevPointerPos.x, pointerPos.x);

			rgui_sliderDesc desc = s_guiData.sliderDescArray[sliderIndex];

			int32_t panelIndex = s_guiData.sliderDescArray[sliderIndex].panel.id - 1;
			assert(panelIndex >= 0);
			vec4 xAxis = s_guiData.panelArray[panelIndex].rotation.xAxis;
			vec4 yAxis = s_guiData.panelArray[panelIndex].rotation.yAxis;
			vec4 zAxis = s_guiData.panelArray[panelIndex].rotation.zAxis;
			vec4 panelPos = s_guiData.panelArray[panelIndex].position;

			float x = s_guiData.sliderDescArray[sliderIndex].x;
			float y = s_guiData.sliderDescArray[sliderIndex].y;
			float width = s_guiData.sliderDescArray[sliderIndex].width;
			float height = s_guiData.sliderDescArray[sliderIndex].height;
			float value = s_guiData.sliderDescArray[sliderIndex].internalValue;
			float sliderPosX = (width - (height * kSliderAspectRatio)) * value;

			vec4 baseSliderPos = panelPos + xAxis * x;
			vec4 currentSliderPos = baseSliderPos + xAxis * sliderPosX;
			vec4 pointerRelSlider = s_guiData.prevPointerPos - currentSliderPos;
			vec4 newSliderPos = pointerPos - pointerRelSlider;
			newSliderPos = newSliderPos - baseSliderPos;

			float newSliderPosX = vec4_dot(newSliderPos, xAxis).x;
			float newValue = newSliderPosX / (width - (height * kSliderAspectRatio));

			if (newValue >= 0.0f && newValue <= 1.0f)
			{
				s_guiData.sliderDescArray[sliderIndex].internalValue = newValue;
				float remappedValue = desc.minValue * (1.0f - newValue) + desc.maxValue * newValue;
				float currentValue = *desc.value;
				float delta = remappedValue - currentValue;
				if (desc.step == 0.0f)
				{
					*s_guiData.sliderDescArray[sliderIndex].value = remappedValue;
					s_guiData.prevPointerPos = pointerPos;
					if (desc.action != NULL)
					{
						desc.action();
						Block_release(desc.action);
						desc.action = NULL;
					}
				}
				else if (fabsf(delta) > (desc.step * 0.5f))
				{
					*s_guiData.sliderDescArray[sliderIndex].value = delta > 0.0f ? ceilf(remappedValue) : floorf(remappedValue);
					s_guiData.prevPointerPos = pointerPos;
					if (desc.action != NULL)
					{
						desc.action();
						Block_release(desc.action);
						desc.action = NULL;
					}
				}
			}
		}
	}
}


int32_t rgui_text3D(const rgui_textDesc* desc)
{
	int32_t textIndex = -1;

	if (desc == NULL)
		goto skip;

	if (desc->text)
	{
		s_guiData.totalNumChars += strlen(desc->text);
		sb_push(s_guiData.textDescArray, *desc);
		textIndex = sb_count(s_guiData.textDescArray) - 1;
	}
skip:
	return textIndex;
}

void rgui_endFrame(void)
{
#if 0
	int32_t batchCount = sb_count(s_guiData.batchArray);
	for (int32_t batchIdx = 0; batchIdx < batchCount; ++batchIdx)
	{
		rgfx_instanceBatchInfo* batch = &s_guiData.batchArray[batchIdx];
		batch->command.instanceCount = 0;
	}
#endif
	sb_free(s_guiData.panelArray);
	s_guiData.panelArray = NULL;
	sb_free(s_guiData.collisionQuadArray);
	s_guiData.collisionQuadArray = NULL;
	sb_free(s_guiData.untexturedQuadArray);
	s_guiData.untexturedQuadArray = NULL;
	sb_free(s_guiData.pointerDescArray);
	s_guiData.pointerDescArray = NULL;
	sb_free(s_guiData.sliderDescArray);
	s_guiData.sliderDescArray = NULL;
	sb_free(s_guiData.textDescArray);
	s_guiData.textDescArray = NULL;
	sb_free(s_guiData.texturedQuadArray);
	s_guiData.texturedQuadArray = NULL;
	sb_free(s_guiData.buttonDescArray);
	s_guiData.buttonDescArray = NULL;
	s_guiData.totalNumChars = 0;
}

bool pointInTriangle(vec4 p, vec4 a, vec4 b, vec4 c)
{
	a -= p;
	b -= p;
	c -= p;
	float ab = vec4_dot(a, b).x;
	float ac = vec4_dot(a, c).x;
	float bc = vec4_dot(b, c).x;
	float cc = vec4_dot(c, c).x;

	if (bc * ac - cc * ab < 0.0f)
		return false;

	float bb = vec4_dot(b, b).x;
	if (ab * bc - ac * bb < 0.0f)
		return false;

	return true;
}

void rgui_update(bool triggerPressed)
{
#if 1
	int32_t numPointers = sb_count(s_guiData.pointerDescArray);
//	if (numPointers > 0)
	{
		vec4 rayP1;
		vec4 rayP2;
		if (numPointers > 0) {
			rayP1 = s_guiData.pointerDescArray[0].startPosition;
			rayP2 = s_guiData.pointerDescArray[0].endPosition;
		}
		else
		{
			rayP1 = s_guiData.mouseRayP1;
			rayP2 = s_guiData.mouseRayP2;
		}
		int32_t numCollisionQuads = sb_count(s_guiData.collisionQuadArray);
		for (int32_t quad = 0; quad < numCollisionQuads; ++quad)
		{
			if (s_guiData.collisionQuadArray[quad].bDisabled)
				continue;

			vec4 p0 = { s_guiData.collisionQuadArray[quad].points[0].x, s_guiData.collisionQuadArray[quad].points[0].y, s_guiData.collisionQuadArray[quad].points[0].z, 1.0f };
			vec4 p1 = { s_guiData.collisionQuadArray[quad].points[1].x, s_guiData.collisionQuadArray[quad].points[1].y, s_guiData.collisionQuadArray[quad].points[1].z, 1.0f };
			vec4 p2 = { s_guiData.collisionQuadArray[quad].points[2].x, s_guiData.collisionQuadArray[quad].points[2].y, s_guiData.collisionQuadArray[quad].points[2].z, 1.0f };
			vec4 p3 = { s_guiData.collisionQuadArray[quad].points[3].x, s_guiData.collisionQuadArray[quad].points[3].y, s_guiData.collisionQuadArray[quad].points[3].z, 1.0f };
			plane quadPlane = plane_init(p0, p1, p2);

			vec4 planeNormal = quadPlane * (vec4){ 1.0f, 1.0f, 1.0f, 0.0f };
			float p1DotN = vec4_dot(rayP1, planeNormal).x;
			float numer = quadPlane.w - p1DotN;
			vec4 rayVec = rayP2 - rayP1;
			float denom = vec4_dot(rayVec, planeNormal).x;
			float t = numer / denom;
			vec4 xPos = vec4_lerp(rayP1, rayP2, t);
			bool bAlreadySelected = false;
			if ((s_guiData.collisionQuadArray[quad].widgetType == s_guiData.selectedWidgetType && s_guiData.selectedWidgetIndex == s_guiData.collisionQuadArray[quad].widgetIndex && s_guiData.selectedWidgetPanelHash == s_guiData.collisionQuadArray[quad].panelHash))
			{
				bAlreadySelected = true;
			}
			bool bNothingWasSelected = s_guiData.selectedWidgetIndex == -1 && bAlreadySelected == false;
			if (bAlreadySelected || ((pointInTriangle(xPos, p0, p1, p2) || pointInTriangle(xPos, p2, p1, p3)) && bNothingWasSelected))
			{
				//CLR - We only use pointers in VR, in 2D we have an imaginary pointer.
				if (numPointers > 0)
				{
					s_guiData.pointerDescArray[0].endPosition = xPos;
				}
#if 1
//				assert(s_guiData.collisionQuadArray[quad].widgetIndex >= 0);
				if (s_guiData.collisionQuadArray[quad].widgetIndex >= 0)
				{
					int32_t widgetIndex = s_guiData.collisionQuadArray[quad].widgetIndex;
					int32_t panelHash = s_guiData.collisionQuadArray[quad].panelHash;
					switch (s_guiData.collisionQuadArray[quad].widgetType)
					{
					case kPanelWidget:
						if (!vec4_isZero(s_guiData.untexturedQuadArray[widgetIndex].collisionColor))
						{
							s_guiData.untexturedQuadArray[widgetIndex].color = s_guiData.untexturedQuadArray[widgetIndex].collisionColor;
						}
						break;
					case kSliderWidget:
					{
						int32_t sliderPanel = s_guiData.sliderDescArray[widgetIndex].sliderPanel;
						if (!vec4_isZero(s_guiData.texturedQuadArray[sliderPanel].collisionColor))
						{
							s_guiData.texturedQuadArray[sliderPanel].color = s_guiData.texturedQuadArray[sliderPanel].collisionColor;
						}
						rgui_updateSlider(widgetIndex, panelHash, xPos, triggerPressed);
						break;
					}
					case kTexturedQuad:
						if (!vec4_isZero(s_guiData.texturedQuadArray[widgetIndex].collisionColor))
						{
							//						s_guiData.panelDescArray[widgetIndex].color = s_guiData.panelDescArray[widgetIndex].collisionColor;
						}
						break;
					case kButtonWidget:
					{
						int32_t buttonQuad = s_guiData.buttonDescArray[widgetIndex].buttonQuad;
						if (!vec4_isZero(s_guiData.texturedQuadArray[buttonQuad].collisionColor))
						{
							s_guiData.texturedQuadArray[buttonQuad].color = s_guiData.texturedQuadArray[buttonQuad].collisionColor;
						}
						if (s_guiData.collisionQuadArray[quad].action != NULL)
						{
							if (triggerPressed) {
								s_guiData.selectedWidgetType = kButtonWidget;
								s_guiData.selectedWidgetIndex = widgetIndex;
								s_guiData.selectedWidgetPanelHash = panelHash;
								if (!bAlreadySelected) {
									s_guiData.collisionQuadArray[quad].action();
								}
							}
							Block_release(s_guiData.collisionQuadArray[quad].action);
							s_guiData.collisionQuadArray[quad].action = NULL;
						}
						break;
					}
					default:
						break;
					}
				}
#endif
			}
		}
	}
//	sb_free(s_guiData.collisionQuadArray);
//	s_guiData.collisionQuadArray = NULL;

	if (triggerPressed == false)
	{
		if (s_guiData.selectedWidgetIndex >= 0)
		{
//			rsys_print("Slider %d deselected.\n", s_guiData.selectedSliderIndex);
			s_guiData.selectedWidgetIndex = -1;
			s_guiData.selectedWidgetPanelHash = 0;
		}
	}
#endif
}

void rgui_prepareIndexBuffer(rgfx_vertexBuffer vb, int32_t numQuads)
{
	rgfx_buffer ib = rgfx_getVertexBufferBuffer(vb, kIndexBuffer);
	uint16_t* idxData = (uint16_t*)rgfx_mapBufferRange(ib, 0, sizeof(int16_t) * INDICES_PER_QUAD * numQuads);

	int32_t vertexCount = 0;
	int32_t indexCount = 0;
	for (int32_t i = 0; i < numQuads; ++i)
	{
#ifdef TRIANGLE_STRIP_QUADS
		static const uint16_t kIndicesTemplate[INDICES_PER_QUAD] =
		{
			0,  1, 2, 3, 0xffff
		};
#else
		static const uint16_t kIndicesTemplate[INDICES_PER_QUAD] =
		{
			0,  1, 2,
			1,  3, 2
		};
#endif
		for (int32_t idx = 0; idx < INDICES_PER_QUAD; ++idx)
		{
#ifdef TRIANGLE_STRIP_QUADS
			*(idxData++) = kIndicesTemplate[idx] != 0xffff ? vertexCount + kIndicesTemplate[idx] : kIndicesTemplate[idx];
#else
			*(idxData++) = vertexCount + kIndicesTemplate[idx];
#endif
		}
		vertexCount += 4;
	}
	rgfx_unmapBuffer(ib);
}

extern GLint g_viewIdLoc;
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

void rgui_render(int32_t eye, bool bHmdMounted)
{
	if (s_guiData.panelPipeline.id == 0)
	{
		s_guiData.panelPipeline = rgfx_createPipeline(&(rgfx_pipelineDesc) {
			.vertexShader = rgfx_loadShader("shaders/gui.vert", kVertexShader, 0),
			.fragmentShader = rgfx_loadShader("shaders/gui.frag", kFragmentShader, 0),
		});

		s_guiData.texturedQuadPipeline = rgfx_createPipeline(&(rgfx_pipelineDesc) {
			.vertexShader = rgfx_loadShader("shaders/gui_utu.vert", kVertexShader, 0),
			.fragmentShader = rgfx_loadShader("shaders/gui_utu.frag", kFragmentShader, 0),
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
			int32_t vertexCapacity = MAX_UNTEXTURED_QUADS * VERTICES_PER_QUAD * vertexStride;
			int32_t indexCapacity = MAX_UNTEXTURED_QUADS * INDICES_PER_QUAD * sizeof(uint16_t);
			s_guiData.panelVertexBuffer = rgfx_createVertexBuffer(&(rgfx_vertexBufferDesc) {
				.format = vertexFormat,
				.capacity = vertexCapacity,
				.indexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
				.type = kIndexBuffer,
					.capacity = indexCapacity,
					.stride = sizeof(uint16_t),
					.flags = kMapWriteBit | kDynamicStorageBit
				})
			});
			rgui_prepareIndexBuffer(s_guiData.panelVertexBuffer, MAX_UNTEXTURED_QUADS);
		}
	}

	if (s_guiData.textPipeline.id == 0)
	{
		s_guiData.textPipeline = rgfx_createPipeline(&(rgfx_pipelineDesc) {
			.vertexShader = rgfx_loadShader("shaders/sdf_font.vert", kVertexShader, 0),
			.fragmentShader = rgfx_loadShader("shaders/sdf_font.frag", kFragmentShader, 0),
		});

		uint32_t vertexStride = 0;
		{
			rgfx_vertexElement vertexStreamElements[] =
			{
				{ kFloat, 3, false },
				{ kFloat, 2, false },
				{ kUnsignedByte, 4, true },
			};
			int32_t numElements = STATIC_ARRAY_SIZE(vertexStreamElements);
			rgfx_vertexFormat vertexFormat = rgfx_registerVertexFormat(numElements, vertexStreamElements);
			vertexStride = rgfx_getVertexFormatInfo(vertexFormat)->stride;
			int32_t vertexCapacity = MAX_TEXTURED_QUADS * VERTICES_PER_QUAD * vertexStride;
			int32_t indexCapacity = MAX_TEXTURED_QUADS * INDICES_PER_QUAD * sizeof(uint16_t);
			s_guiData.texturedQuadVertexBuffer = rgfx_createVertexBuffer(&(rgfx_vertexBufferDesc) {
				.format = vertexFormat,
				.capacity = vertexCapacity,
				.indexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.type = kIndexBuffer,
					.capacity = indexCapacity,
					.stride = sizeof(uint16_t),
					.flags = kMapWriteBit | kDynamicStorageBit
				})
			});
			rgui_prepareIndexBuffer(s_guiData.texturedQuadVertexBuffer, MAX_TEXTURED_QUADS);

			s_guiData.texturedQuadIndexStream = rgfx_createBuffer(&(rgfx_bufferDesc) {
				.type = kVertexBuffer,
				.capacity = MAX_TEXTURED_QUADS * VERTICES_PER_QUAD * sizeof(uint8_t),
				.stride = sizeof(uint8_t),
				.flags = kMapWriteBit | kDynamicStorageBit
			});

			vertexCapacity = MAX_TEXT_QUADS * VERTICES_PER_QUAD * vertexStride;
			indexCapacity = MAX_TEXT_QUADS * INDICES_PER_QUAD * sizeof(uint16_t);
			s_guiData.textVertexBuffer = rgfx_createVertexBuffer(&(rgfx_vertexBufferDesc) {
				.format = vertexFormat,
				.capacity = vertexCapacity,
				.indexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.type = kIndexBuffer,
					.capacity = indexCapacity,
					.stride = sizeof(uint16_t),
					.flags = kMapWriteBit | kDynamicStorageBit
				})
			});
			rgui_prepareIndexBuffer(s_guiData.textVertexBuffer, MAX_TEXT_QUADS);
		}
	}
#ifdef TRIANGLE_STRIP_QUADS
	glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
#endif
	rgui_renderQuads(eye, bHmdMounted);
	rgui_renderText(eye, bHmdMounted);
	rgfx_unbindVertexBuffer();
#ifdef TRIANGLE_STRIP_QUADS
	glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
#endif
}

#define GL_SAMPLE_ALPHA_TO_ONE_EXT                     0x809F

void rgui_renderQuads(int32_t eye, bool bHmdMounted)
{
	int32_t numUntexturedQuad = sb_count(s_guiData.untexturedQuadArray);
	int32_t numPointers = sb_count(s_guiData.pointerDescArray);
	int32_t numTexturedQuads = sb_count(s_guiData.texturedQuadArray);
	if (numUntexturedQuad == 0 && numPointers == 0 && numTexturedQuads == 0)
		return;

#ifdef RE_PLATFORM_WIN64
	glDisable(GL_FRAMEBUFFER_SRGB);
#else
	glDisable(GL_FRAMEBUFFER_SRGB);
#endif

	if (numUntexturedQuad || numPointers)
	{
/*
		if (s_guiData.untexturedQuadArray[0].color.a < 1.0f)
		{
			glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			glEnable(GL_SAMPLE_ALPHA_TO_ONE_EXT);
		}
*/
		rgfx_buffer vertexBuffer = rgfx_getVertexBufferBuffer(s_guiData.panelVertexBuffer, kVertexBuffer);
		float* data = (float*)rgfx_mapBuffer(vertexBuffer);

		int32_t indexCount = numUntexturedQuad * INDICES_PER_QUAD;
		for (int32_t quad = 0; quad < numUntexturedQuad; ++quad)
		{
			int32_t panelIndex = s_guiData.untexturedQuadArray[quad].panel.id - 1;
			assert(panelIndex >= 0);
			vec4 curPos = s_guiData.panelArray[panelIndex].position;
			vec4 xDelta = s_guiData.panelArray[panelIndex].rotation.xAxis;
			vec4 yDelta = s_guiData.panelArray[panelIndex].rotation.yAxis;
			vec4 zDelta = s_guiData.panelArray[panelIndex].rotation.zAxis * 0.001f;

			float x = s_guiData.untexturedQuadArray[quad].x;
			float y = s_guiData.untexturedQuadArray[quad].y;
			float z = s_guiData.untexturedQuadArray[quad].z;
			float width = s_guiData.untexturedQuadArray[quad].width;
			float height = s_guiData.untexturedQuadArray[quad].height;
			curPos += xDelta * x + yDelta * y;
			curPos += zDelta * z; // *0.001f; // *x + yDelta * y;
			if (s_guiData.untexturedQuadArray[quad].layoutFlags & kAlignHCenter) {
				curPos -= xDelta * width * 0.5f;
			}
			//		curPos -= xDelta * (widthForString * 0.5f);
			//		curPos -= yDelta * 10.0f; // (fontSize * 0.5f);
			uint32_t r = (uint32_t)(s_guiData.untexturedQuadArray[quad].color.r * 255.0f);
			uint32_t g = (uint32_t)(s_guiData.untexturedQuadArray[quad].color.g * 255.0f);
			uint32_t b = (uint32_t)(s_guiData.untexturedQuadArray[quad].color.b * 255.0f);
			uint32_t a = (uint32_t)(s_guiData.untexturedQuadArray[quad].color.a * 255.0f);

			rgfx_debugColor color = { (a << 24) | (b << 16) | (g << 8) | (r << 0) };

			vec4 p1 = curPos + yDelta * height;
			vec4 p2 = curPos;
			vec4 p3 = curPos + yDelta * height + xDelta * width;
			vec4 p4 = curPos + xDelta * width;
			*(data++) = p1.x;
			*(data++) = p1.y;
			*(data++) = p1.z;
			*((rgfx_debugColor*)data++) = color;
			*(data++) = p2.x;
			*(data++) = p2.y;
			*(data++) = p2.z;
			*((rgfx_debugColor*)data++) = color;
			*(data++) = p3.x;
			*(data++) = p3.y;
			*(data++) = p3.z;
			*((rgfx_debugColor*)data++) = color;
			*(data++) = p4.x;
			*(data++) = p4.y;
			*(data++) = p4.z;
			*((rgfx_debugColor*)data++) = color;
		}

		indexCount += numPointers * INDICES_PER_QUAD;
		for (int32_t pointer = 0; pointer < numPointers; ++pointer)
		{
			vec4 startPos = s_guiData.pointerDescArray[pointer].startPosition;
			vec4 endPos = s_guiData.pointerDescArray[pointer].endPosition;
// CLR - This is effectively a cylindrical billboard.
			vec4 pointerDir = vec4_normalize(endPos - startPos);
			vec4 camDir = vec4_normalize(startPos - s_guiData.frameParams.hmdTransform.wAxis);
			vec4 tangent = vec4_normalize(vec4_cross(pointerDir, camDir));
			float thickness = s_guiData.pointerDescArray[pointer].thickness;
			uint32_t r = (uint32_t)(s_guiData.pointerDescArray[pointer].color.r * 255.0f);
			uint32_t g = (uint32_t)(s_guiData.pointerDescArray[pointer].color.g * 255.0f);
			uint32_t b = (uint32_t)(s_guiData.pointerDescArray[pointer].color.b * 255.0f);
			uint32_t a = (uint32_t)(s_guiData.pointerDescArray[pointer].color.a * 255.0f);

			rgfx_debugColor color = { (a << 24) | (b << 16) | (g << 8) | (r << 0) };

			vec4 p1 = startPos + tangent * (+thickness * 0.5f);
			vec4 p2 = startPos + tangent * (-thickness * 0.5f);
			vec4 p3 = endPos + tangent * (+thickness * 0.5f);
			vec4 p4 = endPos + tangent * (-thickness * 0.5f);
			*(data++) = p1.x;
			*(data++) = p1.y;
			*(data++) = p1.z;
			*((rgfx_debugColor*)data++) = color;
			*(data++) = p2.x;
			*(data++) = p2.y;
			*(data++) = p2.z;
			*((rgfx_debugColor*)data++) = color;
			*(data++) = p3.x;
			*(data++) = p3.y;
			*(data++) = p3.z;
			*((rgfx_debugColor*)data++) = color;
			*(data++) = p4.x;
			*(data++) = p4.y;
			*(data++) = p4.z;
			*((rgfx_debugColor*)data++) = color;
		}
		rgfx_unmapBuffer(vertexBuffer);

		rgfx_bindVertexBuffer(s_guiData.panelVertexBuffer);
		rgfx_bindPipeline(s_guiData.panelPipeline);

		GLuint program = rgfx_getPipelineProgram(s_guiData.panelPipeline, kVertexShader);
		if (g_viewIdLoc >= 0)  // NOTE: will not be present when multiview path is enabled.
		{
			glProgramUniform1i(program, g_viewIdLoc, eye);
		}

		rgfx_bindBuffer(1, rgfx_getTransforms());
#ifdef TRIANGLE_STRIP_QUADS
		rgfx_drawIndexedPrims(kPrimTypeTriangleStrip, indexCount, 0);
#else
		rgfx_drawIndexedPrims(kPrimTypeTriangles, indexCount, 0);
#endif
		rgfx_unbindVertexBuffer();
/*
		if (s_guiData.untexturedQuadArray[0].color.a < 1.0f)
		{
			glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			glDisable(GL_SAMPLE_ALPHA_TO_ONE_EXT);
		}
*/
	}

	if (numTexturedQuads)
	{
		glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

		rgfx_buffer vertexBuffer = rgfx_getVertexBufferBuffer(s_guiData.texturedQuadVertexBuffer, kVertexBuffer);
		float* data = (float*)rgfx_mapBufferRange(vertexBuffer, 0, sizeof(float) * 6 * VERTICES_PER_QUAD * numTexturedQuads);

		//		rgfx_texture guiTexture = s_guiData.texturedQuadArray[0].texture;
		rgfx_texture guiTexture = s_guiData.textureSet.texture;

		int32_t indexCount = numTexturedQuads * INDICES_PER_QUAD;
		for (int32_t quad = 0; quad < numTexturedQuads; ++quad)
		{
			int32_t panelIndex = s_guiData.texturedQuadArray[quad].panel.id - 1;
			assert(panelIndex >= 0);
			vec4 curPos = s_guiData.panelArray[panelIndex].position;
			vec4 xDelta = s_guiData.panelArray[panelIndex].rotation.xAxis;
			vec4 yDelta = s_guiData.panelArray[panelIndex].rotation.yAxis;
			vec4 zDelta = s_guiData.panelArray[panelIndex].rotation.zAxis * 0.001f;

			float x = s_guiData.texturedQuadArray[quad].x;
			float y = s_guiData.texturedQuadArray[quad].y;
			float z = s_guiData.texturedQuadArray[quad].z;
			float width = s_guiData.texturedQuadArray[quad].width;
			float height = s_guiData.texturedQuadArray[quad].height;
			curPos += xDelta * x + yDelta * y;
			curPos += zDelta * z; // *0.001f; // *x + yDelta * y;
			if (s_guiData.texturedQuadArray[quad].layoutFlags & kAlignHCenter) {
				curPos -= xDelta * width * 0.5f;
			}

			uint32_t r = (uint32_t)(s_guiData.texturedQuadArray[quad].color.r * 255.0f);
			uint32_t g = (uint32_t)(s_guiData.texturedQuadArray[quad].color.g * 255.0f);
			uint32_t b = (uint32_t)(s_guiData.texturedQuadArray[quad].color.b * 255.0f);
			uint32_t a = (uint32_t)(s_guiData.texturedQuadArray[quad].color.a * 255.0f);

			rgfx_debugColor color = { (a << 24) | (b << 16) | (g << 8) | (r << 0) };

			vec4 p1 = curPos + yDelta * height;
			vec4 p2 = curPos;
			vec4 p3 = curPos + yDelta * height + xDelta * width;
			vec4 p4 = curPos + xDelta * width;
			*(data++) = p1.x;
			*(data++) = p1.y;
			*(data++) = p1.z;
			*(data++) = 0.0f;
			*(data++) = 1.0f;
			*((rgfx_debugColor*)data++) = color;
			*(data++) = p2.x;
			*(data++) = p2.y;
			*(data++) = p2.z;
			*(data++) = 0.0f;
			*(data++) = 0.0f;
			*((rgfx_debugColor*)data++) = color;
			*(data++) = p3.x;
			*(data++) = p3.y;
			*(data++) = p3.z;
			*(data++) = 1.0f;
			*(data++) = 1.0f;
			*((rgfx_debugColor*)data++) = color;
			*(data++) = p4.x;
			*(data++) = p4.y;
			*(data++) = p4.z;
			*(data++) = 1.0f;
			*(data++) = 0.0f;
			*((rgfx_debugColor*)data++) = color;
		}
		rgfx_unmapBuffer(vertexBuffer);

		uint8_t* indexData = (uint8_t*)rgfx_mapBufferRange(s_guiData.texturedQuadIndexStream, 0, sizeof(uint8_t) * numTexturedQuads * VERTICES_PER_QUAD);
		for (int32_t quad = 0; quad < numTexturedQuads; ++quad)
		{
			int32_t texIndex = s_guiData.texturedQuadArray[quad].texture.id - 1;
			assert(texIndex >= 0);
			if (s_guiData.texturedQuadArray[quad].flags & kGreyedOut)
			{
				texIndex = (texIndex + 1) - 1;
			}
			uint8_t texFlag = (s_guiData.texturedQuadArray[quad].flags & kGreyedOut ? 0x80 : 0x0) | (uint8_t)texIndex;
			for (int32_t vert = 0; vert < VERTICES_PER_QUAD; ++vert)
			{
				*(indexData++) = texFlag;
			}
		}
		rgfx_unmapBuffer(s_guiData.texturedQuadIndexStream);

		rgfx_bindVertexBuffer(s_guiData.texturedQuadVertexBuffer);
		rgfx_bindBuffer(1, s_guiData.texturedQuadIndexStream);
// CLR - HACK: This is a bit hack but no point doing it properly as we should move to using instancing for UI.
		glVertexAttribIPointer(3, 1, GL_UNSIGNED_BYTE, 0, BUFFER_OFFSET(0));
		glEnableVertexAttribArray(3);
// HACK - END
		rgfx_bindPipeline(s_guiData.texturedQuadPipeline);

		GLuint program = rgfx_getPipelineProgram(s_guiData.texturedQuadPipeline, kVertexShader);
		if (g_viewIdLoc >= 0)  // NOTE: will not be present when multiview path is enabled.
		{
			glProgramUniform1i(program, g_viewIdLoc, eye);
		}

		rgfx_bindBuffer(1, rgfx_getTransforms());
		rgfx_bindTexture(0, guiTexture);
#ifdef TRIANGLE_STRIP_QUADS
		rgfx_drawIndexedPrims(kPrimTypeTriangleStrip, indexCount, 0);
#else
		rgfx_drawIndexedPrims(kPrimTypeTriangles, indexCount, 0);
#endif
		// CLR - HACK: This is a bit hack but no point doing it properly as we should move to using instancing for UI.
		glDisableVertexAttribArray(3);
		// HACK - END
		rgfx_unbindVertexBuffer();
	}

#ifdef RE_PLATFORM_WIN64
	glEnable(GL_FRAMEBUFFER_SRGB);
#else
	glEnable(GL_FRAMEBUFFER_SRGB);
#endif
}

void rgui_renderText(int32_t eye, bool bHmdMounted)

{
	int32_t numStrings = sb_count(s_guiData.textDescArray);
	if (numStrings == 0)
		return;

	int32_t indexCount = 0;
	rgfx_buffer vertexBuffer = rgfx_getVertexBufferBuffer(s_guiData.textVertexBuffer, kVertexBuffer);
	int32_t numTexturedQuads = sb_count(s_guiData.texturedQuadArray);
	if (numTexturedQuads == 0)
	{
		glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
	}
	float* data = (float*)rgfx_mapBufferRange(vertexBuffer, 0, sizeof(float) * 6 * VERTICES_PER_QUAD * s_guiData.totalNumChars);

	rgfx_texture fontTexture = rgui_getFontTexture(s_guiData.textDescArray[0].font);
	float fontSize = rgui_getFontSize(s_guiData.textDescArray[0].font);
	float ascent, descent, lineGap;
	rgui_getFontVMetrics(s_guiData.textDescArray[0].font, &ascent, &descent, &lineGap);
	for (int32_t string = 0; string < numStrings; ++string)
	{
		const char* currentString = s_guiData.textDescArray[string].text;
		float scale = s_guiData.textDescArray[string].scale / fontSize;
		int32_t panelIndex = s_guiData.textDescArray[string].panel.id - 1;
		int32_t numChars = strlen(currentString);
		vec4 curPos; // = s_guiData.panelArray[panelIndex].position;
		vec4 xDelta, yDelta;
		vec4 zDelta = { 0.0f, 0.0f, 0.0f, 0.0f };
		if (panelIndex >= 0)
		{
			float x = s_guiData.textDescArray[string].x;
			float y = s_guiData.textDescArray[string].y;
			float z = s_guiData.textDescArray[string].z;
			curPos = s_guiData.panelArray[panelIndex].position;
			xDelta = s_guiData.panelArray[panelIndex].rotation.xAxis;
			yDelta = s_guiData.panelArray[panelIndex].rotation.yAxis;
			zDelta = (s_guiData.panelArray[panelIndex].rotation.zAxis * 0.001f);
			curPos += zDelta + (zDelta * z) + yDelta * y + xDelta * x;
			xDelta *= 0.001f * scale;
			yDelta *= 0.001f * scale;
		}
		else
		{
			curPos = s_guiData.textDescArray[string].position;
			xDelta = (s_guiData.textDescArray[string].rotation.xAxis * 0.001f) * scale;
			yDelta = (s_guiData.textDescArray[string].rotation.yAxis * 0.001f) * scale;
		}
		float widthForString = 0.0f;
		float heightForString = 0.0f;
		for (int32_t ch = 0; ch < numChars; ++ch)
		{
			rgui_fontChar charData;
			rgui_getFontDataForChar(s_guiData.textDescArray[string].font, (int32_t)currentString[ch], &charData);
			widthForString += charData.advance;
			float deltaY = charData.yoff + charData.h;;
			heightForString = deltaY > heightForString ? deltaY : heightForString;
		}
		if (s_guiData.textDescArray[string].layoutFlags & kAlignHCenter) {
			curPos -= xDelta * (widthForString * 0.5f);
		}
		if (s_guiData.textDescArray[string].layoutFlags & kAlignHRight) {
			curPos -= xDelta * (widthForString * 1.0f);
		}
		if (s_guiData.textDescArray[string].layoutFlags & kAlignVCenter) {
			curPos += yDelta * -descent;
		}
		if (s_guiData.textDescArray[string].layoutFlags & kAlignVCenterHeading) {
			curPos += yDelta * 0.5f * -descent;
		}
		//		curPos -= yDelta * 10.0f; // (fontSize * 0.5f);

		rgfx_debugColor textColor = s_guiData.textDescArray[string].color;
		if (textColor.color == 0)
		{
			textColor = kDebugColorWhite;
		}

		indexCount += numChars * INDICES_PER_QUAD;
		for (int32_t ch = 0; ch < numChars; ++ch)
		{
			rgui_fontChar charData;
			rgui_getFontDataForChar(s_guiData.textDescArray[string].font, (int32_t)currentString[ch], &charData);
			vec4 p1 = curPos + xDelta * charData.xoff + yDelta * -charData.yoff;
			vec4 p2 = p1 + yDelta * -charData.h;
			vec4 p3 = p1 + xDelta * charData.w;
			vec4 p4 = p1 + yDelta * -charData.h + xDelta * charData.w;
			*(data++) = p1.x;
			*(data++) = p1.y;
			*(data++) = p1.z;
			*(data++) = charData.rect.x0;
			*(data++) = charData.rect.y0;
			*((rgfx_debugColor*)data++) = textColor;
			*(data++) = p2.x;
			*(data++) = p2.y;
			*(data++) = p2.z;
			*(data++) = charData.rect.x0;
			*(data++) = charData.rect.y1;
			*((rgfx_debugColor*)data++) = textColor;
			*(data++) = p3.x;
			*(data++) = p3.y;
			*(data++) = p3.z;
			*(data++) = charData.rect.x1;
			*(data++) = charData.rect.y0;
			*((rgfx_debugColor*)data++) = textColor;
			*(data++) = p4.x;
			*(data++) = p4.y;
			*(data++) = p4.z;
			*(data++) = charData.rect.x1;
			*(data++) = charData.rect.y1;
			*((rgfx_debugColor*)data++) = textColor;
			curPos += xDelta * charData.advance;
		}
	}
	rgfx_unmapBuffer(vertexBuffer);

	rgfx_bindVertexBuffer(s_guiData.textVertexBuffer);
	rgfx_bindPipeline(s_guiData.textPipeline);

	GLuint program = rgfx_getPipelineProgram(s_guiData.textPipeline, kVertexShader);
	if (g_viewIdLoc >= 0)  // NOTE: will not be present when multiview path is enabled.
	{
		glProgramUniform1i(program, g_viewIdLoc, eye);
	}

	rgfx_bindBuffer(1, rgfx_getTransforms());
	rgfx_bindTexture(0, fontTexture);
#ifdef TRIANGLE_STRIP_QUADS
	rgfx_drawIndexedPrims(kPrimTypeTriangleStrip, indexCount, 0);
#else
	rgfx_drawIndexedPrims(kPrimTypeTriangles, indexCount, 0);
#endif
	rgfx_unbindVertexBuffer();

	glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
}
