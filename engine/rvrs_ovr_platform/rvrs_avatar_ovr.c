#include "stdafx.h"
#include "system.h"
#include "rgfx/renderer.h"
#include "resource.h"
#include "vrsystem.h"
#include "rgfx/vertexPacker.h"
#include "stb/stretchy_buffer.h"

#ifdef VRSYSTEM_OCULUS_WIN
#include "OVR_CAPI.h"
#endif
#if 1
#include "rvrs_avatar_ovr.h"
#include "OVR_Avatar.h"

#define OCULUS_APP_ID "1869952619797041"
//#define AVATAR_CONSOLE
#define MAX_JOINTS 25

rvrs_avatarBody rvrs_createAvatarBodyOvr(const rvrs_avatarOvr avatar, const ovrAvatar* avatarOvr);
void rvrs_loadAvatarBodyMesh(rvrs_avatarOvr avatar, ovrAvatarAsset* asset);
void rvrs_loadAvatarBodyTextures(rvrs_avatarOvr avatar, ovrAvatarAssetID id, const ovrAvatarTextureAssetData* tex);
rvrs_avatarHands rvrs_createAvatarHandsOvr(const rvrs_avatarOvr avatar, const ovrAvatar* avatarOvr);
void rvrs_loadAvatarHandsMesh(rvrs_avatarOvr avatar, ovrAvatarAssetID id, ovrAvatarAsset* asset);
void rvrs_loadAvatarHandsTextures(rvrs_avatarOvr avatar, ovrAvatarAssetID id, const ovrAvatarTextureAssetData* tex);

typedef struct ovrAvatarBodyTextureSet
{
	ovrAvatarAssetID albedo;
	ovrAvatarAssetID normal;
	ovrAvatarAssetID metallic;
}ovrAvatarBodyTextureSet;

typedef struct rvrs_avatarMesh { int32_t id; } rvrs_avatarMesh;

typedef struct rvrs_avatarBodyOvr
{
	// These will correspond to an entry in the texture array. Once loaded
	// set the entry to 0. One all are 0, we are no longer waiting on any textures
	// and this data can be cleared.
	ovrAvatarBodyTextureSet* textureAssetArray;
	ovrAvatarExpressiveParameters expressiveParams;

//	rvrs_avatarMesh mesh;
	rgfx_renderable mesh;

	rgfx_texture albedoTextures;
	rgfx_texture normalTextures;
	rgfx_texture metallicTextures;

	uint32_t blendShapeSize;
	// Blend shapes will be stored in a texture buffer.
	// Note, this will only work on PC, Quest, and Go. Not GearVR.
	// Probably better to use a compute shader anyway, but using a texture buffer in the vertex shader
	// is simpler.
	rgfx_texture blendShapeTexture;
	rgfx_buffer blendShapeTextureBuffer;

//	rgfx_buffer materialVertexBuffer;
	rgfx_buffer materialFragmentBuffer;

	// Body Component cache
	ovrAvatarBodyComponent* bodyComp;
	// Cache the body render part, will use to retrieve blend shape parameters every frame.
	const ovrAvatarRenderPart* combinedRenderPart;
	// Cache the render component too.
	const ovrAvatarComponent* combinedRenderComp;

	// Store inverse bind pose array locally.
	mat4x4* inverseBindPoseArray;
	// Hold onto our world Transform. Created in Frame, used in Render
//	mat4x4 worldTransform;
	bool bMeshLoaded;
}rvrs_avatarBodyOvr;

typedef struct ovrAvatarHandsTextureSet
{
	ovrAvatarAssetID albedo;
	ovrAvatarAssetID normal;
}ovrAvatarHandsTextureSet;

typedef struct rvrs_avatarHandsOvr
{
	// These will correspond to an entry in the texture array. Once loaded
	// set the entry to 0. One all are 0, we are no longer waiting on any textures
	// and this data can be cleared.
	ovrAvatarAssetID* meshesToLoadArray;
	ovrAvatarHandsTextureSet* textureAssetArray;
	
//	rvrs_avatarMesh meshes[kAvatarNumHands];
	rgfx_renderable meshes[kAvatarNumHands];

	rgfx_texture albedoTextures;
	rgfx_texture normalTextures;

//	rgfx_buffer materialVertexBuffer;

	vec4 albedoTints[kAvatarNumHands];
	// Hold onto our world Transform. Created in Frame, used in Render
//	mat4x4 worldTransforms[kAvatarNumHands];

	// Cache the body render part, will use to retrieve blend shape parameters every frame.
	const ovrAvatarRenderPart* renderParts[kAvatarNumHands];
	// Cache the render component too.
	const ovrAvatarComponent* renderComps[kAvatarNumHands];
	// Store inverse bind pose array locally.
	mat4x4* inverseBindPoseArray[kAvatarNumHands];
}rvrs_avatarHandsOvr;

typedef struct rvrs_avatarInfoOvr
{
	ovrAvatar* avatarOvr;
	uint32_t hashId;
	uint32_t nameHash;
	rvrs_avatarBody body;
	rvrs_avatarHands hands;
	rgfx_renderable firstRenderable;
}rvrs_avatarInfoOvr;

typedef struct rvrs_avatarInstanceInfo
{
	rvrs_avatarOvr avatar;
	uint32_t flags;
	int32_t bodyBoneCount;
	int32_t handBoneCount;
	mat4x4 worldTransform[3];					// CLR - Transforms for Body (0), Left Hand (1) and Right Hand (2).
}rvrs_avatarInstanceInfo;

typedef struct rvrs_avatarDataOvr
{
	rvrs_avatarOvr avatar[4];
	rvrs_avatarOvr loadingAvatar;
	rvrs_avatarInfoOvr* avatarArray;
	rvrs_avatarInstanceInfo* avatarInstanceArray;
	rvrs_avatarBodyOvr* avatarBodiesArray;
	rvrs_avatarHandsOvr* avatarHandsArray;
	rsys_hash avatarLookup;
	uint64_t lastRequestedUserId;
	int32_t lastRequestedUserIdSlot;
	rgfx_buffer worldTransformBuffer;
	rgfx_buffer bodyMaterialVertexBuffer;
	rgfx_buffer bodyMaterialFragmentBuffer;
	rgfx_buffer handsMaterialVertexBuffer;
	rgfx_pipeline avatarBodyPipeline;
	rgfx_pipeline avatarHandsPipeline;
	int32_t avatarHandsUniformIndexHand;
	int32_t playerIndex;
	bool bInitialized;
}rvrs_avatarDataOvr;

const int32_t kBlendGroupSize = 4;
const int32_t kMaxBlendParams = 16;

const float kDiffuseIntensity[ovrAvatarBodyPartType_Count] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };		// CLR - Oculus Mirror sample values { 0.3f, 0.1f, 0.0f, 0.15f, 0.15f }; // 
const float kReflectionIntensity[ovrAvatarBodyPartType_Count] = { 0.0f, 0.3f, 0.4f, 0.0f, 0.0f };

#pragma pack(push, 1)
// std140 layout, must match shader
typedef struct rvrs_avatarBodyVertParamsOvr 
{
	vec4i blendShapeConstants; // x has size of a blend shape(in verts, i.e. 3 texels). y has number blend
								// shapes used, always a multiple of 4								//16 bytes
	vec4i blendShapeIndices[kMaxBlendParams / kBlendGroupSize]; // which blend shapes to blend		//64 bytes
	vec4 blendShapeWeights[kMaxBlendParams / kBlendGroupSize]; // blend shape weights				//64 bytes
	mat4x4 bones[9];																				//64 bytes * 9
	vec4 pad[3];																					//48 bytes
}rvrs_avatarBodyVertParamsOvr;																		//768 bytes total (padding to 256 bytes alignment).

typedef struct rvrs_avatarBodyFragParamsOvr 
{
	vec4 albedoTint[ovrAvatarBodyPartType_Count]; // One per renderpart.							//80 bytes
	vec4 intensity[ovrAvatarBodyPartType_Count]; // Diffuse in r, Reflection in g, brow in b, lip	//80 bytes
												 // smoothness in a
	vec4 colors[7]; // Expressive Colors															//448 bytes
}rvrs_avatarBodyFragParamsOvr;

typedef struct rvrs_avatarHandsVertParamsOvr 
{
	mat4x4 bones[28];																				//1792 bytes (256 bytes aligned).
}rvrs_avatarHandsVertParamsOvr;

#pragma pack(pop)

static rvrs_avatarDataOvr s_data = { 0 };

void rvrs_avatarMeshLoadCallback(rvrs_avatarOvr avatar);

void rvrs_initializeAvatarOvr(const char* appId)
{
	memset(&s_data, 0, sizeof(rvrs_avatarDataOvr));
	if (appId != NULL)
	{
#ifdef RE_PLATFORM_ANDROID
		extern ovrApp* s_appState;

		ovrAvatar_InitializeAndroid(appId, s_appState->Java.ActivityObject, s_appState->Java.Env);
#else
		ovrAvatar_Initialize(appId);
#endif
		s_data.bInitialized = true;
	}
}

void rvrs_terminateAvatarOvr()
{
	rsys_print("Shutting down Avatar SDK\n");
	int32_t count = sb_count(s_data.avatarArray);
	for (int32_t i = 0; i < count; ++i)
	{
		if (s_data.avatarArray[i].avatarOvr != NULL) {
			ovrAvatar_Destroy(s_data.avatarArray[i].avatarOvr);
			s_data.avatarArray[i].avatarOvr = NULL;
		}
	}
	sb_free(s_data.avatarArray);
	sb_free(s_data.avatarInstanceArray);
	for (int body = 0; body < sb_count(s_data.avatarBodiesArray); ++body) {
		sb_free(s_data.avatarBodiesArray[body].inverseBindPoseArray);
	}
	sb_free(s_data.avatarBodiesArray);
	for (int hands = 0; hands < sb_count(s_data.avatarHandsArray); ++hands) {
		sb_free(s_data.avatarHandsArray[hands].inverseBindPoseArray[0]);
		sb_free(s_data.avatarHandsArray[hands].inverseBindPoseArray[1]);
	}
	sb_free(s_data.avatarHandsArray);
	memset(&s_data, 0, sizeof(rvrs_avatarDataOvr));
	ovrAvatar_Shutdown();
}

#include <inttypes.h>

bool rvrs_processAvatarMessagesOvr()
{
	if (s_data.bInitialized == false) {
		return false;
	}
	ovrAvatarMessage* message = NULL;
	while ((message = ovrAvatarMessage_Pop()) != NULL)
	{
		switch (ovrAvatarMessage_GetType(message))
		{
			case ovrAvatarMessageType_AvatarSpecification:
			{
				const ovrAvatarMessage_AvatarSpecification* spec = ovrAvatarMessage_GetAvatarSpecification(message);
#ifdef AVATAR_CONSOLE
				rsys_consoleLog("Received Avatar Spec: %" PRIu64, spec->oculusUserID);
#endif
				if (spec->oculusUserID == s_data.lastRequestedUserId)
				{
					// We have our avatars specification, so we can create it now. Can't render the avatar yet
					// though. Need to download all of the needed meshes and textures still.
					ovrAvatarCapabilities caps = ovrAvatarCapability_Body | ovrAvatarCapability_Expressive | ovrAvatarCapability_Hands;
					//				s_data.avatarOvr = ovrAvatar_Create(spec->avatarSpec, caps);
					ovrAvatar* avatarOvr = ovrAvatar_Create(spec->avatarSpec, caps);

					ovrAvatar_SetLeftControllerVisibility(avatarOvr, false);
					ovrAvatar_SetLeftHandVisibility(avatarOvr, true);
					ovrAvatar_SetRightControllerVisibility(avatarOvr, false);
					ovrAvatar_SetRightHandVisibility(avatarOvr, true);

					s_data.loadingAvatar = rvrs_createAvatarOvr(spec->oculusUserID, avatarOvr);
					rvrs_avatarMeshLoadCallback(s_data.loadingAvatar);
					s_data.lastRequestedUserId = 0;
				}
				else
				{
					rsys_print("");
				}
				break;
			}
			case ovrAvatarMessageType_AssetLoaded:
			{
				const ovrAvatarMessage_AssetLoaded* assetMsg = ovrAvatarMessage_GetAssetLoaded(message);
				if (assetMsg)
				{
					ovrAvatarAssetType type = ovrAvatarAsset_GetType(assetMsg->asset);
					// Combined Mesh will always be the body mesh. Controllers and hands are always regular
					// meshes
					if (type == ovrAvatarAssetType_CombinedMesh)
					{
#ifdef AVATAR_CONSOLE
						rsys_consoleLog("Combined Mesh:");
#endif
						// For combined meshes, we dont use the assetId's. Just get the avatar
						// For our simple example, we only have a single avatar, so just assert that this mesh is
						// ours.
						const ovrAvatar* avatar = ovrAvatarAsset_GetAvatar(assetMsg->asset);
						int32_t avatarIdx = s_data.loadingAvatar.id - 1;
						assert(avatarIdx >= 0);
//						assert(avatar == s_data.avatarArray[avatarIdx].avatarOvr);
//						if (avatar == s_data.avatarArray[avatarIdx].avatarOvr)
						{
							// assert(avatar == s_data.avatarOvr);
							rvrs_loadAvatarBodyMesh(s_data.loadingAvatar, assetMsg->asset);
#ifdef AVATAR_CONSOLE
							rsys_consoleLog("Idx %d: Received %" PRIu64 " want %" PRIu64, avatarIdx, (uint64_t)avatar, (uint64_t)s_data.avatarArray[avatarIdx].avatarOvr);
#endif
							rsys_print("Load Combined Mesh\n");
						}
//						else
//						{
//							rsys_consoleLog("Idx %d: Received %" PRIu64 " want %" PRIu64, avatarIdx, (uint64_t)avatar, (uint64_t)s_data.avatarArray[avatarIdx].avatarOvr);
//						}
					}
					else if (type == ovrAvatarAssetType_Mesh)
					{
						rvrs_loadAvatarHandsMesh(s_data.loadingAvatar, assetMsg->assetID, assetMsg->asset);
						//					Controllers->LoadMesh(assetMsg->assetID, assetMsg->asset);
						//					Hands->LoadMesh(assetMsg->assetID, assetMsg->asset);
						rsys_print("Load Mesh\n");
					}
					else if (type == ovrAvatarAssetType_Texture)
					{
						const ovrAvatarTextureAssetData* tex = ovrAvatarAsset_GetTextureData(assetMsg->asset);
						rvrs_loadAvatarBodyTextures(s_data.loadingAvatar, assetMsg->assetID, tex);
						rvrs_loadAvatarHandsTextures(s_data.loadingAvatar, assetMsg->assetID, tex);
						//					Body->LoadTexture(assetMsg->assetID, tex);
						//					Controllers->LoadTexture(assetMsg->assetID, tex);
						//					Hands->LoadTexture(assetMsg->assetID, tex);
						rsys_print("Load Texture\n");
					}
				}
				break;
			}
			default:
				break;
		}
		ovrAvatarMessage_Free(message);
	}
	return true;
}

bool rvrs_requestAvatarSpecOvr(uint64_t userId, int32_t slot)
{
	if (s_data.bInitialized && s_data.lastRequestedUserId == 0)
	{
		s_data.lastRequestedUserIdSlot = slot;
#ifdef AVATAR_CONSOLE
		rsys_consoleClear();
		rsys_consoleLog("Request Avatar Spec: %" PRIu64, userId);
#endif
		s_data.lastRequestedUserId = userId;
		// Now request our avatar specification
		ovrAvatarSpecificationRequest* requestSpec = ovrAvatarSpecificationRequest_Create(userId);
		// For performance reasons, you should always use combined meshes.
		ovrAvatarSpecificationRequest_SetCombineMeshes(requestSpec, true);
		ovrAvatarSpecificationRequest_SetLookAndFeelVersion(requestSpec, ovrAvatarLookAndFeelVersion_Two);
		ovrAvatarSpecificationRequest_SetExpressiveFlag(requestSpec, true);
		ovrAvatar_RequestAvatarSpecificationFromSpecRequest(requestSpec);
		ovrAvatarSpecificationRequest_Destroy(requestSpec);

		return true;
	}
	return false;
}

bool rvrs_destroyAvatarOvr(int32_t slot)
{
	if (s_data.avatar[slot].id == 0)
		return false;

	rvrs_avatarBody body = s_data.avatarArray[slot].body;
	rvrs_avatarHands hands = s_data.avatarArray[slot].hands;

	int32_t bodyIdx = body.id - 1;
	rvrs_avatarBodyOvr *bodyInfo = &s_data.avatarBodiesArray[bodyIdx];
	rgfx_destroyTexture(bodyInfo->albedoTextures);
	rgfx_destroyTexture(bodyInfo->normalTextures);
	rgfx_destroyTexture(bodyInfo->metallicTextures);
	rgfx_destroyBuffer(bodyInfo->materialFragmentBuffer);
	if (bodyInfo->inverseBindPoseArray != NULL) {
		sb_free(bodyInfo->inverseBindPoseArray);
	}
	memset(bodyInfo, 0, sizeof(rvrs_avatarBodyOvr));
	sb_pop(s_data.avatarBodiesArray, 1);

	int32_t handsIdx = hands.id - 1;
	rvrs_avatarHandsOvr* handsInfo = &s_data.avatarHandsArray[handsIdx];
	rgfx_destroyTexture(handsInfo->albedoTextures);
	rgfx_destroyTexture(handsInfo->normalTextures);
	if (handsInfo->inverseBindPoseArray[0] != NULL) {
		sb_free(handsInfo->inverseBindPoseArray[0]);
	}
	if (handsInfo->inverseBindPoseArray[1] != NULL) {
		sb_free(handsInfo->inverseBindPoseArray[1]);
	}
	memset(bodyInfo, 0, sizeof(rvrs_avatarBodyOvr));
	memset(handsInfo, 0, sizeof(rvrs_avatarHandsOvr));
	sb_pop(s_data.avatarHandsArray, 1);
	rgfx_popMesh(1);
	rsys_hm_clear(&s_data.avatarLookup, s_data.avatarArray[slot].hashId);
	sb_pop(s_data.avatarArray, 1);
	s_data.avatar[slot].id = 0;

	return true;
}

#define MIN_ID(a, b) ((a.id) < (b.id) ? (a) : (b))

rvrs_avatarOvr rvrs_createAvatarOvr(uint64_t userId, const ovrAvatar* avatarOvr)
{
	rvrs_avatarOvr avatar;
/*
	if (s_data.worldTransformBuffer.id == 0)
	{
		s_data.worldTransformBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
			.capacity = sizeof(mat4x4),
			.stride = sizeof(mat4x4),
			.flags = GL_MAP_WRITE_BIT
		});
	}
*/
	uint32_t hashId = hashData(&userId, sizeof(userId));

	int32_t idx = rsys_hm_find(&s_data.avatarLookup, hashId);
	avatar.id = idx + 1;
	if (idx < 0)
	{
		rvrs_avatarInfoOvr* info = sb_add(s_data.avatarArray, 1);
		idx = info - s_data.avatarArray;
		avatar.id = idx + 1;
		rsys_hm_insert(&s_data.avatarLookup, hashId, idx);

		char avatarNameSting[64];
		//	sprintf_s(avatarNameSting, sizeof(avatarNameSting), "Avatar%d", idx);
		snprintf(avatarNameSting, sizeof(avatarNameSting), "Avatar%d", idx);
		rres_registerMesh(avatarNameSting);
		//	uint32_t meshNameHash = rres_getMesh(meshIdx)->
#ifdef AVATAR_CONSOLE
		rsys_consoleLog("Inserting avatar %" PRIu64 " in slot %d for userID %" PRIu64, (uint64_t)avatarOvr, idx, userId);
#endif
		info->avatarOvr = avatarOvr;
		info->hashId = hashId;
		info->nameHash = hashString(avatarNameSting);
		info->body = rvrs_createAvatarBodyOvr(avatar, avatarOvr);
		info->hands = rvrs_createAvatarHandsOvr(avatar, avatarOvr);
		info->firstRenderable = (rgfx_renderable){ .id = 0 };
	}
	return avatar;
}

rvrs_avatarBody rvrs_createAvatarBodyOvr(const rvrs_avatarOvr avatar, const ovrAvatar* avatarOvr)
{	
	const ovrAvatarBodyComponent* bodyComp = ovrAvatarPose_GetBodyComponent((ovrAvatar*)avatarOvr);
	const ovrAvatarComponent* rcomp = bodyComp->renderComponent;

	ovrAvatarExpressiveParameters expressiveParams = ovrAvatar_GetExpressiveParameters(avatarOvr);

	rgfx_buffer materialFragmentBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
		.capacity = sizeof(rvrs_avatarBodyFragParamsOvr),
		.stride = sizeof(rvrs_avatarBodyFragParamsOvr),
		.flags = kMapWriteBit
	});
	rvrs_avatarBodyFragParamsOvr* matFragBuf = rgfx_mapBuffer(materialFragmentBuffer);

	ovrAvatarBodyTextureSet* textureAssetArray = NULL;
	sb_add(textureAssetArray, ovrAvatarBodyPartType_Count);
	memset(textureAssetArray, 0, sizeof(ovrAvatarBodyTextureSet) * ovrAvatarBodyPartType_Count);
	//	TexturesToLoad.resize(ovrAvatarBodyPartType::ovrAvatarBodyPartType_Count);
		// Note, all of these renderParts will be replaced by a single renderPart after the combined mesh
		// has been loaded So you can not cache much here. Will need to go through the renderPart list
		// again later, on first Frame call, Mostly so we can get the bones and blend shape weights, which
		// are different for the combined renderPart.
#ifdef AVATAR_CONSOLE
	rsys_consoleLog("Request Body Mesh: %d part(s)", rcomp->renderPartCount);
#endif
	for (uint32_t rendIndex = 0; rendIndex < rcomp->renderPartCount; ++rendIndex)
	{
		const ovrAvatarRenderPart* rendPart = rcomp->renderParts[rendIndex];
		ovrAvatarRenderPartType type = ovrAvatarRenderPart_GetType(rendPart);
		// should always be a PBS V2, rest of code will assume that.
		assert(type == ovrAvatarRenderPartType_SkinnedMeshRenderPBS_V2);

		const ovrAvatarRenderPart_SkinnedMeshRenderPBS_V2* mesh = ovrAvatarRenderPart_GetSkinnedMeshRenderPBSV2(rendPart);
		assert(mesh->meshAssetID != 0);
		// Start fetching mesh asset(from cache, or downloaded. May take a while)
#ifdef AVATAR_CONSOLE
		rsys_consoleLog("    Part: %d", mesh->meshAssetID);
#endif
		ovrAvatarAsset_BeginLoading(mesh->meshAssetID);

		ovrAvatarBodyPartType renderType = ovrAvatarRenderPart_GetBodyPartType((ovrAvatar*)avatarOvr, rendPart);
		assert(renderType != ovrAvatarBodyPartType_Count);

		const ovrAvatarPBSMaterialState* mat = &mesh->materialState;
		textureAssetArray[renderType] = (ovrAvatarBodyTextureSet) {
			.albedo = mat->albedoTextureID,
			.normal = mat->normalTextureID,
			.metallic = mat->metallicnessTextureID,
		};
		//		TextureToLoad textures{ mat.albedoTextureID, mat.normalTextureID, mat.metallicnessTextureID };
		//		TexturesToLoad[renderType] = textures;
				// Start fetching texture asset(from cache, or downloaded. May take a while)
		ovrAvatarAsset_BeginLoading(mat->albedoTextureID);
		ovrAvatarAsset_BeginLoading(mat->normalTextureID);
		ovrAvatarAsset_BeginLoading(mat->metallicnessTextureID);

		const float kGamma = 2.2f;
		if (renderType == 2)
			matFragBuf->albedoTint[renderType] = (vec4){ powf(mat->baseColor.x, kGamma), powf(mat->baseColor.y, kGamma), powf(mat->baseColor.z, kGamma), mat->baseColor.w };
		else
			matFragBuf->albedoTint[renderType] = (vec4){ powf(mat->albedoMultiplier.x, kGamma), powf(mat->albedoMultiplier.y, kGamma), powf(mat->albedoMultiplier.z, kGamma), mat->albedoMultiplier.w };

		matFragBuf->intensity[renderType][0] = kDiffuseIntensity[renderType];
		matFragBuf->intensity[renderType][1] = kReflectionIntensity[renderType];
		// Note these 2 are only used for body part, just write same values into all fields.
		matFragBuf->intensity[renderType][2] = expressiveParams.browLashIntensity;
		matFragBuf->intensity[renderType][3] = expressiveParams.lipSmoothness;
	}

	matFragBuf->colors[0] = (vec4){ expressiveParams.browColor.x, expressiveParams.browColor.y, expressiveParams.browColor.z, expressiveParams.browColor.w };
	matFragBuf->colors[1] = (vec4){ expressiveParams.teethColor.x, expressiveParams.teethColor.y, expressiveParams.teethColor.z, expressiveParams.teethColor.w };
	matFragBuf->colors[2] = (vec4){ expressiveParams.gumColor.x, expressiveParams.gumColor.y, expressiveParams.gumColor.z, expressiveParams.gumColor.w };
	matFragBuf->colors[3] = (vec4){ expressiveParams.lipColor.x, expressiveParams.lipColor.y, expressiveParams.lipColor.z, expressiveParams.lipColor.w };
	matFragBuf->colors[4] = (vec4){ expressiveParams.lashColor.x, expressiveParams.lashColor.y, expressiveParams.lashColor.z, expressiveParams.lashColor.w };
	matFragBuf->colors[5] = (vec4){ expressiveParams.scleraColor.x, expressiveParams.scleraColor.y, expressiveParams.scleraColor.z, expressiveParams.scleraColor.w };
	matFragBuf->colors[6] = (vec4){ expressiveParams.irisColor.x, expressiveParams.irisColor.y, expressiveParams.irisColor.z, expressiveParams.irisColor.w };
#if 0
	for (int32_t i = 0; i < 7; ++i)
	{
//		matFragBuf->colors[i] = (vec4){ powf(matFragBuf->colors[i].x, 2.2f), powf(matFragBuf->colors[i].y, 2.2f), powf(matFragBuf->colors[i].z, 2.2f), matFragBuf->colors[i].w };
	}
#endif
	rgfx_unmapBuffer(materialFragmentBuffer);

	// Create this buffer here, but nothing to write to it at this time, it will be updated every
	// frame.
/*
	rgfx_buffer materialVertexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
		.capacity = sizeof(rvrs_avatarBodyVertParamsOvr),
		.stride = sizeof(rvrs_avatarBodyVertParamsOvr),
		.flags = GL_MAP_WRITE_BIT
	});
*/
	if (s_data.avatarBodyPipeline.id == 0) {
		s_data.avatarBodyPipeline = rgfx_createPipeline(&(rgfx_pipelineDesc) {
			.vertexShader = rgfx_loadShader("shaders/avatarBody.vert", kVertexShader, 0),
			.fragmentShader = rgfx_loadShader("shaders/avatarBody.frag", kFragmentShader, 0),
		});
	}

	rvrs_avatarBodyOvr* avatarBody = sb_add(s_data.avatarBodiesArray, 1);
	memset(avatarBody, 0, sizeof(rvrs_avatarBodyOvr));
	rvrs_avatarBody retVal = { 0 };

	avatarBody->bMeshLoaded = false;
	avatarBody->textureAssetArray = textureAssetArray;
	avatarBody->albedoTextures.id = 0;
	avatarBody->normalTextures.id = 0;
	avatarBody->metallicTextures.id = 0;
	avatarBody->blendShapeSize = 0;
	avatarBody->blendShapeTexture.id = 0;
	avatarBody->blendShapeTextureBuffer.id = 0;
//	avatarBody->materialVertexBuffer = materialVertexBuffer;
	avatarBody->materialFragmentBuffer = materialFragmentBuffer;
	avatarBody->bodyComp = (ovrAvatarBodyComponent *)bodyComp;
	avatarBody->combinedRenderPart = NULL;
	avatarBody->combinedRenderComp = NULL;
//	avatarBody->worldTransform = mat4x4_identity();

	retVal.id = (avatarBody - s_data.avatarBodiesArray) + 1;

//	int32_t avatarIdx = avatar.id - 1;
//	assert(avatarIdx >= 0);
//	s_data.avatarArray[avatarIdx].body = retVal;

	return retVal;
}

rvrs_avatarHands rvrs_createAvatarHandsOvr(const rvrs_avatarOvr avatar, const ovrAvatar* avatarOvr)
{
	const ovrAvatarHandComponent* handComps[kAvatarNumHands] = { 0 };
	handComps[0] = ovrAvatarPose_GetLeftHandComponent((ovrAvatar*)avatarOvr);
	handComps[1] = ovrAvatarPose_GetRightHandComponent((ovrAvatar*)avatarOvr);

	ovrAvatarHandsTextureSet* textureAssetArray = NULL;
	sb_add(textureAssetArray, kAvatarNumHands);
	memset(textureAssetArray, 0, sizeof(ovrAvatarHandsTextureSet) * kAvatarNumHands);

	ovrAvatarAssetID* meshAssetArray = NULL;
	sb_add(meshAssetArray, kAvatarNumHands);
	memset(meshAssetArray, 0, sizeof(ovrAvatarAssetID) * kAvatarNumHands);

	const ovrAvatarRenderPart* renderParts[2] = { 0 };
	const ovrAvatarComponent* renderComps[2] = { 0 };
	for (uint32_t hand = 0; hand < 2; ++hand)
	{
		const ovrAvatarComponent* rcomp = handComps[hand]->renderComponent;
		assert(rcomp->renderPartCount == 1); // should only be one render component on a hand

		const ovrAvatarRenderPart* rendPart = rcomp->renderParts[0];
		ovrAvatarRenderPartType type = ovrAvatarRenderPart_GetType(rendPart);
		// should always be a PBS V2, rest of code will assume that.
		assert(type == ovrAvatarRenderPartType_SkinnedMeshRenderPBS_V2);

		const ovrAvatarRenderPart_SkinnedMeshRenderPBS_V2* mesh =
			ovrAvatarRenderPart_GetSkinnedMeshRenderPBSV2(rendPart);
		assert(mesh->meshAssetID != 0);
		// Start fetching mesh asset(from cache, or downloaded. May take a while)
#ifdef AVATAR_CONSOLE
		rsys_consoleLog("Request Hand Mesh:");
#endif
		ovrAvatarAsset_BeginLoading(mesh->meshAssetID);
		meshAssetArray[hand] = mesh->meshAssetID;

		const ovrAvatarPBSMaterialState* mat = &mesh->materialState;
		textureAssetArray[hand] = (ovrAvatarHandsTextureSet){
			.albedo = mat->albedoTextureID,
			.normal = mat->normalTextureID,
		};

		ovrAvatarAsset_BeginLoading(mat->albedoTextureID);
		ovrAvatarAsset_BeginLoading(mat->normalTextureID);

		renderParts[hand] = rendPart;
		renderComps[hand] = rcomp;
	}
/*
	rgfx_buffer materialVertexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
		.capacity = sizeof(rvrs_avatarHandsVertParamsOvr) * 2,
		.stride = sizeof(rvrs_avatarHandsVertParamsOvr),
		.flags = GL_MAP_WRITE_BIT
	});
*/
	if (s_data.avatarHandsPipeline.id == 0) {
		s_data.avatarHandsPipeline = rgfx_createPipeline(&(rgfx_pipelineDesc) {
			.vertexShader = rgfx_loadShader("shaders/avatarHands.vert", kVertexShader, 0),
			.fragmentShader = rgfx_loadShader("shaders/avatarHands.frag", kFragmentShader, 0),
		});
	}
	uint32_t fragmentProgram = rgfx_getPipelineProgram(s_data.avatarHandsPipeline, kFragmentShader);
	s_data.avatarHandsUniformIndexHand = glGetProgramResourceLocation(fragmentProgram, GL_UNIFORM, "Hand");

	rvrs_avatarHandsOvr* avatarHands = sb_add(s_data.avatarHandsArray, 1);
	memset(avatarHands, 0, sizeof(rvrs_avatarHandsOvr));
	rvrs_avatarHands retVal = { 0 };

	avatarHands->meshesToLoadArray = meshAssetArray;
	avatarHands->textureAssetArray = textureAssetArray;
	avatarHands->albedoTextures.id = 0;
	avatarHands->normalTextures.id = 0;
//	avatarHands->materialVertexBuffer = materialVertexBuffer;
	avatarHands->renderParts[0] = renderParts[0];
	avatarHands->renderComps[0] = renderComps[0];
	avatarHands->renderParts[1] = renderParts[1];
	avatarHands->renderComps[1] = renderComps[1];
	retVal.id = (avatarHands - s_data.avatarHandsArray) + 1;

//	int32_t avatarIdx = avatar.id - 1;
//	assert(avatarIdx >= 0);
//	s_data.avatarArray[avatarIdx].hands = retVal;

	return retVal;
}

/*
typedef struct VertexPacker
{
	uint8_t* base;
	uint8_t* ptr;
}VertexPacker;

void packPosition(VertexPacker* packer, float x, float y, float z)
{
	float* destPtr = (float*)packer->ptr;
	destPtr[0] = x;
	destPtr[1] = y;
	destPtr[2] = z;
	packer->ptr += (sizeof(float) * 3);
}

void packFrenet(VertexPacker* packer, float x, float y, float z)
{
	uint32_t* packedPtr = (uint32_t*)packer->ptr;
	*packedPtr = 0;
//	AssertMsg((fabsf(x) <= 1.0f), "X component is greater than 1.0f\n");
//	AssertMsg((fabsf(y) <= 1.0f), "Y component is greater than 1.0f\n");
//	AssertMsg((fabsf(z) <= 1.0f), "Z component is greater than 1.0f\n");
	int32_t xInt = (int32_t)(x * (powf(2.0f, 10.0f - 1.0f) - 1.0f));
	int32_t yInt = (int32_t)(y * (powf(2.0f, 10.0f - 1.0f) - 1.0f));
	int32_t zInt = (int32_t)(z * (powf(2.0f, 10.0f - 1.0f) - 1.0f));
	*packedPtr = (zInt & 0x3ff) << 20 | (yInt & 0x3ff) << 10 | (xInt & 0x3ff);
	//		*packedPtr = (xInt & 0x3ff) << 20 | (yInt & 0x3ff) << 10 | (zInt & 0x3ff);
	packer->ptr += sizeof(uint32_t);
}

void packUV(VertexPacker* packer, float u, float v)
{
	float* destPtr = (float*)packer->ptr;
	destPtr[0] = u;
	destPtr[1] = v;
	packer->ptr += (sizeof(float) * 2);

}

void packJointIndices(VertexPacker* packer, uint8_t index1, uint8_t index2)
{
	uint8_t* destPtr = (uint8_t*)packer->ptr;
	destPtr[0] = index1;
	destPtr[1] = index2;
	packer->ptr += 2;
}

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef clamp
#define clamp(a, b, c)		(min(max((a), (b)), (c)))
#endif

void packJointWeights(VertexPacker* packer, float weight1, float weight2)
{
	uint8_t* destPtr = (uint8_t*)packer->ptr;
//	weight1 min(max(weight1, 0.0f), 1.0f);
	weight1 = clamp(weight1, 0.0f, 1.0f) * 255.0f;
	weight2 = clamp(weight2, 0.0f, 1.0f) * 255.0f;
	destPtr[0] = (uint8_t)weight1;
	destPtr[1] = (uint8_t)weight2;
	packer->ptr += 2;
}

void packRenderPart(VertexPacker* packer, float part)
{
	uint8_t* destPtr = (uint8_t*)packer->ptr;
	part *= 4.0f;
	part = clamp(roundf(part), 0.0f, 4.0f);
	destPtr[0] = (uint8_t)part;
	destPtr[1] = 0;
	destPtr[2] = 0;
	destPtr[3] = 0;
	packer->ptr += 4;
}
*/

/*inline*/ mat4x4 Convert(const ovrAvatarTransform *transform) 
{
	assert(transform != NULL);

	vec4 rot = (vec4) { transform->orientation.x, transform->orientation.y, transform->orientation.z, transform->orientation.w };
	vec4 position = (vec4) { transform->position.x, transform->position.y, transform->position.z, 1.0f };
//	glm::vec3 scale = glm::make_vec3((float*)&transform.scale);
	return mat4x4_create(rot, position); // ::translate(tran)* glm::mat4_cast(rot)* glm::scale(scale);
}

/*inline*/ mat4x4 ConvertPose(const ovrPosef* pose)
{
	assert(pose != NULL);

	vec4 rot = (vec4){ pose->Orientation.x, pose->Orientation.y, pose->Orientation.z, pose->Orientation.w };
	vec4 position = (vec4){ pose->Position.x, pose->Position.y, pose->Position.z, 1.0f };
	return mat4x4_create(rot, position);
}

rgfx_renderable rvrs_loadAvatarMeshOvr(const ovrAvatarAsset* asset, const void* ovrMesh, mat4x4** ppInverseBindPoseArray, bool isV2)
{
	const ovrAvatarMeshAssetData* assetData = (const ovrAvatarMeshAssetData*)ovrMesh;
	const ovrAvatarMeshAssetDataV2* assetDataV2 = (const ovrAvatarMeshAssetDataV2*)ovrMesh;
	// TODO: Not currently rendering any submeshes, except for submesh 0, the primary mesh
	// CLR - What exactly does the above mean?
	uint32_t smCount = ovrAvatarAsset_GetSubmeshCount(asset);
	assert(smCount >= 1);
#ifdef AVATAR_CONSOLE
	rsys_consoleLog("    SubmeshCount = %d", smCount);
#endif

	uint32_t indexCount = ovrAvatarAsset_GetSubmeshLastIndex(asset, 0);
	assert(indexCount > 0);
#ifdef AVATAR_CONSOLE
	rsys_consoleLog("    indexCount = %d", indexCount);
#endif

//	if (s_data.vertexBuffer.id == 0)		// CLR - This was to keep all avatars in one big vertex buffer, but we need different vbs for body
//											// and hands so it's not really working correctly right now.
	rgfx_vertexBuffer vertexBuffer = { 0 };
	{
		uint32_t vertexStride = 0;
		{
			rgfx_vertexElement vertexStreamElements[] =
			{
				{ kFloat, 3, false },
				{ kInt2_10_10_10_Rev, 4, true },
				{ kInt2_10_10_10_Rev, 4, true },
				{ kFloat, 2, false },
				{ kUnsignedByte, 2, false },
				{ kUnsignedByte, 2, true },
				{ kUnsignedByte, 4, false },			// CLR - Only used when isV2 is true.
			};
			int32_t numElements = STATIC_ARRAY_SIZE(vertexStreamElements);
			rgfx_vertexFormat vertexFormat = rgfx_registerVertexFormat((isV2 ? numElements : numElements - 1), vertexStreamElements);
			vertexStride = rgfx_getVertexFormatInfo(vertexFormat)->stride;
			vertexBuffer = rgfx_createVertexBuffer(&(rgfx_vertexBufferDesc) {
				.format = vertexFormat,
				.capacity = vertexStride * 64 * 1024, // 6553(isV2 ? assetDataV2->vertexCount : assetData->vertexCount),
				.indexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.type = kIndexBuffer,
					.capacity = 64 * 1024 * sizeof(uint16_t),
					.stride = sizeof(uint16_t),
					.flags = kMapWriteBit | kDynamicStorageBit,
						//				.data = (isV2 ? assetDataV2->indexBuffer : assetData->indexBuffer),
				})
			});
		}
	}
	rgfx_bufferInfo* bufferInfo = rgfx_getBufferInfo(rgfx_getVertexBufferBuffer(vertexBuffer, kVertexBuffer));
	uint32_t vertexStride = bufferInfo->stride;

	uint32_t firstIndex = rgfx_writeIndexData(vertexBuffer, sizeof(uint16_t) * indexCount, (uint8_t*)(isV2 ? assetDataV2->indexBuffer : assetData->indexBuffer), sizeof(uint16_t));

	uint32_t vertexCount = isV2 ? assetDataV2->vertexCount : assetData->vertexCount;
	VertexPacker packer;
	void* memory = malloc(vertexStride * vertexCount);
	memset(memory, 0, vertexStride);
	packer.base = packer.ptr = memory;

#ifdef AVATAR_CONSOLE
	rsys_consoleLog("    vertexCount = %d", vertexCount);
#endif
	for (uint32_t i = 0; i < vertexCount; ++i)
	{
		if (isV2)
		{
			packPosition(&packer, assetDataV2->vertexBuffer[i].x, assetDataV2->vertexBuffer[i].y, assetDataV2->vertexBuffer[i].z);
			packFrenet(&packer, assetDataV2->vertexBuffer[i].nx, assetDataV2->vertexBuffer[i].ny, assetDataV2->vertexBuffer[i].nz);
			packFrenet(&packer, assetDataV2->vertexBuffer[i].tx, assetDataV2->vertexBuffer[i].ty, assetDataV2->vertexBuffer[i].tz);
			packUV(&packer, assetDataV2->vertexBuffer[i].u, assetDataV2->vertexBuffer[i].v);
			packJointIndices(&packer, assetDataV2->vertexBuffer[i].blendIndices[0], assetDataV2->vertexBuffer[i].blendIndices[1]);
			packJointWeights(&packer, assetDataV2->vertexBuffer[i].blendWeights[0], assetDataV2->vertexBuffer[i].blendWeights[1]);
			packRenderPart(&packer, assetDataV2->vertexBuffer[i].r);
		}
		else
		{
			packPosition(&packer, assetData->vertexBuffer[i].x, assetData->vertexBuffer[i].y, assetData->vertexBuffer[i].z);
			packFrenet(&packer, assetData->vertexBuffer[i].nx, assetData->vertexBuffer[i].ny, assetData->vertexBuffer[i].nz);
			packFrenet(&packer, assetData->vertexBuffer[i].tx, assetData->vertexBuffer[i].ty, assetData->vertexBuffer[i].tz);
			packUV(&packer, assetData->vertexBuffer[i].u, assetData->vertexBuffer[i].v);
			packJointIndices(&packer, assetData->vertexBuffer[i].blendIndices[0], assetData->vertexBuffer[i].blendIndices[1]);
			packJointWeights(&packer, assetData->vertexBuffer[i].blendWeights[0], assetData->vertexBuffer[i].blendWeights[1]);
		}
	}
	uint32_t firstVertex = rgfx_writeVertexData(vertexBuffer, vertexStride*vertexCount, packer.base);
	free(memory);

	//	.data = (isV2 ? assetDataV2->indexBuffer : assetData->indexBuffer),

		// now compute the inverse bind pose
	int32_t bindPoseIndex = -1;
	if (isV2)
	{
		bindPoseIndex = 0;
	}
	else
	{
		char* isLeftHand = strstr(assetData->skinnedBindPose.jointNames[0], "l_hand");
		if (isLeftHand)
		{
			bindPoseIndex = 1;
		}
		else
		{
			bindPoseIndex = 2;
		}
	}
	assert(bindPoseIndex >= 0);
	assert(bindPoseIndex < 3);

	assert(assetData->skinnedBindPose.jointCount <= MAX_JOINTS);

	const uint32_t maxJoints = min(assetData->skinnedBindPose.jointCount, MAX_JOINTS);
	mat4x4* inverseBindPoseArray = NULL;
	sb_add(inverseBindPoseArray, maxJoints);
	*ppInverseBindPoseArray = inverseBindPoseArray;
//	rsys_print("%d Joints \n", assetData->skinnedBindPose.jointCount);
	for (uint32_t i = 0; i < maxJoints; i++)
	{
		mat4x4 worldPose;
		mat4x4 local = Convert(&assetData->skinnedBindPose.jointTransform[i]);
		const int32_t parentIndex = assetData->skinnedBindPose.jointParents[i];
		if (parentIndex < 0)
		{
			worldPose = local;
		}
		else
		{
			worldPose = mat4x4_mul(inverseBindPoseArray[parentIndex], local);
		}
/*
		rsys_print("Joint %02d: \n", i);
		for (int32_t axis = 0; axis < 4; ++axis)
		{
			float x = worldPose.axis[axis].x;
			float y = worldPose.axis[axis].y;
			float z = worldPose.axis[axis].z;
			float w = worldPose.axis[axis].w;
			rsys_print("\t%.3f, %.3f, %.3f, %.3f\n", x, y, z, w);
		}
*/
		inverseBindPoseArray[i] = worldPose;
	}

	for (uint32_t i = 0; i < maxJoints; i++)
	{
		mat4x4 joint = inverseBindPoseArray[i];
		inverseBindPoseArray[i] = mat4x4_orthoInverse(joint);
	}

	rgfx_renderable rend = rgfx_addRenderable(&(rgfx_renderableInfo) {
		.firstVertex = firstVertex,
		.firstIndex = firstIndex * 2,
		.indexCount = indexCount,
		.materialIndex = -1,
		.pipeline = isV2 ? s_data.avatarBodyPipeline : s_data.avatarHandsPipeline,
		.vertexBuffer = vertexBuffer,
	});

	return rend;
}

void rvrs_loadAvatarTextureOvr(rgfx_texture* texDest, uint32_t index, const ovrAvatarTextureAssetData* tex, bool srgb)
{
	rgfx_format format = kFormatInvalid;
	if (tex->format == ovrAvatarTextureFormat_ASTC_RGB_6x6_MIPMAPS)
	{
// CLR - We don't currently have an rgfx_format for ASTC textures.
//		format = srgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR : GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
		format = srgb ? kFormatSRGBASTC6x6 : kFormatRGBAASTC6x6;
	}
	else if (tex->format == ovrAvatarTextureFormat_DXT1)
	{
		format = srgb ? kFormatSRGBDXT1 : kFormatRGBDXT1;
	}
	else if (tex->format == ovrAvatarTextureFormat_DXT5)
	{
		format = srgb ? kFormatSRGBDXT5 : kFormatRGBADXT5;
	}
	else
	{
		rsys_print("Error: unsupported texture format from Avatar SDK %d\n", tex->format);
		assert(0);
	}

	// Textures are all the same size for a given platform. If this is first one we have seen, create
	// the texture array with the size of that texture. For combined meshes, we will render the avatar
	// in a single draw call, but we still have up to 5 parts, each with a seperate texture. So we
	// will store the textures in a texture array, one entry per part(body, clothing, hair, eyewear,
	// beard). For hands we will put 2 textures, one for left, one for right

	if (texDest->id == 0)
	{
		*texDest = rgfx_createTexture(&(rgfx_textureDesc) {
			.width = tex->sizeX,
			.height = tex->sizeY,
			.depth = 5, // CLR - Textures for each of the 5 parts are stored in a texture array.
			.mipCount = tex->mipCount,
			.format = format,
			.type = kTexture2DArray,
			.minFilter = kFilterLinearMipmapLinear,
			.magFilter = kFilterLinear,
			.wrapS = kWrapModeRepeat,
			.wrapT = kWrapModeRepeat,
			.anisoFactor = 0.25f,
		});
	}

	uint32_t width = tex->sizeX;
	uint32_t height = tex->sizeY;
	const uint8_t* data = tex->textureData;

	for (unsigned int mip = 0; mip < tex->mipCount; mip++)
	{
		int32_t mipSize = 0;
		if (tex->format == ovrAvatarTextureFormat_ASTC_RGB_6x6_MIPMAPS)
		{
			int32_t blocksWide = (width + 5) / 6;
			int32_t blocksHigh = (height + 5) / 6;
			mipSize = 16 * blocksWide * blocksHigh;
		}
		else if (tex->format == ovrAvatarTextureFormat_DXT1)
		{
			int32_t blocksWide = (width + 3) / 4;
			int32_t blocksHigh = (height + 3) / 4;
			mipSize = 8 * blocksWide * blocksHigh;
		}
		else if (tex->format == ovrAvatarTextureFormat_DXT5)
		{
			int32_t blocksWide = (width + 3) / 4;
			int32_t blocksHigh = (height + 3) / 4;
			mipSize = 16 * blocksWide * blocksHigh;
		}
		else
		{
			rsys_print("Error: Invalid texture format\n");
			assert(0);
		}

		rgfx_writeCompressedTexSubImage3D(*texDest, mip, 0, 0, index, width, height, 1, format, mipSize, data);

		data += mipSize;
		width /= 2;
		height /= 2;
		width = max(width, 1u);
		height = max(height, 1u);
	}
}

void rvrs_avatarMeshLoadCallback(rvrs_avatarOvr avatar)
{
	int32_t avatarIdx = avatar.id - 1;
	assert(avatarIdx >= 0);
	rvrs_avatarBody body = s_data.avatarArray[avatarIdx].body;
	rvrs_avatarHands hands = s_data.avatarArray[avatarIdx].hands;

	int32_t bodyIdx = body.id - 1;
	assert(bodyIdx >= 0);
	int32_t handsIdx = hands.id - 1;
	assert(handsIdx >= 0);
	if (s_data.avatarBodiesArray[bodyIdx].mesh.id > 0 && 
		s_data.avatarHandsArray[handsIdx].meshes[0].id > 0 && 
		s_data.avatarHandsArray[handsIdx].meshes[1].id > 0 && 
		s_data.avatarBodiesArray[bodyIdx].textureAssetArray == NULL &&
		s_data.avatarHandsArray[handsIdx].textureAssetArray == NULL)
	{
		rgfx_renderable body = s_data.avatarBodiesArray[bodyIdx].mesh;
		rgfx_renderable leftHand = s_data.avatarHandsArray[handsIdx].meshes[0];
		rgfx_renderable rightHand = s_data.avatarHandsArray[handsIdx].meshes[1];
		rgfx_renderable firstRenderable = MIN_ID(body, MIN_ID(leftHand, rightHand));
		s_data.avatarArray[avatarIdx].firstRenderable = firstRenderable;
		rgfx_mesh bodyMesh = rgfx_addMesh(&(rgfx_meshDesc) {
			.meshHash = s_data.avatarArray[avatarIdx].nameHash,
			.numRenderables = 3,
			.firstRenderable = firstRenderable,
		});
		s_data.avatar[s_data.lastRequestedUserIdSlot] = s_data.loadingAvatar;
		s_data.loadingAvatar = (rvrs_avatarOvr){ 0 };
		s_data.lastRequestedUserId = 0;
		s_data.lastRequestedUserIdSlot = -1;
	}
}

void rvrs_loadAvatarBodyMesh(rvrs_avatarOvr avatar, ovrAvatarAsset* asset)
{
	int32_t avatarIdx = avatar.id - 1;
	assert(avatarIdx >= 0);
	rvrs_avatarBodyOvr* avatarBody = &s_data.avatarBodiesArray[avatarIdx];

	const ovrAvatarMeshAssetDataV2* mesh = ovrAvatarAsset_GetCombinedMeshData(asset);

	rsys_print("Body Mesh:\n");
#ifdef AVATAR_CONSOLE
	rsys_consoleLog("Body Mesh:");
#endif
	avatarBody->mesh = rvrs_loadAvatarMeshOvr(asset, mesh, &avatarBody->inverseBindPoseArray, true);

	rvrs_avatarMeshLoadCallback(avatar);
#if 0
	// And blend shapes too.
	uint32_t blendShapeCount = ovrAvatarAsset_GetMeshBlendShapeCount(asset);
	assert(blendShapeCount != 0);

	const ovrAvatarBlendVertex* blendShapes = ovrAvatarAsset_GetMeshBlendShapeVertices(asset);

	// Blend shapes are large. cut size in half by using half floats. each uint64 is 4 half floats
#pragma pack(push, 1)
	struct BlendShapeVertex {
		uint64 Position; // w unused, just for padding
		uint64 Normal; // w unused
		uint64 Tangent; // w unused
	};
#pragma pack(pop)

	// prep data first
	std::vector<BlendShapeVertex> BlendShapes;
	uint32_t vertIndex = 0;
	for (uint32_t shape = 0; shape < blendShapeCount; ++shape) {
		for (uint32_t vertex = 0; vertex < mesh->vertexCount; ++vertex) {
			// make a copy of the vert, not safe to access directly, alignment bug in Avatar SDK
			ovrAvatarBlendVertex blendVert;
			memcpy((void*)&blendVert, (void*)(blendShapes + vertIndex), sizeof(ovrAvatarBlendVertex));

			BlendShapes.emplace_back();
			auto& shape = BlendShapes.back();
			shape.Position = packHalf4x16(vec4{ blendVert.x, blendVert.y, blendVert.z, 0.0f });
			shape.Normal = packHalf4x16(vec4{ blendVert.nx, blendVert.ny, blendVert.nz, 0.0f });
			shape.Tangent = packHalf4x16(vec4{ blendVert.tx, blendVert.ty, blendVert.tz, 0.0f });

			++vertIndex;
		}
	}

	BlendShapeSize = mesh->vertexCount;

	// now generate the actual texture buffer
	BShapeTextureBuffer = ovrGlBuffer(
		ovrGlBufferType_t::GLBUFFER_TYPE_TEXTURE_BUFFER,
		size(BlendShapes) * sizeof(BlendShapeVertex));
	assert(BShapeTextureBuffer.IsValid());
	BShapeTexture = ovrGlTexture(BShapeTextureBuffer, GL_RGBA16F);
	assert(BShapeTexture.IsValid());

	BlendShapeVertex* bshapeBuffer = BShapeTextureBuffer.Map<BlendShapeVertex>();
	memcpy(bshapeBuffer, data(BlendShapes), size(BlendShapes) * sizeof(BlendShapeVertex));
	BShapeTextureBuffer.Unmap();
#endif
	avatarBody->bMeshLoaded = true;
}

void rvrs_loadAvatarBodyTextures(rvrs_avatarOvr avatar, ovrAvatarAssetID id, const ovrAvatarTextureAssetData* tex)
{
	int32_t avatarIdx = avatar.id - 1;
//	assert(avatarIdx >= 0);
	if (avatarIdx < 0)
		return;

	rvrs_avatarBodyOvr* avatarBody = &s_data.avatarBodiesArray[avatarIdx];
	int32_t numTextures = sb_count(avatarBody->textureAssetArray);
	for (uint32_t i = 0; i < numTextures; ++i) 
	{
		if (avatarBody->textureAssetArray[i].albedo == id) 
		{
			// These checks are required because of some bad assets currently. These textures won't show
			// up.
			if (tex->format == ovrAvatarTextureFormat_DXT1) 
			{
				rsys_print("Warning: body albedo texture %llu was DXT1, should always be DXT5\n", id);
			}
			else 
			{
				rvrs_loadAvatarTextureOvr(&avatarBody->albedoTextures, i, tex, true);
				rsys_print("Load Body Albedo Texture\n", id);
			}
			avatarBody->textureAssetArray[i].albedo = 0;
		}
		if (avatarBody->textureAssetArray[i].normal == id)
		{
			if (tex->format == ovrAvatarTextureFormat_DXT5) 
			{
				rsys_print("Warning: body normal texture %llu was DXT5, should always be DXT1\n", id);
			}
			else {
				rvrs_loadAvatarTextureOvr(&avatarBody->normalTextures, i, tex, false);
				rsys_print("Load Body Normal Texture\n", id);
			}
			avatarBody->textureAssetArray[i].normal = 0;
		}
		if (avatarBody->textureAssetArray[i].metallic == id)
		{
			if (tex->format == ovrAvatarTextureFormat_DXT1)
			{
				rsys_print("Warning: body metallic texture %llu was DXT1, should always be DXT5\n", id);
			}
			else
			{
				rvrs_loadAvatarTextureOvr(&avatarBody->metallicTextures, i, tex, false);
				rsys_print("Load Body Metallic Texture\n", id);
			}
			avatarBody->textureAssetArray[i].metallic = 0;
		}
	}

	uint64_t sum = 0;
	for (int32_t i = 0; i < numTextures; ++i)
	{
		ovrAvatarBodyTextureSet* set = &avatarBody->textureAssetArray[i];
		sum += (set->albedo + set->normal + set->metallic);
	}
	if (sum == 0)
	{
		sb_free(avatarBody->textureAssetArray);
		avatarBody->textureAssetArray = NULL;
		rsys_print("All body textures loaded.\n");
		rvrs_avatarMeshLoadCallback(avatar);
	}
}

void rvrs_loadAvatarHandsMesh(rvrs_avatarOvr avatar, ovrAvatarAssetID id, ovrAvatarAsset* asset)
{
	int32_t avatarIdx = avatar.id - 1;
	assert(avatarIdx >= 0);
	rvrs_avatarHandsOvr* avatarHands = &s_data.avatarHandsArray[avatarIdx];

	int32_t hand = -1;

	if (sb_count(avatarHands->meshesToLoadArray) == 0)
	{
		return;
	}

	if (avatarHands->meshesToLoadArray[0] == id) 
	{
		hand = 0;
		avatarHands->meshesToLoadArray[0] = 0;
	}
	else if (avatarHands->meshesToLoadArray[1] == id) 
	{
		hand = 1;
		avatarHands->meshesToLoadArray[1] = 0;
	}
	else 
	{
		return; // not our mesh
	}

	const ovrAvatarMeshAssetData* mesh = ovrAvatarAsset_GetMeshData(asset);

	rsys_print("Hands Mesh:\n");
#ifdef AVATAR_CONSOLE
	rsys_consoleLog("Hands Mesh:");
#endif
	avatarHands->meshes[hand] = rvrs_loadAvatarMeshOvr(asset, mesh, &avatarHands->inverseBindPoseArray[hand], false);
	uint64_t sum = 0;
	const int32_t kNumMeshes = 2;
	for (int32_t i = 0; i < kNumMeshes; ++i)
	{
		sum += avatarHands->meshesToLoadArray[i];
	}
	if (sum == 0)
	{
		sb_free(avatarHands->meshesToLoadArray);
		avatarHands->meshesToLoadArray = NULL;
		rsys_print("Both hand meshes loaded.\n");

		rvrs_avatarMeshLoadCallback(avatar);
	}
}

void rvrs_loadAvatarHandsTextures(rvrs_avatarOvr avatar, ovrAvatarAssetID id, const ovrAvatarTextureAssetData* tex)
{
	int32_t avatarIdx = avatar.id - 1;
//	assert(avatarIdx >= 0);
	if (avatarIdx < 0)
		return;

	rvrs_avatarHandsOvr* avatarHands = &s_data.avatarHandsArray[avatarIdx];
	int32_t numTextures = sb_count(avatarHands->textureAssetArray);
	for (uint32_t i = 0; i < numTextures; ++i)
	{
		if (avatarHands->textureAssetArray[i].albedo == id)
		{
			// These checks are required because of some bad assets currently. These textures won't show
			// up.
			if (tex->format == ovrAvatarTextureFormat_DXT1)
			{
				rsys_print("Warning: body albedo texture %llu was DXT1, should always be DXT5\n", id);
			}
			else
			{
				rvrs_loadAvatarTextureOvr(&avatarHands->albedoTextures, i, tex, true);
				rsys_print("Load Hands Albedo Texture\n", id);
			}
			avatarHands->textureAssetArray[i].albedo = 0;
		}
		if (avatarHands->textureAssetArray[i].normal == id)
		{
			if (tex->format == ovrAvatarTextureFormat_DXT5)
			{
				rsys_print("Warning: body normal texture %llu was DXT5, should always be DXT1\n", id);
			}
			else {
				rvrs_loadAvatarTextureOvr(&avatarHands->normalTextures, i, tex, false);
				rsys_print("Load Hands Normal Texture\n", id);
			}
			avatarHands->textureAssetArray[i].normal = 0;
		}
	}

	uint64_t sum = 0;
	for (int32_t i = 0; i < numTextures; ++i)
	{
		ovrAvatarHandsTextureSet* set = &avatarHands->textureAssetArray[i];
		sum += (set->albedo + set->normal);
	}
	if (sum == 0)
	{
		sb_free(avatarHands->textureAssetArray);
		avatarHands->textureAssetArray = NULL;
		rsys_print("All hand textures loaded.\n");
		rvrs_avatarMeshLoadCallback(avatar);
	}
}

extern GLint g_viewIdLoc;

#define ION_PI (3.14159265359f)
#define ION_PI_OVER_180 (ION_PI / 180.0f)
#define RADIANS(x) ((x) * ION_PI_OVER_180)
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

int32_t invBindPoseIndices[3] = { -1, -1, -1 };
void rvrs_updateAvatarInstance(int32_t instanceIdx, float dt, const rvrs_avatarFrameParams* params)
{
	bool multipleLoaded = s_data.avatar[0].id > 0 && s_data.avatar[1].id > 0;
	int32_t avatarIdx = multipleLoaded ? s_data.avatar[instanceIdx].id - 1 : s_data.avatar[0].id - 1;
	if (avatarIdx < 0)
		return;

	int32_t bufferIndex = params->playerIndex;
	if (s_data.avatarInstanceArray == NULL)
	{
		sb_add(s_data.avatarInstanceArray, 2);
		for (int32_t i = 0; i < 2; ++i)
		{
			s_data.avatarInstanceArray[i].avatar = s_data.avatar[0];
			s_data.avatarInstanceArray[i].flags = 0;
			for (int32_t j = 0; j < 3; ++j) 
			{
				s_data.avatarInstanceArray[i].worldTransform[j] = mat4x4_identity();
			}
		}

		s_data.worldTransformBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
			.capacity = sizeof(mat4x4) * 2,										//CLR - Technically we only need 2, but due to GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT we need a minimum offset
			.stride = 0,														//		of 256 bytes if binding individually.
			.flags = kMapWriteBit
		});

		size_t bodyParamSize = sizeof(rvrs_avatarBodyVertParamsOvr);
		s_data.bodyMaterialVertexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
			.capacity = sizeof(rvrs_avatarBodyVertParamsOvr) * 2,
			.stride = 0,
			.flags = kMapWriteBit
		});

		size_t handsParamSize = sizeof(rvrs_avatarHands);
		s_data.handsMaterialVertexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
			.capacity = sizeof(rvrs_avatarHandsVertParamsOvr) * 2 * 2,	//2 hands, 2 instances?
			.stride = 0,
			.flags = kMapWriteBit
		});
	}
#if 1
		// Update Avatar
//		if (params.DeltaSeconds < 0.00005) {
//			return;
//		}

/*
		// Handle microphone audio
		MicAudio([&](auto& micAudio) {
			if (!micAudio.empty()) {
				LipSync.ProcessAudio(data(micAudio), (uint32_t)micAudio.size());
				micAudio.clear();
			}
			});
*/
// Update our shared uniform Buffer
//		ovrSharedParams* sharedParams = SharedParams.Map<ovrSharedParams>();
//		sharedParams->ViewPosition = params.EyePos;
//		sharedParams->LightPosition = LightPosition;
//		SharedParams.Unmap();

		// Update our SDK Avatar
	vec4 pos = params->eyePos;
	vec4 rot = params->eyeRot;
	ovrAvatarTransform bodyTransform;
	bodyTransform.orientation = (ovrAvatarQuatf){ rot.x, rot.y, rot.z, rot.w };
	bodyTransform.position = (ovrAvatarVector3f){ pos.x, pos.y, pos.z };
	bodyTransform.scale = (ovrAvatarVector3f){ 1.0f, 1.0f, 1.0f };

	ovrAvatarPose_UpdateBody(s_data.avatarArray[avatarIdx].avatarOvr, bodyTransform);

	ovrAvatarHandInputState hState[2] = { 0 };
	for (uint32_t hand = 0; hand < 2; ++hand)
	{
		uint32_t mirrorHand = hand;
		hState[mirrorHand].isActive = params->hands[hand].isActive;
		if (params->hands[mirrorHand].isActive)
		{
			vec4 pos = params->hands[hand].position;
			vec4 rot = params->hands[hand].rotation;

			ovrAvatarTransform handTransform;
			handTransform.orientation = (ovrAvatarQuatf){ rot.x, rot.y, rot.z, rot.w };
			handTransform.position = (ovrAvatarVector3f){ pos.x, pos.y, pos.z };
			handTransform.scale = (ovrAvatarVector3f){ 1.0f, 1.0f, 1.0f };

			hState[hand].transform = handTransform;
			hState[mirrorHand].joystickX = -params->hands[hand].joystickX;
			hState[mirrorHand].joystickY = params->hands[hand].joystickY;
			hState[mirrorHand].handTrigger = params->hands[hand].handTrigger;
			hState[mirrorHand].indexTrigger = params->hands[hand].indexTrigger;

			hState[mirrorHand].buttonMask = params->hands[hand].buttonMask;
			hState[mirrorHand].touchMask = params->hands[hand].touchMask;
		}
	}
	ovrAvatarPose_UpdateHandsWithType(s_data.avatarArray[avatarIdx].avatarOvr, hState[0], hState[1], ovrAvatarControllerType_Quest);
		/*
				ovrAvatarControllerType controllerType =
					(HmdType == HMDType::Rift ? ovrAvatarControllerType::ovrAvatarControllerType_Touch
						: ovrAvatarControllerType::ovrAvatarControllerType_Quest);
				ovrAvatarPose_UpdateHandsWithType(Avatar, hState[0], hState[1], controllerType);

				auto visemes = LipSync.GetVisemes();
				if (!visemes.empty()) {
					ovrAvatarVisemes sdkVisemes;
					sdkVisemes.visemeParamCount = (uint32_t)size(visemes);
					memcpy(sdkVisemes.visemeParams, data(visemes), sizeof(float) * size(visemes));
					ovrAvatar_SetVisemes(Avatar, &sdkVisemes);
				}
		*/
	ovrAvatarPose_Finalize(s_data.avatarArray[avatarIdx].avatarOvr, dt);

	int32_t baseRendIdx = s_data.avatarArray[avatarIdx].firstRenderable.id - 1;
	rvrs_avatarBodyOvr* avatarBody = &s_data.avatarBodiesArray[avatarIdx];
	if (baseRendIdx >= 0 && avatarBody->bMeshLoaded)
	{
		if (!avatarBody->combinedRenderPart)
		{
			assert(avatarBody->bodyComp);
			const ovrAvatarComponent* rcomp = avatarBody->bodyComp->renderComponent;

			// should be a single render part for the combined mesh.
			assert(rcomp->renderPartCount == 1);
			avatarBody->combinedRenderPart = rcomp->renderParts[0];
			avatarBody->combinedRenderComp = rcomp;
		}

		{
			int32_t meshIdx = (avatarBody->mesh.id - 1) - baseRendIdx;
			assert(meshIdx >= 0);

			// Body Frame
			assert(avatarBody->combinedRenderPart);
			/*
						const ovrAvatarBlendShapeParams* blendShapeParms = ovrAvatarSkinnedMeshRender_GetBlendShapeParams(avatarBody->combinedRenderPart);

						// Dont send more blends than we need. We do send a multiple of 4 though.
						vector<pair<int32_t, float>> importantBlendParams;
						importantBlendParams.clear();
						for (int32_t param = 0; param < (int32_t)blendShapeParms->blendShapeParamCount; ++param) {
							importantBlendParams.emplace_back(param, blendShapeParms->blendShapeParams[param]);
						}
						// sort so most important blends first
						sort(
							begin(importantBlendParams),
							end(importantBlendParams),
							[](const auto& arg1, const auto& arg2) { return arg1.second > arg2.second; });
						// now remove least important if there are more than we support in the shader.
						if (importantBlendParams.size() > MaxBlendParams) {
							importantBlendParams.resize(MaxBlendParams);
						}
						// We must have a multiple of 4, so pad with 0's to next multiple.
						while ((importantBlendParams.size() % BlendGroupSize) != 0) {
							importantBlendParams.emplace_back(0, 0.0f);
						}
						// now remove any that are too small to matter, have to do in group of 4's
						while (!importantBlendParams.empty() &&
							(importantBlendParams.end() - BlendGroupSize)->second < MinBlendWeight) {
							importantBlendParams.erase(importantBlendParams.end() - 4, importantBlendParams.end());
						}
			*/
//			rvrs_avatarBodyVertParamsOvr* matVertParms = (rvrs_avatarBodyVertParamsOvr*)rgfx_mapBuffer(avatarBody->materialVertexBuffer);
			rvrs_avatarBodyVertParamsOvr* matVertParms = (rvrs_avatarBodyVertParamsOvr*)rgfx_mapBufferRange(s_data.bodyMaterialVertexBuffer, sizeof(rvrs_avatarBodyVertParamsOvr)*bufferIndex, sizeof(rvrs_avatarBodyVertParamsOvr));
//			matVertParms = matVertParms + bufferIndex;
			/*
						for (int group = 0; group < (importantBlendParams.size() / BlendGroupSize); ++group) {
							matVertParms->BSIndices[group] = { importantBlendParams[BlendGroupSize * group + 0].first,
															  importantBlendParams[BlendGroupSize * group + 1].first,
															  importantBlendParams[BlendGroupSize * group + 2].first,
															  importantBlendParams[BlendGroupSize * group + 3].first };
							matVertParms->BSWeights[group] = { importantBlendParams[BlendGroupSize * group + 0].second,
															  importantBlendParams[BlendGroupSize * group + 1].second,
															  importantBlendParams[BlendGroupSize * group + 2].second,
															  importantBlendParams[BlendGroupSize * group + 3].second };
						}

						matVertParms->BSConstants.x = BlendShapeSize;
						matVertParms->BSConstants.y = (int)importantBlendParams.size() / BlendGroupSize;
						matVertParms->BSConstants.z = 0;
						matVertParms->BSConstants.w = 0;
			*/
			const ovrAvatarRenderPart_SkinnedMeshRenderPBS_V2* skinnedMesh = ovrAvatarRenderPart_GetSkinnedMeshRenderPBSV2(avatarBody->combinedRenderPart);
			//+1 to avoid a branch, if bone doesn't have parent(index will be -1) then use 0 index which will
			// be identity.
			mat4x4 worldPose[MAX_JOINTS + 1];
			worldPose[0] = mat4x4_identity();
			const int* jointParents = skinnedMesh->skinnedPose.jointParents;
			const ovrAvatarTransform* jointTransforms = skinnedMesh->skinnedPose.jointTransform;
			assert(skinnedMesh->skinnedPose.jointCount <= MAX_JOINTS);
			meshIdx = invBindPoseIndices[0];

			for (uint32_t i = 0; i < skinnedMesh->skinnedPose.jointCount; i++) {
				mat4x4 local = Convert(&jointTransforms[i]);

				const int parentIndex = jointParents[i];
				assert(parentIndex >= -1);
				worldPose[i + 1] = mat4x4_mul(worldPose[parentIndex + 1], local);
				matVertParms->bones[i] = mat4x4_mul(worldPose[i + 1], avatarBody->inverseBindPoseArray[i]);
			}

//			rgfx_unmapBuffer(avatarBody->materialVertexBuffer);
			rgfx_unmapBuffer(s_data.bodyMaterialVertexBuffer);

			mat4x4 compXForm = Convert(&avatarBody->combinedRenderComp->transform);
			mat4x4 meshLocalXForm = Convert(&skinnedMesh->localTransform);
			mat4x4 bodyTransform = mat4x4_mul(compXForm, meshLocalXForm);
			s_data.avatarInstanceArray[bufferIndex].worldTransform[0] = mat4x4_mul(params->avatarWorldMatrix, bodyTransform);
		}
	}
#if 1
	rvrs_avatarHandsOvr* avatarHands = &s_data.avatarHandsArray[avatarIdx];
	if (baseRendIdx >= 0 && avatarHands->meshesToLoadArray == NULL)
	{
		bool isAvatar = params->hands[0].isAvatar && params->hands[1].isAvatar;
		if (isAvatar)
		{
			// Hands Frame
			rvrs_avatarHandsVertParamsOvr* matVertParmsBuffer = (rvrs_avatarHandsVertParamsOvr*)rgfx_mapBuffer(s_data.handsMaterialVertexBuffer);
			for (uint32_t hand = 0; hand < kAvatarNumHands; ++hand)
			{
				//			int64_t rangeOffset = (sizeof(rvrs_avatarHandsVertParamsOvr) * 2 * hand) + (sizeof(rvrs_avatarHandsVertParamsOvr) * instanceIdx);
				//			int64_t rangeSize = sizeof(rvrs_avatarHandsVertParamsOvr);
				//			rvrs_avatarHandsVertParamsOvr* matVertParms = (rvrs_avatarHandsVertParamsOvr*)rgfx_mapBufferRange(s_data.handsMaterialVertexBuffer, rangeOffset, rangeSize);
				rvrs_avatarHandsVertParamsOvr* matVertParms = matVertParmsBuffer + (hand * 2) + bufferIndex;
				//			int32_t meshIdx = avatarHands->meshes[mirroredHand].id - 1;
				//			int32_t meshIdx = (avatarHands->meshes[mirroredHand].id - s_data.avatarArray[avatarIdx].firstRenderable.id) - 1;
				int32_t meshIdx = (avatarHands->meshes[hand].id - 1) - baseRendIdx;
				assert(meshIdx >= 0);

				const ovrAvatarRenderPart_SkinnedMeshRenderPBS_V2* skinnedMesh = ovrAvatarRenderPart_GetSkinnedMeshRenderPBSV2(avatarHands->renderParts[hand]);
				//+1 to avoid a branch, if bone doesn't have parent(index will be -1) then use 0 index which will
				// be identity.
				mat4x4 worldPose[MAX_JOINTS + 1];
				worldPose[0] = mat4x4_identity();
				const int* jointParents = skinnedMesh->skinnedPose.jointParents;
				const ovrAvatarTransform* jointTransforms = skinnedMesh->skinnedPose.jointTransform;
				assert(skinnedMesh->skinnedPose.jointCount <= MAX_JOINTS);
				meshIdx = invBindPoseIndices[1 + hand];
				for (uint32_t i = 0; i < skinnedMesh->skinnedPose.jointCount; i++) {
					mat4x4 local = Convert(&jointTransforms[i]);

					const int parentIndex = jointParents[i];
					assert(parentIndex >= -1);
					worldPose[i + 1] = mat4x4_mul(worldPose[parentIndex + 1], local);

					matVertParms->bones[i] = mat4x4_mul(worldPose[i + 1], avatarHands->inverseBindPoseArray[hand][i]);
				}

				mat4x4 compXForm = Convert(&avatarHands->renderComps[hand]->transform);
				mat4x4 meshLocalXForm = Convert(&skinnedMesh->localTransform);
				mat4x4 handTransform = mat4x4_mul(compXForm, meshLocalXForm);
				s_data.avatarInstanceArray[bufferIndex].worldTransform[1 + hand] = mat4x4_mul(params->avatarWorldMatrix, handTransform);
			}
			rgfx_unmapBuffer(s_data.handsMaterialVertexBuffer);
		}
#ifdef VRSYSTEM_OCULUS_MOBILE
		else
		{
			rvrs_handRenderData* renderData = rvrs_getHandRenderData();
			rvrs_handsVertParamsOvr* matVertParmsBuffer = (rvrs_handsVertParamsOvr*)rgfx_mapBuffer(renderData->vertexParams);
			for (uint32_t hand = 0; hand < kAvatarNumHands; ++hand)
			{
				ovrPosef* handPose = rvrs_getHandPose(hand);
				ovrHandSkeleton* handSkeleton = rvrs_getHandSkeleton(hand);
				ovrQuatf* handRots = rvrs_getHandBoneRotations(hand);
				mat4x4 handScale = rvrs_getHandScale(hand);
				if (handSkeleton != NULL)
				{
					mat4x4 worldPose[MAX_JOINTS + 1];
					worldPose[0] = mat4x4_identity();
					const int16_t* jointParents = handSkeleton->BoneParentIndices;
					assert(handSkeleton->NumBones < MAX_JOINTS);
					rvrs_handsVertParamsOvr* matVertParms = matVertParmsBuffer + hand;

					for (uint32_t i = 0; i < handSkeleton->NumBones; i++)
					{
						ovrPosef currentPose;
						currentPose.Orientation = handRots[i];
						currentPose.Position = handSkeleton->BonePoses[i].Translation;
						mat4x4 local = ConvertPose(&currentPose);

						const int16_t parentIndex = jointParents[i];
						assert(parentIndex >= -1);
						worldPose[i + 1] = mat4x4_mul(worldPose[parentIndex + 1], local);
						matVertParms->bones[i] = mat4x4_mul(worldPose[i + 1], renderData->inverseBindPoseArrays[hand][i]);
					}

					mat4x4 displayPose;
					mat4x4 handTransform = mat4x4_mul(ConvertPose(handPose), handScale);
/*
					displayPose = handTransform;
					for (int32_t i = 0; i < 4; ++i)
					{
						rsys_consoleLog("Hand %d: %.3f, %.3f, %.3f, %.3f", hand, displayPose.axis[i].x, displayPose.axis[i].y, displayPose.axis[i].z, displayPose.axis[i].w);
					}
*/
					s_data.avatarInstanceArray[instanceIdx].worldTransform[1 + hand] = mat4x4_mul(params->avatarWorldMatrix, handTransform);
				}
			}
			rgfx_unmapBuffer(renderData->vertexParams);
		}		
#endif
	}
#endif
#endif
	s_data.playerIndex = params->playerIndex;
}

void rvrs_renderAvatarsOvr(int32_t eye, const rvrs_avatarFrameParams* params)
{
	for (int32_t avatar = 0; avatar < 2; ++avatar)
	{
		int32_t index = 1-avatar;
		int32_t avatarIdx = s_data.avatar[index].id - 1;
		if (avatarIdx < 0)
			continue;

		rvrs_avatarBodyOvr* avatarBody = &s_data.avatarBodiesArray[avatarIdx];
		bool shouldDraw = !rvrs_isHmdMounted() || (rvrs_isHmdMounted() && index != 0);
		if (avatarBody->bMeshLoaded && shouldDraw)
		{
			{
				rgfx_bindPipeline(s_data.avatarBodyPipeline);

				GLuint program = rgfx_getPipelineProgram(s_data.avatarBodyPipeline, kVertexShader);
				if (g_viewIdLoc >= 0)  // NOTE: will not be present when multiview path is enabled.
				{
					glProgramUniform1i(program, g_viewIdLoc, eye);
				}

				mat4x4* matrixPtr = (mat4x4*)rgfx_mapBuffer(s_data.worldTransformBuffer);
				if (avatar == 0)
				{
					matrixPtr[0] = s_data.avatarInstanceArray[0].worldTransform[0];
				}
				else
				{
					matrixPtr[0] = s_data.avatarInstanceArray[1].worldTransform[0];
				}
				rgfx_unmapBuffer(s_data.worldTransformBuffer);
				if (avatarBody->albedoTextures.id > 0)
				{
					rgfx_bindTexture(0, avatarBody->albedoTextures);
				}
				if (avatarBody->normalTextures.id > 0)
				{
					rgfx_bindTexture(1, avatarBody->normalTextures);
				}
				if (avatarBody->metallicTextures.id > 0)
				{
					rgfx_bindTexture(2, avatarBody->metallicTextures);
				}

				if (s_data.worldTransformBuffer.id > 0) {
					rgfx_bindBufferRange(0, s_data.worldTransformBuffer, 0, sizeof(mat4x4));
				}

				rgfx_bindBuffer(1, rgfx_getTransforms());

				if (s_data.bodyMaterialVertexBuffer.id > 0) {
					rgfx_bindBufferRange(2, s_data.bodyMaterialVertexBuffer, sizeof(rvrs_avatarBodyVertParamsOvr) * avatar, sizeof(rvrs_avatarBodyVertParamsOvr));
				}

				rgfx_bindBuffer(3, rgfx_getLightsBuffer());

				if (avatarBody->materialFragmentBuffer.id > 0) {
					rgfx_bindBuffer(4, avatarBody->materialFragmentBuffer);
				}

				rgfx_drawRenderableInstanced(avatarBody->mesh, 1); // : 2);
				rgfx_unbindVertexBuffer();
			}
		}

		rvrs_avatarHandsOvr* avatarHands = &s_data.avatarHandsArray[avatarIdx];
		if (avatarHands->meshesToLoadArray == NULL)
		{
			{
				// Hands Render
				int32_t currentVbIdx = -1;
				rgfx_bindPipeline(s_data.avatarHandsPipeline);

				for (uint32_t hand = 0; hand < kAvatarNumHands; ++hand)
				{
					GLuint vertexProgram = rgfx_getPipelineProgram(s_data.avatarHandsPipeline, kVertexShader);
					if (g_viewIdLoc >= 0)  // NOTE: will not be present when multiview path is enabled.
					{
						glProgramUniform1i(vertexProgram, g_viewIdLoc, eye);
					}

					GLuint fragmentProgram = rgfx_getPipelineProgram(s_data.avatarHandsPipeline, kFragmentShader);
					if (s_data.avatarHandsUniformIndexHand >= 0)  // NOTE: will not be present when multiview path is enabled.
					{
						glProgramUniform1i(fragmentProgram, s_data.avatarHandsUniformIndexHand, hand);
					}

					mat4x4* matPtr = (mat4x4*)rgfx_mapBuffer(s_data.worldTransformBuffer);
					if (avatar == 0)
					{
						matPtr[0] = s_data.avatarInstanceArray[0].worldTransform[1 + hand];
					}
					else
					{
						matPtr[0] = s_data.avatarInstanceArray[1].worldTransform[1 + hand];
					}
					rgfx_unmapBuffer(s_data.worldTransformBuffer);

					if (avatarHands->albedoTextures.id > 0)
					{
						rgfx_bindTexture(0, avatarHands->albedoTextures);
					}

					if (avatarHands->normalTextures.id > 0)
					{
						rgfx_bindTexture(1, avatarHands->normalTextures);
					}

					if (s_data.worldTransformBuffer.id > 0) {
						rgfx_bindBuffer(0, s_data.worldTransformBuffer);
					}

					rgfx_bindBuffer(1, rgfx_getTransforms());

					if (s_data.handsMaterialVertexBuffer.id > 0) {
						rgfx_bindBufferRange(2, s_data.handsMaterialVertexBuffer, (hand * 2 + avatar) * sizeof(rvrs_avatarHandsVertParamsOvr), sizeof(rvrs_avatarHandsVertParamsOvr)/* * 2*/);
					}
					rgfx_drawRenderableInstanced(avatarHands->meshes[hand], 1);
				}
				rgfx_unbindVertexBuffer();
			}
		}
#if 0
		if (params->hands[1].isAvatar == false)
		{
			const rvrs_handRenderData* renderData = rvrs_getHandRenderData();
			// Hands Render
			int32_t currentVbIdx = -1;
			rgfx_bindPipeline(renderData->pipeline);

			for (uint32_t hand = 0; hand < kAvatarNumHands; ++hand)
			{
				GLuint vertexProgram = rgfx_getPipelineProgram(renderData->pipeline, kVertexShader);
				if (g_viewIdLoc >= 0)  // NOTE: will not be present when multiview path is enabled.
				{
					glProgramUniform1i(vertexProgram, g_viewIdLoc, eye);
				}

				GLuint fragmentProgram = rgfx_getPipelineProgram(renderData->pipeline, kFragmentShader);
				if (s_data.avatarHandsUniformIndexHand >= 0)  // NOTE: will not be present when multiview path is enabled.
				{
					glProgramUniform1i(fragmentProgram, renderData->uniformIndexHand, hand);
				}

				mat4x4* matPtr = (mat4x4*)rgfx_mapBuffer(s_data.worldTransformBuffer);
				matPtr[0] = s_data.avatarInstanceArray[1].worldTransform[1 + hand];
				rgfx_unmapBuffer(s_data.worldTransformBuffer);

				if (avatarHands->albedoTextures.id > 0)
				{
					rgfx_bindTexture(0, avatarHands->albedoTextures);
				}
				/*
							if (avatarHands->normalTextures.id > 0)
							{
								rgfx_bindTexture(1, avatarHands->normalTextures);
							}
				*/
				if (s_data.worldTransformBuffer.id > 0) {
					rgfx_bindBuffer(0, s_data.worldTransformBuffer);
				}

				rgfx_bindBuffer(1, rgfx_getTransforms());

				if (renderData->vertexParams.id > 0) {
					rgfx_bindBufferRange(2, renderData->vertexParams, hand * sizeof(rvrs_avatarHandsVertParamsOvr), sizeof(rvrs_avatarHandsVertParamsOvr));
				}
				rgfx_drawRenderableInstanced(renderData->rend[hand], 1);
			}
			rgfx_unbindVertexBuffer();
		}
#endif
	}
}

void rvrs_renderAvatarOvr(int32_t eye, const rvrs_avatarFrameParams* params)
{
	bool multipleLoaded = s_data.avatar[0].id > 0 && s_data.avatar[1].id > 0;
	if (multipleLoaded)
	{
		rvrs_renderAvatarsOvr(eye, params);
		return;
	}

	int32_t avatarIdx = s_data.avatar[0].id - 1;
	if (avatarIdx < 0)
		return;

	rvrs_avatarBodyOvr* avatarBody = &s_data.avatarBodiesArray[avatarIdx];
	if (avatarBody->bMeshLoaded)
	{
		{
			rgfx_bindPipeline(s_data.avatarBodyPipeline);

			GLuint program = rgfx_getPipelineProgram(s_data.avatarBodyPipeline, kVertexShader);
			if (g_viewIdLoc >= 0)  // NOTE: will not be present when multiview path is enabled.
			{
				glProgramUniform1i(program, g_viewIdLoc, eye);
			}

			mat4x4* matrixPtr = (mat4x4*)rgfx_mapBuffer(s_data.worldTransformBuffer);
			matrixPtr[0] = s_data.avatarInstanceArray[0].worldTransform[0];
			matrixPtr[1] = s_data.avatarInstanceArray[1].worldTransform[0];
			rgfx_unmapBuffer(s_data.worldTransformBuffer);
			if (avatarBody->albedoTextures.id > 0)
			{
				rgfx_bindTexture(0, avatarBody->albedoTextures);
			}

			if (avatarBody->normalTextures.id > 0)
			{
				rgfx_bindTexture(1, avatarBody->normalTextures);
			}
			if (avatarBody->metallicTextures.id > 0)
			{
				rgfx_bindTexture(2, avatarBody->metallicTextures);
			}

			if (s_data.worldTransformBuffer.id > 0) {
				rgfx_bindBuffer(0, s_data.worldTransformBuffer);
			}

			rgfx_bindBuffer(1, rgfx_getTransforms());

			if (s_data.bodyMaterialVertexBuffer.id > 0) {
				rgfx_bindBuffer(2, s_data.bodyMaterialVertexBuffer);
			}

			rgfx_bindBuffer(3, rgfx_getLightsBuffer());

			if (avatarBody->materialFragmentBuffer.id > 0) {
				rgfx_bindBuffer(4, avatarBody->materialFragmentBuffer);
			}

			rgfx_drawRenderableInstanced(avatarBody->mesh, rvrs_isHmdMounted() ? 1 : 2);
			rgfx_unbindVertexBuffer();
		}
	}
#if 1
	rvrs_avatarHandsOvr* avatarHands = &s_data.avatarHandsArray[avatarIdx];
	if (avatarHands->meshesToLoadArray == NULL)
	{
		{
			// Hands Render
			int32_t currentVbIdx = -1;
			rgfx_bindPipeline(s_data.avatarHandsPipeline);

			for (uint32_t hand = 0; hand < kAvatarNumHands; ++hand)
			{
				GLuint vertexProgram = rgfx_getPipelineProgram(s_data.avatarHandsPipeline, kVertexShader);
				if (g_viewIdLoc >= 0)  // NOTE: will not be present when multiview path is enabled.
				{
					glProgramUniform1i(vertexProgram, g_viewIdLoc, eye);
				}

				GLuint fragmentProgram = rgfx_getPipelineProgram(s_data.avatarHandsPipeline, kFragmentShader);
				if (s_data.avatarHandsUniformIndexHand >= 0)  // NOTE: will not be present when multiview path is enabled.
				{
					glProgramUniform1i(fragmentProgram, s_data.avatarHandsUniformIndexHand, hand);
				}

				mat4x4* matPtr = (mat4x4*)rgfx_mapBuffer(s_data.worldTransformBuffer);
				matPtr[0] = s_data.avatarInstanceArray[0].worldTransform[1 + hand];
				matPtr[1] = s_data.avatarInstanceArray[1].worldTransform[1 + hand];
				rgfx_unmapBuffer(s_data.worldTransformBuffer);

				if (avatarHands->albedoTextures.id > 0)
				{
					rgfx_bindTexture(0, avatarHands->albedoTextures);
				}

				if (avatarHands->normalTextures.id > 0)
				{
					rgfx_bindTexture(1, avatarHands->normalTextures);
				}

				if (s_data.worldTransformBuffer.id > 0) {
					rgfx_bindBuffer(0, s_data.worldTransformBuffer);
				}

				rgfx_bindBuffer(1, rgfx_getTransforms());

				if (s_data.handsMaterialVertexBuffer.id > 0) {
					rgfx_bindBufferRange(2, s_data.handsMaterialVertexBuffer, hand * (2 * sizeof(rvrs_avatarHandsVertParamsOvr)), sizeof(rvrs_avatarHandsVertParamsOvr) * 2);
				}
				rgfx_drawRenderableInstanced(avatarHands->meshes[hand], params->hands[1].isAvatar ? 2 : 1);
			}
			rgfx_unbindVertexBuffer();
		}
	}
	if (params->hands[1].isAvatar == false)
	{
		const rvrs_handRenderData* renderData = rvrs_getHandRenderData();
		// Hands Render
		int32_t currentVbIdx = -1;
		rgfx_bindPipeline(renderData->pipeline);

		for (uint32_t hand = 0; hand < kAvatarNumHands; ++hand)
		{
			GLuint vertexProgram = rgfx_getPipelineProgram(renderData->pipeline, kVertexShader);
			if (g_viewIdLoc >= 0)  // NOTE: will not be present when multiview path is enabled.
			{
				glProgramUniform1i(vertexProgram, g_viewIdLoc, eye);
			}

			GLuint fragmentProgram = rgfx_getPipelineProgram(renderData->pipeline, kFragmentShader);
			if (s_data.avatarHandsUniformIndexHand >= 0)  // NOTE: will not be present when multiview path is enabled.
			{
				glProgramUniform1i(fragmentProgram, renderData->uniformIndexHand, hand);
			}

			mat4x4* matPtr = (mat4x4*)rgfx_mapBuffer(s_data.worldTransformBuffer);
			matPtr[0] = s_data.avatarInstanceArray[1].worldTransform[1 + hand];
			rgfx_unmapBuffer(s_data.worldTransformBuffer);

			if (avatarHands->albedoTextures.id > 0)
			{
				rgfx_bindTexture(0, avatarHands->albedoTextures);
			}
/*
			if (avatarHands->normalTextures.id > 0)
			{
				rgfx_bindTexture(1, avatarHands->normalTextures);
			}
*/
			if (s_data.worldTransformBuffer.id > 0) {
				rgfx_bindBuffer(0, s_data.worldTransformBuffer);
			}

			rgfx_bindBuffer(1, rgfx_getTransforms());

			if (renderData->vertexParams.id > 0) {
				rgfx_bindBufferRange(2, renderData->vertexParams, hand * sizeof(rvrs_avatarHandsVertParamsOvr), sizeof(rvrs_avatarHandsVertParamsOvr));
			}
			rgfx_drawRenderableInstanced(renderData->rend[hand], 1);
		}
		rgfx_unbindVertexBuffer();
	}
#endif
}

rvrs_avatarOvr rvrs_getAvatar(int32_t slot)
{
	return s_data.avatar[slot];
}

#endif //VRSYSTEM_OCULUS_WIN
