#include "stdafx.h"
#include "game.h"
#include "stb/stretchy_buffer.h"

#include "engine/math/mat4x4.h"
#include "engine/math/vec4.h"
#include "engine/math/quat.h"
#include "engine/rgfx/renderer.h"
#include "engine/resource.h"
#include "engine/vrsystem.h"
#include "engine/audio.h"
#include "engine/system.h"
#include "engine/font.h"
#include "engine/hash.h"
#include "engine/fourcc.h"

//#define FORCE_GUI
#define DISABLE_LOGO
#define NP_FIXED_POINT
#define INCLUDE_AVATAR_DATA
//#define DISABLE_VOIP
//#define DISABLE_ROOM_BUTTON
//#define SHOW_CONSOLE
//#define SHOW_PERF_HUD

#ifdef FORCE_GUI
#define DISABLE_LOGO
#endif

#ifdef VRSYSTEM_OCULUS_WIN
#define OCULUS_APP_ID ""		//Your Rift Store APP_ID here for Platform Services / Avatars etc.
#else
#define OCULUS_APP_ID ""		//Your Quest Store APP_ID here for Platform Services / Avatars etc.
#endif

#define ION_PI (3.14159265359f)
#define ION_180_OVER_PI (180.0f / ION_PI)
#define ION_PI_OVER_180 (ION_PI / 180.0f)

#define DEGREES(x) ((x) * ION_180_OVER_PI)
#define RADIANS(x) ((x) * ION_PI_OVER_180)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MIN_ID(a, b) ((a.id) < (b.id) ? (a) : (b))
#define MAX_ID(a, b) ((a.id) > (b.id) ? (a) : (b))

#define MAX_SUPPORTED_REFRESH_RATES 6

#define GAME_TO_WORLD(x) ((x) * kGameToWorldScale)

const uint16_t kSettingsVersionMajor = 1;
const uint16_t kSettingsVersionMinor = 1;

const int32_t kSuggestedEyeScaleForRefreshRateQuest[MAX_SUPPORTED_REFRESH_RATES] = { 1, 0, 0, 0, 0, 0 };
const int32_t kSuggestedFFRForRefreshRateQuest[MAX_SUPPORTED_REFRESH_RATES] = { 2, -1, -1, -1, -1, -1 };

const int32_t kSuggestedEyeScaleForRefreshRateQuest2[MAX_SUPPORTED_REFRESH_RATES] = { 3, 2, 0, 0, 0, 0 };
const int32_t kSuggestedFFRForRefreshRateQuest2[MAX_SUPPORTED_REFRESH_RATES] = { 2, 3, 3, -1, -1, -1 };

const int32_t kPlayerAvatarSlot = 0;
const int32_t kOpponentAvatarSlot = 1;
const int32_t kNumTableParts = 8;

const vec4 kPlayerCamPos[2] = { 
	{0.0f, 0.0f, 1.5f, 1.0f},
	{0.0f, 0.0f, -1.5f, 1.0f},
};

const float kGameToWorldScale = 0.115f;
const float kDefaultTableHeight = 0.78f;
const float kTableOffsetScale = 0.01f;
float kVolumeScale = 10.0f;

rengApi reng = {};
rsysApi rsys = {};
rresApi rres = {};
rgfxApi rgfx = {};
raudApi raud = {};
rvrsApi rvrs = {};
rguiApi rgui = {};

rgfx_meshInstance minId = { 0x7fffffff };
rgfx_meshInstance maxId = { 0x0 };

typedef enum Hand
{
	kLeftHand = 0,
	kRightHand
}Hand;

typedef enum AANetPacketType
{
	kNPUnknown,
	kNPNetStart,
	kNPClockSyncRequest,
	kNPClockSyncResponse,
	kAANPPlayerAvatar,
}AANetPacketType;

typedef struct NetStartPacket
{
	rvrp_netPacketHeader header;
	int64_t serverClock;			//Server time
	int64_t pingTime;				//Last ping time for this user
}NetStartPacket;

typedef struct ClockSyncRequestPacket
{
	rvrp_netPacketHeader header;
	int64_t clientClock;			//Client time 
}ClockSyncRequestPacket;

typedef struct ClockSyncResponsePacket
{
	rvrp_netPacketHeader header;
	int64_t clientClock;			//Client time
	int64_t serverClock;			//Server time
}ClockSyncResponsePacket;


typedef int16_t fixed16;
typedef uint8_t ufixed8;

typedef struct GamePlayerAvatarPacket
{
	rvrp_netPacketHeader header;
	uint32_t flagsButtTouch;
	int64_t timeStamp;
	fixed16 headRot[4];
	fixed16 leftHandRot[4];
	fixed16 rightHandRot[4];
	fixed16 headPos[3];
	fixed16 leftHandPos[3];
	fixed16 rightHandPos[3];
	ufixed8 leftHandGrip;
	ufixed8 leftHandTrigger;
	ufixed8 rightHandGrip;
	ufixed8 rightHandTrigger;
}GamePlayerAvatarPacket;

typedef enum GUIMode
{
	kGUINone = 0,
	kGUIOptions,
	kGUISettings,
	kGUIOnline,
	kGUICredits,
	kGUIWarning,
	kGUIOnlineWarning,
}GUIMode;

typedef enum RoomState
{
	kRSSolo = 0,
	kRSCreatingRoom,
	kRSCreatedAwaitingFriendsList,
	kRSCreatedWithFriendsList,
	kRSCreatedNetRunning,
	kRSDestroyRoom,
}RoomState;

typedef struct RoomData
{
	uint64_t roomID;
	uint64_t roomOwner;
	int64_t netClock;
	int64_t prevTick;
	int64_t syncRequestClientClock;
	RoomState state;
	int32_t numFriends;
	rvrs_userOvr* friendsList;					// List of players friends.
	rvrs_userOvr* userArray;					// List of players in the room you are in.
	char* userNameBuffer;
	char* inviteTokenBuffer;
	int32_t userNameBufferSize;
	rsys_hash friendLookup;						// Hashtable to find a lookup a friend.
	rsys_hash userLookup;						// Hashtable to look up a user.  Is this needed if we are limiting to a max of 4 players in a room?
}RoomData;

typedef struct GameData
{
	rvrs_rigidBodyPose headPose;
	vec4 headOrientation;
	vec4 headPosition;
	rvrs_rigidBodyPose handPoses[2];
	uint32_t playerButtons;
	uint32_t playerTouches;
	quat opponentHeadRot;
	vec4 opponentHeadPos;
	quat opponentHandsRot[2];
	vec4 opponentHandsPos[2];
	uint32_t opponentButtons;
	uint32_t opponentTouches;
	uint32_t opponentAvatarFlags;
	float opponentGrip[2];
	float opponentTrigger[2];
	mat4x4 projectionMatrix;
	mat4x4 camViewMatrix;
	mat4x4 camWorldMatrix;
	mat4x4 guiTransformMain;
	GUIMode guiMode;
	GUIMode lastGuiMode;
	RoomData roomData;
	int32_t screenWidth;
	int32_t screenHeight;
	uint64_t userId;
	int32_t playerIndex;
	int32_t roomPanelUserListScrollPos;
	Hand lastUsedHand;
	bool hmdWasMounted;
	bool bRequestScreenshot;
	bool bEntitled;
	bool bSettingsWarningHeeded;
	bool bOnlineWarningHeeded;
	float mousePosNdcX;
	float mousePosNdcY;
	float leftHandGripValue;
	float leftHandTriggerValue;
	float rightHandGripValue;
	float rightHandTriggerValue;
	float leftControllerVibrationTime;
	float rightControllerVibrationTime;
	float masterVolume;
	float difficultySlider;
	float currentFramerateSlider;
	float requestedFramerateSlider;
	float currentEyeScaleSlider;
	float requestedEyeScaleSlider;
	float currentFoveationSlider;
	float requestedFoveationSlider;
	float roomTypeSlider;
	float brightness;
	float prevBrightness;
	float lightBrightness[4];
	float supportedRefreshRates[MAX_SUPPORTED_REFRESH_RATES];
	int32_t recomendedeFFRForRefreshRate[MAX_SUPPORTED_REFRESH_RATES];
	int32_t numSupportedRefreshRates;
	int32_t avatarIndex;
	int32_t screenshotCountdown;
	int32_t playerCount;
	uint64_t updateTime;
	uint64_t renderTime;
	uint32_t keyPresses;
	uint32_t prevKeyPresses;
	uint32_t mousePresses;
	rgfx_renderStats renderStats;
	rgfx_pass msaaPass;
	rgfx_pipeline blinnphongPipeline;
	rgfx_pipeline blinnphongNormalMappedPipeline;
	rgfx_pipeline blinnphongSkinnedPipeline;
	rgfx_pipeline blinnphongEmissivePipeline;
	rgfx_meshInstance ahScene;
	rgfx_texture scoreTextures[10];
	rgui_texture sliderTexture;
	rgui_texture scrollUpButtonTexture;
	rgui_texture scrollDownButtonTexture;
	rgui_texture prevButtonTexture;
	rgui_texture nextButtonTexture;
	rgui_texture tableButtonBlueTexture;
	rgui_texture tableButtonTexture;
	rgui_texture tableButtonRedTexture;
	rgfx_texture photoTestTextureArray;
	rgfx_texture wallAlbedoTextureArray;
	rgfx_texture wall4AlbedoTextureArray;
	rgui_font font;
	raud_sound guiClickSound;
}GameData;

static GameData s_gameData = { 0 };

float getRefreshRateSliderValue(float refreshRate);
uint64_t cycleAvatars(int32_t dir);
void updateRoom(float dt);
void updateScore(int32_t scoringPlayerIndex);
int32_t updateNetworkFromFile(float dt, int32_t numPackets);
int32_t updateFromNetwork(float dt);
void updateNetwork(float dt);
bool clearTimeoutPacket(int64_t timestamp, uint32_t packetType);
void netConnect(uint64_t userId);
void voipConnect(uint64_t userId);
void voipUpdate(void);
int voipRead(void* sound, void* data, unsigned int length);

void netSendPlayerAvatarPacket(void);
void updateAvatar(float dt);
void updateNetworkAvatar(float dt);
void showGUI(float dt);
void showOptionsPanel(void);
void showSettingsPanel(void);
void showSupportPanel(void);
void showCreditsPanel(void);
void showOnlinePanel(void);
void showWarningPanel(void);
void showOnlineWarningPanel(void);
void showPerfHUD(float dt);

void createPrivateRoom(int32_t maxUsers);
void destroyRoom(void);
void buildFriendsList(int32_t numFriends, rvrs_userOvr* friendsList);
void roomUpdateMemberList(uint64_t roomId, int32_t numUsers, rvrs_userOvr* userList, rvrp_roomCallbackType type);


void game_initialize(const int32_t width, const int32_t height, const rengApi *_reng)
{
	memset(&s_gameData, 0, sizeof(GameData));

	reng = *_reng;
	rsys = reng.rsys;
	rres = reng.rres;
	rgfx = reng.rgfx;
	raud = reng.raud;
	rvrs = reng.rvrs;
	rgui = reng.rgui;

//CLR - Set some sensible defaults.
	s_gameData.difficultySlider = 1.0f;
	s_gameData.masterVolume = 11.0f;
	s_gameData.requestedFramerateSlider = s_gameData.currentFramerateSlider = 0.0f;
	s_gameData.roomTypeSlider = 0.0f;
	s_gameData.brightness = 1.0f;
	int32_t currentFramerateSlider = (int32_t)s_gameData.currentFramerateSlider;
	s_gameData.requestedEyeScaleSlider = rvrs.getDeviceType() == kVrDeviceTypeQuest2 ? kSuggestedEyeScaleForRefreshRateQuest2[currentFramerateSlider] : rvrs.getDeviceType() == kVrDeviceTypeQuest ? kSuggestedEyeScaleForRefreshRateQuest[currentFramerateSlider] : 0;
	s_gameData.requestedFoveationSlider = rvrs.getDeviceType() == kVrDeviceTypeQuest2 ? kSuggestedFFRForRefreshRateQuest2[currentFramerateSlider] : rvrs.getDeviceType() == kVrDeviceTypeQuest ? kSuggestedFFRForRefreshRateQuest[currentFramerateSlider] : 0;
	kVolumeScale = s_gameData.masterVolume;

	s_gameData.screenWidth = width;
	s_gameData.screenHeight = height;
#ifdef DEMO_HUD
	s_gameData.lightBrightness[0] = 1.0f;
	s_gameData.lightBrightness[1] = 1.0f;
	s_gameData.lightBrightness[2] = 1.0f;
	s_gameData.lightBrightness[3] = 1.0f;
#endif

	rres_resourceDesc resources[] =
	{
		{ "AHScene.s3d" , kRTMesh },
		{ "Wall1_ao.png", kRTTexture },
		{ "Wall2_ao.png", kRTTexture },
		{ "Wall3_ao.png", kRTTexture },
		{ "Wall4_ao.png", kRTTexture },
		{ "Floor_ao.png", kRTTexture },
		{ "calibri.ttf", kRTFont },
		{ "click.wav", kRTSound },
	};

	uint32_t numResources = STATIC_ARRAY_SIZE(resources);
	for (uint32_t i = 0; i < numResources; ++i)
	{
		rres.registerResource(resources[i]);
	}

	const char* photoTestArrayNames[] = { 
		"phototest_albedo.png",
		"phototest2_albedo.png",
		"phototest3_albedo.png",
		"phototest4_albedo.png",
		"phototest5_albedo.png",
		"phototest6_albedo.png",
		"phototest7_albedo.png",
	};

	s_gameData.photoTestTextureArray = rgfx.createTextureArray(photoTestArrayNames, STATIC_ARRAY_SIZE(photoTestArrayNames));

	const char* albedoTextureArrayNames[] = {
		"Wall2_ao.png",
		"Wall3_ao.png",
		"Wall1_ao.png",
	};

	s_gameData.wallAlbedoTextureArray = rgfx.createTextureArray(albedoTextureArrayNames, STATIC_ARRAY_SIZE(albedoTextureArrayNames));

	const char* albedoTexture2ArrayNames[] = {
		"Wall4_ao.png",
	};

	s_gameData.wall4AlbedoTextureArray = rgfx.createTextureArray(albedoTexture2ArrayNames, STATIC_ARRAY_SIZE(albedoTexture2ArrayNames));

	mat4x4 transform = mat4x4_identity();
	s_gameData.ahScene = rgfx.addMeshInstance(&(rgfx_meshInstanceDesc) {
		.instanceName = "Scene",
		.meshName = "AHScene.s3d",
		.transform = transform,
	});

	s_gameData.font = rres.findFont(hashString("calibri.ttf"));
	s_gameData.guiClickSound = rres.findSound(hashString("click.wav"));

	uint32_t pictureFrameMesh = hashString("PictureFrame");
	uint32_t canvasMaterial = hashString("CanvasMtl");
	rgfx.updateMeshMaterial(pictureFrameMesh, &(rgfx_meshInstanceDesc) {
		.changeMaterialFlags = kMFDiffuseMap,
		.changeMaterialNameHash = canvasMaterial,
		.changeMaterialTexture = s_gameData.photoTestTextureArray,
		.newMaterialFlags = kMFDiffuseMap | kMFInstancedAlbedo,
	});

	uint32_t wallMesh = hashString("AHRoom:wall2");
	uint32_t wallMaterial = hashString("Wall2Mtl");
	rgfx.updateMeshMaterial(wallMesh, &(rgfx_meshInstanceDesc) {
		.changeMaterialFlags = kMFAmbientMap,
		.changeMaterialNameHash = wallMaterial,
		.changeMaterialTexture = s_gameData.wallAlbedoTextureArray,
		.newMaterialFlags = kMFDiffuseMap | kMFAmbientMap | kMFInstancedAO,
	});

	wallMesh = hashString("AHRoom:wall4");
	wallMaterial = hashString("Wall4Mtl");
	rgfx.updateMeshMaterial(wallMesh, &(rgfx_meshInstanceDesc) {
		.changeMaterialFlags = kMFAmbientMap,
		.changeMaterialNameHash = wallMaterial,
		.changeMaterialTexture = s_gameData.wall4AlbedoTextureArray,
		.newMaterialFlags = kMFDiffuseMap | kMFAmbientMap | kMFInstancedAO,
	});

	rgfx_texture floorAOTexture = rres.findTexture(hashString("Floor_ao.png"));
	uint32_t floorMesh = hashString("AHRoom:Floor");
	uint32_t floorMaterial = hashString("floorMtl");
	rgfx.updateMeshMaterial(floorMesh, &(rgfx_meshInstanceDesc) {
		.changeMaterialFlags = kMFAmbientMap,
		.changeMaterialNameHash = floorMaterial,
		.changeMaterialTexture = floorAOTexture,
		.newMaterialFlags = kMFDiffuseMap | kMFNormalMap | kMFAmbientMap,
	});

	const char* guiTextures[] = {
		"BluePaddleSprite.png",
		"UpAngleBracket.png",
		"DownAngleBracket.png",
		"LeftAngleBracket.png",
		"RightAngleBracket.png",
		"TableButtonBlue.png",
		"TableButton.png",
		"TableButtonRed.png",
	};
	rgui.registerTextureSet(guiTextures, STATIC_ARRAY_SIZE(guiTextures));
	s_gameData.sliderTexture = rgui.findTexture("BluePaddleSprite.png");
	s_gameData.scrollUpButtonTexture = rgui.findTexture("UpAngleBracket.png");
	s_gameData.scrollDownButtonTexture = rgui.findTexture("DownAngleBracket.png");
	s_gameData.prevButtonTexture = rgui.findTexture("LeftAngleBracket.png");
	s_gameData.nextButtonTexture = rgui.findTexture("RightAngleBracket.png");
	s_gameData.tableButtonBlueTexture = rgui.findTexture("TableButtonBlue.png");
	s_gameData.tableButtonTexture = rgui.findTexture("TableButton.png");
	s_gameData.tableButtonRedTexture = rgui.findTexture("TableButtonRed.png");

	// Initialize 4xMSAA'd RenderTarget & Depth Buffer;
	rgfx_renderTargetDesc rtDesc = {
		.width = s_gameData.screenWidth,
		.height = s_gameData.screenHeight,
		.format = kFormatSRGBA8,
		.minFilter = kFilterLinear,
		.magFilter = kFilterLinear,
		.sampleCount = 4,
		.stereo = true,
	};
	rgfx_renderTarget aaRenderTarget = rgfx.createRenderTarget(&rtDesc);
	rtDesc.format = kFormatD32F;
	rgfx_renderTarget aaDepthRenderTarget = rgfx.createRenderTarget(&rtDesc); 

	s_gameData.msaaPass = rgfx.createPass(&(rgfx_passDesc) {
		.colorAttachments[0].rt = aaRenderTarget,
		.depthAttachment.rt = aaDepthRenderTarget,
	});

	s_gameData.blinnphongPipeline = rgfx.createPipeline(&(rgfx_pipelineDesc) {
		.vertexShader = rgfx.loadShader("shaders/blinnphong.vert", kVertexShader, 0),
		.fragmentShader = rgfx.loadShader("shaders/blinnphong.frag", kFragmentShader, kMFDiffuseMap),
	});

	s_gameData.blinnphongNormalMappedPipeline = rgfx.createPipeline(&(rgfx_pipelineDesc) {
		.vertexShader = rgfx.loadShader("shaders/blinnphong.vert", kVertexShader, kMFNormalMap),
		.fragmentShader = rgfx.loadShader("shaders/blinnphong.frag", kFragmentShader, kMFDiffuseMap | kMFNormalMap),
	});

	s_gameData.blinnphongSkinnedPipeline = rgfx.createPipeline(&(rgfx_pipelineDesc) {
		.vertexShader = rgfx.loadShader("shaders/blinnphong.vert", kVertexShader, kMFSkinned),
		.fragmentShader = rgfx.loadShader("shaders/blinnphong.frag", kFragmentShader, kMFDiffuseMap),
	});

	s_gameData.blinnphongEmissivePipeline = rgfx.createPipeline(&(rgfx_pipelineDesc) {
		.vertexShader = rgfx.loadShader("shaders/blinnphong.vert", kVertexShader, 0),
		.fragmentShader = rgfx.loadShader("shaders/blinnphong.frag", kFragmentShader, kMFEmissiveMap),
	});

	rgfx_pipeline metallic = rgfx.createPipeline(&(rgfx_pipelineDesc) {
		.vertexShader = rgfx.loadShader("shaders/blinnphong.vert", kVertexShader, 0),
		.fragmentShader = rgfx.loadShader("shaders/blinnphong.frag", kFragmentShader, kMFDiffuseMap | kMFMetallicMap),
	});

	rgfx_pipeline instancedAlbedo = rgfx.createPipeline(&(rgfx_pipelineDesc) {
		.vertexShader = rgfx.loadShader("shaders/blinnphong.vert", kVertexShader, 0),
		.fragmentShader = rgfx.loadShader("shaders/blinnphong.frag", kFragmentShader, kMFDiffuseMap | kMFInstancedAlbedo),
	});

	rgfx_pipeline normalAO = rgfx.createPipeline(&(rgfx_pipelineDesc) {
		.vertexShader = rgfx.loadShader("shaders/blinnphong.vert", kVertexShader, kMFNormalMap),
		.fragmentShader = rgfx.loadShader("shaders/blinnphong.frag", kFragmentShader, kMFDiffuseMap | kMFNormalMap | kMFAmbientMap),
	});

	rgfx_pipeline instancedAO = rgfx.createPipeline(&(rgfx_pipelineDesc) {
		.vertexShader = rgfx.loadShader("shaders/blinnphong.vert", kVertexShader, 0),
		.fragmentShader = rgfx.loadShader("shaders/blinnphong.frag", kFragmentShader, kMFDiffuseMap | kMFAmbientMap | kMFInstancedAO),
	});

#ifdef RE_PLATFORM_WIN64
	rgfx.initPostEffects();
#endif
	rgfx.debugRenderInitialize(NULL);

	float* supportedRefreshRates = NULL;
	int32_t numSupportedRefreshRates = rvrs.getSupportedRefreshRates(&supportedRefreshRates);
	for (int32_t i = 0; i < numSupportedRefreshRates; ++i)
	{
		if (supportedRefreshRates[i] >= 72.0f && supportedRefreshRates[i] != 80.0f)	// CLR - There seems to be an issue with Avatar hands at 80fps right now.
		{
			s_gameData.supportedRefreshRates[s_gameData.numSupportedRefreshRates++] = supportedRefreshRates[i];
		}
	}

	rvrs.initializePlatformOvr(&(rvrs_platformDescOvr) {
		.appId = OCULUS_APP_ID,
		.entitledCallback = ^(uint64_t userId) {
			rvrs.initializeAvatarOvr(OCULUS_APP_ID);
			if (userId == 0) {
				userId = cycleAvatars(1);
			}
			else
			{
				s_gameData.bEntitled = true;
				s_gameData.playerCount = 1;
				kVolumeScale = s_gameData.masterVolume;
			}
			s_gameData.userId = userId;
			rvrs.requestAvatarSpecOvr(userId, kPlayerAvatarSlot);
		}
	});
}

void game_terminate()
{
	if (sb_count(s_gameData.roomData.userArray) > 1)
	{
		rvrs.leaveRoomOvr(s_gameData.roomData.roomID);
		destroyRoom();
	}
	
	rgfx.removeAllMeshInstances();
}

bool game_shouldQuit(bool systemQuit)
{
	return systemQuit || rvrs.shouldQuit();
}

typedef enum Keys
{
	kKeyForward = 1,
	kKeyBackwards = 2,
	kKeyLeft = 4,
	kKeyRight = 8,
	kKeyYawLeft = 16,
	kKeyYawRight = 32,
	kKeyMenu = 64,
}Keys;

void game_update(float dt, rsys_input systemInput)
{
	rsys_perfTimer timer;
	rsys.perfTimerStart(&timer);
	s_gameData.prevKeyPresses = s_gameData.keyPresses;
	s_gameData.keyPresses = systemInput.keyPresses;
	s_gameData.mousePresses = systemInput.mousePresses;
	s_gameData.mousePosNdcX = systemInput.mousePosNdcX;
	s_gameData.mousePosNdcY = systemInput.mousePosNdcY;
	const vec4 kPaddlePositionOffset[2] = { { 0.035f, 0.0f, -0.015, 0.0f }, { -0.035f, 0.0f, -0.015, 0.0f } };

#ifdef RE_PLATFORM_WIN64
	if (!rvrs.isHmdMounted())
	{
		s_gameData.hmdWasMounted = false;
		vec4 offset = s_gameData.playerIndex == 0 ? (vec4) { 0.0f, 1.0f, 0.75f, 0.0f } : (vec4) { 0.0f, 1.0f, -0.75f, 0.0f };
		vec4 eye = kPlayerCamPos[s_gameData.playerIndex] + offset;
		vec4 at = { 0.0f, 1.0f, 0.0f, 1.0f };
		vec4 up = { 0.0f, 1.0f, 0.0f, 0.0f };
		s_gameData.camWorldMatrix = mat4x4_lookAtWorld(eye, at, up);
		s_gameData.camViewMatrix = mat4x4_orthoInverse(s_gameData.camWorldMatrix);

		const float fov = RADIANS(90.0f);
		const float aspectRatio = (float)s_gameData.screenHeight / (float)s_gameData.screenWidth;
		const float nearZ = 0.1f;
		const float farZ = 100.0f;
		const float focalLength = 1.0f / tanf(fov * 0.5f);

		float left = -nearZ / focalLength;
		float right = nearZ / focalLength;
		float bottom = -aspectRatio * nearZ / focalLength;
		float top = aspectRatio * nearZ / focalLength;

		mat4x4 projectionMatrix = mat4x4_frustum(left, right, bottom, top, nearZ, farZ);
		rgfx.updateTransforms(&(rgfx_cameraDesc) {
			.camera[0].position = eye,
			.camera[0].viewMatrix = s_gameData.camViewMatrix,
			.camera[0].projectionMatrix = projectionMatrix,
		});
		s_gameData.projectionMatrix = projectionMatrix;
		
		static bool bKeyWasPressed = false;
		if (systemInput.keyPresses & kKeyMenu)
		{
			if (!bKeyWasPressed)
			{
				if (s_gameData.guiMode == kGUINone)
				{
					if (sb_count(s_gameData.roomData.userArray) > 1 && s_gameData.lastGuiMode == kGUIOnline)
					{ 
						s_gameData.guiMode = kGUIOnline;
					}
					else
					{
						s_gameData.guiMode = kGUIOptions;
					}
				}
				else
				{
					s_gameData.lastGuiMode = s_gameData.guiMode;
					s_gameData.guiMode = kGUINone;
				}
				s_gameData.guiTransformMain.wAxis = (vec4){ 0.0f, 0.0f, 0.0f, 0.0f };
				bKeyWasPressed = true;
			}
		}
		else
		{
			bKeyWasPressed = false;
		}
	}
	else
#endif // RE_PLATFORM_WIN64
	{
		s_gameData.hmdWasMounted = true;
		vec4 camEye = kPlayerCamPos[s_gameData.playerIndex];
		vec4 camAt = { 0.0f, 0.0f, 0.0f, 1.0f };
		vec4 camUp = { 0.0f, 1.0f, 0.0f, 0.0f };
		s_gameData.camWorldMatrix = mat4x4_lookAtWorld(camEye, camAt, camUp);
		s_gameData.camViewMatrix = mat4x4_orthoInverse(s_gameData.camWorldMatrix);
		rgfx.setGameCamera(&(rgfx_gameCameraDesc) {
			.worldMatrix = s_gameData.camWorldMatrix,
			.viewMatrix = s_gameData.camViewMatrix,
		});
		uint32_t handIndex = 0;

		vec4 leftHandPosition;
		vec4 rightHandPosition;
		float leftHandGripValue = 0.0f;
		float rightHandGripValue = 0.0f;

		if (1) //m_leftHandControllerIndex >= 0)
		{
			rvrs.getTrackedControllerPose(0, &s_gameData.handPoses[0]);
			vec4 controllerOrientation = (vec4){ s_gameData.handPoses[0].pose.orientation.x, s_gameData.handPoses[0].pose.orientation.y, s_gameData.handPoses[0].pose.orientation.z, s_gameData.handPoses[0].pose.orientation.w };
			vec4 controllerPosition = (vec4){ s_gameData.handPoses[0].pose.position.x, s_gameData.handPoses[0].pose.position.y, s_gameData.handPoses[0].pose.position.z, 1.0f };
			mat4x4 origControllerMatrix = mat4x4_create(controllerOrientation, controllerPosition);

			mat4x4 controllerMatrix = mat4x4_mul(s_gameData.camWorldMatrix, origControllerMatrix);
			leftHandPosition = vec4_vecScale(controllerMatrix.wAxis, (vec4) { 1.0f, 0.0f, 1.0f, 1.0f });
			leftHandPosition = leftHandPosition - kPaddlePositionOffset[s_gameData.playerIndex];
			rvrs_controllerState controllerState;
			if (rvrs.getControllerState(0, &controllerState) >= 0)
			{
				s_gameData.playerButtons = controllerState.buttons;
				s_gameData.playerTouches = controllerState.touches;
				s_gameData.leftHandTriggerValue = controllerState.trigger;
				if (controllerState.grip > 0.1f)
				{
					leftHandGripValue = controllerState.grip;
				}
				if (controllerState.trigger > 0.1f)
				{
					s_gameData.lastUsedHand = kLeftHand;
				}
				if (controllerState.buttonsPressed & ovrButton_Enter)
				{
					if (s_gameData.guiMode == kGUINone)
					{
						if (sb_count(s_gameData.roomData.userArray) > 1 && s_gameData.lastGuiMode == kGUIOnline)
						{
							s_gameData.guiMode = kGUIOnline;
						}
						else
						{
							s_gameData.guiMode = kGUIOptions;
						}
					}
					else
					{
						s_gameData.lastGuiMode = s_gameData.guiMode;
						s_gameData.guiMode = kGUINone;
					}
					s_gameData.guiTransformMain.wAxis = (vec4){ 0.0f, 0.0f, 0.0f, 0.0f };
				}
			}
		}

		if (1)
		{
			rvrs.getTrackedControllerPose(1, &s_gameData.handPoses[1]);
			quat controllerOrientation = (quat){ s_gameData.handPoses[1].pose.orientation.x, s_gameData.handPoses[1].pose.orientation.y, s_gameData.handPoses[1].pose.orientation.z, s_gameData.handPoses[1].pose.orientation.w };
			vec4 controllerPosition = (vec4){ s_gameData.handPoses[1].pose.position.x, s_gameData.handPoses[1].pose.position.y, s_gameData.handPoses[1].pose.position.z, 1.0f };
			mat4x4 origControllerMatrix = mat4x4_create(controllerOrientation, controllerPosition);
			mat4x4 controllerMatrix = mat4x4_mul(s_gameData.camWorldMatrix, origControllerMatrix);
			rightHandPosition = vec4_vecScale(controllerMatrix.wAxis, (vec4) { 1.0f, 0.0f, 1.0f, 1.0f });
			rightHandPosition = rightHandPosition - kPaddlePositionOffset[1-s_gameData.playerIndex];
			rvrs_controllerState controllerState;
			if (rvrs.getControllerState(1, &controllerState) >= 0)
			{
				s_gameData.playerButtons = controllerState.buttons;
				s_gameData.playerTouches = controllerState.touches;
				s_gameData.rightHandTriggerValue = controllerState.trigger;
				if (controllerState.grip > 0.1f)
				{
					rightHandGripValue = controllerState.grip;
				}
				if (controllerState.trigger > 0.1f)
				{
					s_gameData.lastUsedHand = kRightHand;
				}
			}
		}
	}

	if (sb_count(s_gameData.roomData.userArray) > 1)
	{
		int32_t numNetPackets = 0;
		numNetPackets = updateFromNetwork(dt);
//		updateNetwork(dt);
	}

	if (rvrs.isHmdMounted()) {
		rvrs.getHeadPose(&s_gameData.headPose);
		s_gameData.headOrientation = (vec4){ s_gameData.headPose.pose.orientation.x, s_gameData.headPose.pose.orientation.y, s_gameData.headPose.pose.orientation.z, s_gameData.headPose.pose.orientation.w };
		s_gameData.headPosition = (vec4){ s_gameData.headPose.pose.position.x, s_gameData.headPose.pose.position.y, s_gameData.headPose.pose.position.z, 1.0f };
		mat4x4 localHeadMatrix = mat4x4_create(s_gameData.headOrientation, s_gameData.headPosition);
		mat4x4 worldHeadMatrix = mat4x4_mul(s_gameData.camWorldMatrix, localHeadMatrix);
		raud.updateListenerPosition((float*)&worldHeadMatrix.wAxis, NULL, (float*)&worldHeadMatrix.zAxis, (float*)&worldHeadMatrix.yAxis);
	}
	else {
		raud.updateListenerPosition((float*)&s_gameData.camWorldMatrix.wAxis, NULL, (float*)&s_gameData.camWorldMatrix.zAxis, (float*)&s_gameData.camWorldMatrix.yAxis);
	}

	if (sb_count(s_gameData.roomData.userArray) > 1)
	{
		updateNetwork(dt);
	}

	if (rvrs.isInitialized()) {
		updateAvatar(dt);
	}
#ifndef DISABLE_VOIP
	if (sb_count(s_gameData.roomData.userArray) > 1)
	{
		voipUpdate();
	}
#endif

#ifdef DEMO_HUD
	float totalBrightness = 0.0f;
	for (int i = 1; i < 4; ++i)
	{
		totalBrightness += s_gameData.lightBrightness[i];
	}
	s_gameData.brightness = totalBrightness;
#endif
	if (fabsf(s_gameData.brightness - s_gameData.prevBrightness) > 0.0f)
	{
		for (int light = 0; light < 4; ++light)
		{
#ifndef DEMO_HUD
			rgfx.updateLight(light, &(rgfx_lightUpdateDesc) {
				.colorScalar = s_gameData.brightness
			});
#else
			rgfx.updateLight(light, &(rgfx_lightUpdateDesc) {
				.colorScalar = s_gameData.lightBrightness[light]
			});
#endif
		}
		s_gameData.prevBrightness = s_gameData.brightness;
	}

	if (s_gameData.guiMode != kGUINone)
	{
		showGUI(dt);
	}

	if (sb_count(s_gameData.roomData.userArray) > 1)
	{
		updateRoom(dt);
	}

	if (s_gameData.requestedFramerateSlider != s_gameData.currentFramerateSlider)
	{
		int32_t sliderValue = (int32_t)s_gameData.requestedFramerateSlider;
		int32_t refreshRate = s_gameData.supportedRefreshRates[sliderValue];
		rvrs.setRefreshRate(refreshRate);
		s_gameData.currentFramerateSlider = s_gameData.requestedFramerateSlider;
//		if (s_gameData.settingsData.advancedSettings == 0) {
			s_gameData.requestedFoveationSlider = kSuggestedFFRForRefreshRateQuest2[sliderValue];
			s_gameData.requestedEyeScaleSlider = kSuggestedEyeScaleForRefreshRateQuest2[sliderValue];
//		}
	}

	if (s_gameData.requestedEyeScaleSlider != s_gameData.currentEyeScaleSlider)
	{
		int32_t renderScale = 100 + (int32_t)s_gameData.requestedEyeScaleSlider * 10;
		rgfx.scaleEyeFrameBuffers(renderScale);
		s_gameData.currentEyeScaleSlider = s_gameData.requestedEyeScaleSlider;
	}

	if (s_gameData.requestedFoveationSlider != s_gameData.currentFoveationSlider)
	{
		int32_t foveationLevel = (int32_t)s_gameData.requestedFoveationSlider;
		rvrs.setFoveationLevel(foveationLevel);
		s_gameData.currentFoveationSlider = s_gameData.requestedFoveationSlider;
	}

/*
	if (s_gameData.keyPresses & kKeyLeft && s_gameData.prevKeyPresses == 0)
	{
		uint64_t userId = cycleAvatars(-1);
		rvrs.requestAvatarSpecOvr(userId);
	}

	if (s_gameData.keyPresses & kKeyRight && s_gameData.prevKeyPresses == 0)
	{
		uint64_t userId = cycleAvatars(1);
		rvrs.requestAvatarSpecOvr(userId);
	}
*/
	raud.update();			//CLR - Called at the end of the frame to tick FMOD and free up DSP usage.

	uint64_t uSecs = rsys.perfTimerStop(&timer);

	static uint64_t totalUSecs = 0;
	static uint64_t frames = 0;
	totalUSecs += uSecs;
	if (++frames > 24)
	{
		uint64_t avgUSecs = totalUSecs / frames;
		if (avgUSecs < 100000) {
			s_gameData.updateTime = avgUSecs;
		}
		totalUSecs = 0;
		frames = 0;
	}

#ifdef SHOW_PERF_HUD
	showPerfHUD(dt);
#endif // SHOW_PERF_HUD
}

void game_render()
{
	rsys_perfTimer timer;
	rsys.perfTimerStart(&timer);
#ifdef RE_PLATFORM_WIN64
	bool bHmdMounted = rvrs.isHmdMounted();
	if (!bHmdMounted || s_gameData.bRequestScreenshot)
	{
		rgfx.beginPass(s_gameData.msaaPass, &(rgfx_passAction) {
			.colors[0] = { .action = kActionClear, .val = { 0.2f, 0.2f, 0.2f, 1.0f } },
			.depth = { .action = kActionClear, .val = 1.0f }
		});
	}
#endif
	rgfx.renderWorld(s_gameData.bRequestScreenshot);
	rgui.endFrame();
#ifdef RE_PLATFORM_WIN64
	if (!bHmdMounted || s_gameData.bRequestScreenshot)
	{
#ifndef DISABLE_LOGO
		rgfx.renderFullscreenQuad(s_gameData.logoTexture);
#endif
		rgfx.endPass(s_gameData.bRequestScreenshot && s_gameData.screenshotCountdown == 0);
	}
	if (s_gameData.bRequestScreenshot == true && s_gameData.screenshotCountdown-- <= 0)
	{
		s_gameData.bRequestScreenshot = false;
	}
#endif

	uint64_t uSecs = rsys.perfTimerStop(&timer);

	rgfx_renderStats renderStats = rgfx.getRenderStats();

	static uint64_t totalUSecs = 0;
	static uint64_t frames = 0;
	static rgfx_renderStats totalRenderStats = { 0 };
	totalUSecs += uSecs;
	totalRenderStats.cullUSecs += renderStats.cullUSecs;
	totalRenderStats.sortUSecs += renderStats.sortUSecs;
	totalRenderStats.sceneUSecs += renderStats.sceneUSecs;
	totalRenderStats.renderUSecs += renderStats.renderUSecs;
	totalRenderStats.gpuUSecs += renderStats.gpuUSecs;
	if (++frames > 24)
	{
		uint64_t avgUSecs = totalUSecs / frames;
		if (avgUSecs < 100000) {
			s_gameData.renderTime = avgUSecs;
			s_gameData.renderStats.cullUSecs = totalRenderStats.cullUSecs / frames;
			s_gameData.renderStats.sortUSecs = totalRenderStats.sortUSecs / frames;
			s_gameData.renderStats.sceneUSecs = totalRenderStats.sceneUSecs / frames;
			s_gameData.renderStats.renderUSecs = totalRenderStats.renderUSecs / frames;
			s_gameData.renderStats.gpuUSecs = totalRenderStats.gpuUSecs / frames;
		}
		totalUSecs = 0;
		frames = 0;
		memset(&totalRenderStats, 0, sizeof(rgfx_renderStats));
	}
}

float getRefreshRateSliderValue(float refreshRate)
{
	int32_t numSupportedRefreshRates = s_gameData.numSupportedRefreshRates;
	for (int32_t i = 0; i < numSupportedRefreshRates; ++i)
	{
		if (s_gameData.supportedRefreshRates[i] == refreshRate)
		{
			return (float)i;
		}
	}
	return 1.0f;
}

void showPerfHUD(float dt)
{
	mat4x4 localHeadMatrix = rvrs.isHmdMounted() ? mat4x4_create(s_gameData.headOrientation, s_gameData.headPosition) : mat4x4_identity();
	mat4x4 worldHeadMatrix = mat4x4_mul(s_gameData.camWorldMatrix, localHeadMatrix);

	float cursorPosX = -0.3f; // (0.028f * 1.0f);
	float cursorPosY = 0.35f; // (0.028f * 1.0f);

	static char updateString[64];
	sprintf(updateString, "Update: %ldus", s_gameData.updateTime);
	rgui.text3D(&(rgui_textDesc) {
		.rotation = { worldHeadMatrix.xAxis, worldHeadMatrix.yAxis, worldHeadMatrix.zAxis },
		.position = worldHeadMatrix.wAxis + worldHeadMatrix.xAxis * cursorPosX + worldHeadMatrix.yAxis * cursorPosY - worldHeadMatrix.zAxis,
		.color = kDebugColorGreen,
		.scale = 24.0f,
		.text = updateString,  //"Update: 13ms",
		.font = s_gameData.font,
		.layoutFlags = kAlignHLeft | kAlignVCenter, //.layoutFlags = kAlignHRight,
	});

	cursorPosY -= (0.028f * 1.0f);

	static char renderString[64];
	sprintf(renderString, "Render: %ldus", s_gameData.renderTime);
	rgui.text3D(&(rgui_textDesc) {
		.rotation = { worldHeadMatrix.xAxis, worldHeadMatrix.yAxis, worldHeadMatrix.zAxis },
		.position = worldHeadMatrix.wAxis + worldHeadMatrix.xAxis * cursorPosX + worldHeadMatrix.yAxis * cursorPosY - worldHeadMatrix.zAxis,
		.color = kDebugColorRed,
		.scale = 24.0f,
		.text = renderString,
		.font = s_gameData.font,
		.layoutFlags = kAlignHLeft | kAlignVCenter, //.layoutFlags = kAlignHRight,
	});

	cursorPosX += (0.028f * 1.0f);
	cursorPosY -= (0.028f * 1.0f);

	static char cullString[64];
	sprintf(cullString, "Cull: %ldus", s_gameData.renderStats.cullUSecs);
	rgui.text3D(&(rgui_textDesc) {
		.rotation = { worldHeadMatrix.xAxis, worldHeadMatrix.yAxis, worldHeadMatrix.zAxis },
		.position = worldHeadMatrix.wAxis + worldHeadMatrix.xAxis * cursorPosX + worldHeadMatrix.yAxis * cursorPosY - worldHeadMatrix.zAxis,
		.color = kDebugColorRed,
		.scale = 24.0f,
		.text = cullString,
		.font = s_gameData.font,
		.layoutFlags = kAlignHLeft | kAlignVCenter, //.layoutFlags = kAlignHRight,
	});

	cursorPosY -= (0.028f * 1.0f);

	static char sortString[64];
	sprintf(sortString, "Sort: %ldus", s_gameData.renderStats.sortUSecs);
	rgui.text3D(&(rgui_textDesc) {
		.rotation = { worldHeadMatrix.xAxis, worldHeadMatrix.yAxis, worldHeadMatrix.zAxis },
		.position = worldHeadMatrix.wAxis + worldHeadMatrix.xAxis * cursorPosX + worldHeadMatrix.yAxis * cursorPosY - worldHeadMatrix.zAxis,
		.color = kDebugColorRed,
		.scale = 24.0f,
		.text = sortString,
		.font = s_gameData.font,
		.layoutFlags = kAlignHLeft | kAlignVCenter, //.layoutFlags = kAlignHRight,
	});

	cursorPosY -= (0.028f * 1.0f);

	static char sceneString[64];
	sprintf(sceneString, "Scene: %ldus", s_gameData.renderStats.sceneUSecs);
	rgui.text3D(&(rgui_textDesc) {
		.rotation = { worldHeadMatrix.xAxis, worldHeadMatrix.yAxis, worldHeadMatrix.zAxis },
		.position = worldHeadMatrix.wAxis + worldHeadMatrix.xAxis * cursorPosX + worldHeadMatrix.yAxis * cursorPosY - worldHeadMatrix.zAxis,
		.color = kDebugColorRed,
		.scale = 24.0f,
		.text = sceneString,
		.font = s_gameData.font,
		.layoutFlags = kAlignHLeft | kAlignVCenter, //.layoutFlags = kAlignHRight,
	});
	cursorPosY -= (0.028f * 1.0f);

	static char renderString2[64];
	sprintf(renderString2, "RendScn: %ldus", s_gameData.renderStats.renderUSecs);
	rgui.text3D(&(rgui_textDesc) {
		.rotation = { worldHeadMatrix.xAxis, worldHeadMatrix.yAxis, worldHeadMatrix.zAxis },
		.position = worldHeadMatrix.wAxis + worldHeadMatrix.xAxis * cursorPosX + worldHeadMatrix.yAxis * cursorPosY - worldHeadMatrix.zAxis,
		.color = kDebugColorRed,
		.scale = 24.0f,
		.text = renderString2,
		.font = s_gameData.font,
		.layoutFlags = kAlignHLeft | kAlignVCenter, //.layoutFlags = kAlignHRight,
	});

	cursorPosX -= (0.028f * 1.0f);
	cursorPosY -= (0.028f * 1.0f);

	static char gpuString[64];
	sprintf(gpuString, "GPU: %ldus", s_gameData.renderStats.gpuUSecs);
	rgui.text3D(&(rgui_textDesc) {
		.rotation = { worldHeadMatrix.xAxis, worldHeadMatrix.yAxis, worldHeadMatrix.zAxis },
		.position = worldHeadMatrix.wAxis + worldHeadMatrix.xAxis * cursorPosX + worldHeadMatrix.yAxis * cursorPosY - worldHeadMatrix.zAxis,
		.color = kDebugColorBlue,
		.scale = 24.0f,
		.text = gpuString,
		.font = s_gameData.font,
		.layoutFlags = kAlignHLeft | kAlignVCenter, //.layoutFlags = kAlignHRight,
	});

	cursorPosY -= (0.028f * 1.0f);

	static char fpsString[64];
	sprintf(fpsString, "FPS: %d", (int32_t)(1.0f / dt));
	rgui.text3D(&(rgui_textDesc) {
		.rotation = { worldHeadMatrix.xAxis, worldHeadMatrix.yAxis, worldHeadMatrix.zAxis },
		.position = worldHeadMatrix.wAxis + worldHeadMatrix.xAxis * cursorPosX + worldHeadMatrix.yAxis * cursorPosY - worldHeadMatrix.zAxis,
		.color = kDebugColorWhite,
		.scale = 24.0f,
		.text = fpsString,
		.font = s_gameData.font,
		.layoutFlags = kAlignHLeft | kAlignVCenter, //.layoutFlags = kAlignHRight,
	});
}

void moveToDesiredPos(vec4* currentPos, vec4* currentVel, vec4 desiredPos)
{
	const float dt = 1.0f / 72.0f;
	const float k = 200.0f;
	const float b = 20.0f;		//Damping coefficient

	vec4 pos = *currentPos;
	vec4 vel = *currentVel;

	vec4 x = desiredPos - pos;
	//	F = -kx - bv	
	vec4 springForce = x * k;
	vec4 dampingForce = vel * b;
	vec4 totalForce = springForce - dampingForce;

	vec4 accel = totalForce * 0.5f; // 1.0f / 2.0f

	vel = vel + accel * dt;
	pos = pos + vel * dt;

	*currentVel = vel;
	*currentPos = pos;
}

typedef enum rvrs_avatarButton {
	kAvatarButton_One = 0x0001, ///< X/A pressed
	kAvatarButton_Two = 0x0002, ///< Y/B pressed
	kAvatarButton_Three = 0x0004, ///< Select/Oculus button pressed
	kAvatarButton_Joystick = 0x0008, ///< Joystick button pressed
} rvrs_avatarButton;

typedef enum rvrs_avatarTouch {
	kAvatarTouch_One = 0x0001, ///< Capacitive touch for X/A button
	kAvatarTouch_Two = 0x0002, ///< Capacitive touch for Y/B button
	kAvatarTouch_Joystick = 0x0004, ///< Capacitive touch for thumbstick
	kAvatarTouch_ThumbRest = 0x0008, ///< Capacitive touch for thumb rest
	kAvatarTouch_Index = 0x0010, ///< Capacitive touch for index trigger
	kAvatarTouch_Pointing = 0x0040, ///< Index finger is pointing
	kAvatarTouch_ThumbUp = 0x0080, ///< Thumb is up
} rvrs_avatarTouch;

uint64_t cycleAvatars(int32_t dir)
{
	static const uint64_t s_kCrossPlatformAvatars[13] = {
		 2006633449384423,					// Claire.
		10150030458727564,
		10150030458738922,
		10150030458747067,
		10150030458756715,
		10150030458762178,
		10150030458769900,
		10150030458775732,
		10150030458785587,
		10150030458806683,
		10150030458820129,
		10150030458827644,
		10150030458843421,
	};
//	assert(abs(dir) >= 0);
//	assert(abs(dir) < 2);

	s_gameData.avatarIndex += dir;
	s_gameData.avatarIndex = MIN(s_gameData.avatarIndex, 13 - 1);
	s_gameData.avatarIndex = MAX(s_gameData.avatarIndex, 0);
	uint64_t userId = s_kCrossPlatformAvatars[s_gameData.avatarIndex];

	return userId;
}

void updateRoom(float dt)
{
	if (s_gameData.roomData.state == kRSDestroyRoom || (s_gameData.roomData.state == kRSSolo && s_gameData.roomData.friendsList != NULL))
	{
		//CLR - Clear up friends list here if it's been allocated above and is no longer needed. Question: Is this needed, is it best to just keep the list of friends active once created?
		destroyRoom();
	}
}

int32_t updateFromNetwork(float dt)
{
	#define MAX_PACKETS (16)
	rvrp_netPacketDesc packets[MAX_PACKETS] = {};
	uint8_t* packetBuffer = (uint8_t*)alloca(1200 * MAX_PACKETS);
	int32_t packetCount = 0;
	int32_t packetsRead = rvrs.netReadPackets(&packets, packetBuffer, MAX_PACKETS);
	if (packetsRead)
	{
		for (int32_t i = 0; i < packetsRead; ++i)
		{
			rvrp_netPacketHeader* header = packets[i].packetData;
			uint32_t hashId = hashData(&packets[i].userId, sizeof(packets[i].userId));
			int32_t userIndex = rsys_hm_find(&s_gameData.roomData.userLookup, hashId);
			if (userIndex < 0)
				continue;

			switch (header->packetType)
			{
				case kNPNetStart:
				{
					NetStartPacket* netStartPacket = packets[i].packetData;
					int64_t clock = netStartPacket->serverClock;
					int64_t ping = netStartPacket->pingTime;
					s_gameData.roomData.netClock = clock + (ping >> 1);
					if (packets[i].userId == s_gameData.roomData.roomOwner && s_gameData.userId != s_gameData.roomData.roomOwner)
					{
						s_gameData.roomData.state = kRSCreatedNetRunning;
					}
					break;
				}

				case kNPClockSyncRequest:
				{
					ClockSyncRequestPacket* clockSyncRequestPacket = packets[i].packetData;
					ClockSyncResponsePacket clockSyncResponsePacket = {
						.header.packetType = kNPClockSyncResponse,
						.clientClock = clockSyncRequestPacket->clientClock,
						.serverClock = s_gameData.roomData.netClock,
					};
					rvrs.netSendPacket(&(rvrp_netPacketDesc) {
						.userId = s_gameData.roomData.userArray[1].userId,
						.packetSize = sizeof(ClockSyncResponsePacket),
						.packetData = &clockSyncResponsePacket
					});
					break;
				}
				case kNPClockSyncResponse:
				{
					ClockSyncResponsePacket* clockSyncResponsePacket = packets[i].packetData;
					int64_t clientClock = clockSyncResponsePacket->clientClock;
					int64_t serverClock = clockSyncResponsePacket->serverClock;
					int64_t roundTrip = s_gameData.roomData.netClock - clientClock;
					int64_t latency = roundTrip >> 1;
					int64_t serverDelta = serverClock - s_gameData.roomData.netClock;
					int64_t timeDelta = serverDelta + latency;
					if (timeDelta != 0)
					{
						s_gameData.roomData.netClock = s_gameData.roomData.netClock + timeDelta;
					}
					break;
				}
				case kAANPPlayerAvatar:
				{
					GamePlayerAvatarPacket* gameAvatarPacket = packets[i].packetData;

					int64_t timeStamp = gameAvatarPacket->timeStamp;
					if (timeStamp >= s_gameData.roomData.userArray[userIndex].lastPacketTimestamp)
					{
						uint32_t flagsButtTouches = gameAvatarPacket->flagsButtTouch; // (flags << 24) | (buttonsPacked << 16) | touches;
						float headRotX = (float)gameAvatarPacket->headRot[0] / 32767.0f;
						float headRotY = (float)gameAvatarPacket->headRot[1] / 32767.0f;
						float headRotZ = (float)gameAvatarPacket->headRot[2] / 32767.0f;
						float headRotW = (float)gameAvatarPacket->headRot[3] / 32767.0f;
						float lhandRotX = (float)gameAvatarPacket->leftHandRot[0] / 32767.0f;
						float lhandRotY = (float)gameAvatarPacket->leftHandRot[1] / 32767.0f;
						float lhandRotZ = (float)gameAvatarPacket->leftHandRot[2] / 32767.0f;
						float lhandRotW = (float)gameAvatarPacket->leftHandRot[3] / 32767.0f;
						float rhandRotX = (float)gameAvatarPacket->rightHandRot[0] / 32767.0f;
						float rhandRotY = (float)gameAvatarPacket->rightHandRot[1] / 32767.0f;
						float rhandRotZ = (float)gameAvatarPacket->rightHandRot[2] / 32767.0f;
						float rhandRotW = (float)gameAvatarPacket->rightHandRot[3] / 32767.0f;
						float headPosX = (float)gameAvatarPacket->headPos[0] / 4095.0f;
						float headPosY = (float)gameAvatarPacket->headPos[1] / 4095.0f;
						float headPosZ = (float)gameAvatarPacket->headPos[2] / 4095.0f;
						float lhandPosX = (float)gameAvatarPacket->leftHandPos[0] / 4095.0f;
						float lhandPosY = (float)gameAvatarPacket->leftHandPos[1] / 4095.0f;
						float lhandPosZ = (float)gameAvatarPacket->leftHandPos[2] / 4095.0f;
						float rhandPosX = (float)gameAvatarPacket->rightHandPos[0] / 4095.0f;
						float rhandPosY = (float)gameAvatarPacket->rightHandPos[1] / 4095.0f;
						float rhandPosZ = (float)gameAvatarPacket->rightHandPos[2] / 4095.0f;
						float lhandGrip = (float)gameAvatarPacket->leftHandGrip / 255.0f;
						float lhandTrigger = (float)gameAvatarPacket->leftHandTrigger / 255.0f;
						float rhandGrip = (float)gameAvatarPacket->rightHandGrip / 255.0f;
						float rhandTrigger = (float)gameAvatarPacket->rightHandTrigger / 255.0f;
						s_gameData.opponentHeadRot = (quat){ headRotX, headRotY, headRotZ, headRotW };
						s_gameData.opponentHeadPos = (vec4){ headPosX, headPosY, headPosZ, 1.0f };
						s_gameData.opponentHandsRot[0] = (quat){ lhandRotX, lhandRotY, lhandRotZ, lhandRotW };
						s_gameData.opponentHandsRot[1] = (quat){ rhandRotX, rhandRotY, rhandRotZ, rhandRotW };
						s_gameData.opponentHandsPos[0] = (vec4){ lhandPosX, lhandPosY, lhandPosZ, 1.0f };
						s_gameData.opponentHandsPos[1] = (vec4){ rhandPosX, rhandPosY, rhandPosZ, 1.0f };
						s_gameData.opponentButtons = (flagsButtTouches >> 16) & 0xff;
						s_gameData.opponentTouches = flagsButtTouches & 0xffff;
						s_gameData.opponentAvatarFlags = flagsButtTouches >> 24;
						s_gameData.opponentGrip[0] = lhandGrip;
						s_gameData.opponentGrip[1] = rhandGrip;
						s_gameData.opponentTrigger[0] = lhandTrigger;
						s_gameData.opponentTrigger[1] = rhandTrigger;
					}
					break;
				}
			}
			packetCount++;
		}
	}
	return packetCount;
}

void updateNetwork(float dt)
{
	if (s_gameData.roomData.state == kRSCreatedNetRunning)
	{
		int64_t deltaMicros = (int64_t)(dt * 1000000.0f);
		int64_t tickDelta = s_gameData.roomData.netClock - (s_gameData.roomData.prevTick * 1000000);
		netSendPlayerAvatarPacket();
		if (tickDelta > 1000000)
		{
			s_gameData.roomData.prevTick = s_gameData.roomData.netClock / 1000000;

			uint64_t serverId = s_gameData.roomData.roomOwner;
			if (s_gameData.userId != serverId)
			{
				//Client
				ClockSyncRequestPacket clockSyncRequestPacket = {
					.header.packetType = kNPClockSyncRequest,
					.clientClock = s_gameData.roomData.netClock,
				};

				rvrs.netSendPacket(&(rvrp_netPacketDesc) {
					.userId = serverId,
					.packetSize = sizeof(ClockSyncRequestPacket),
					.packetData = &clockSyncRequestPacket
				});
			}
		}
		s_gameData.roomData.netClock += deltaMicros;
	}
}

void netConnect(uint64_t userId)
{
	rvrs.netConnect(&(rvrs_ovrNetConnectDesc) {
		.userId = userId,
		.netCallback = ^ (const rvrs_netCallbackData * callbackData) {
			if (callbackData != NULL)
			{
				uint64_t userId = callbackData->userId;
				uint32_t hashId = hashData(&userId, sizeof(userId));
				int32_t userIndex = rsys_hm_find(&s_gameData.roomData.userLookup, hashId);
				if (userIndex >= 0)
				{
					switch (callbackData->type)
					{
					case kNetCallbackConnectionState:
						switch (callbackData->netState)
						{
						case kNetStateConnected:
							rsys.print("NetConnect: Connected with user %" PRId64 "\n", userId);
							//																s_gameData.roomData.userArray[userIndex].state = userIndex == 0 ? kUserHost : kUserJoined;
							s_gameData.roomData.userArray[userIndex].netState = kNetStateConnected;
							s_gameData.lastGuiMode = s_gameData.guiMode;
							s_gameData.guiMode = kGUINone;
							break;
						case kNetStateTimeout:
							rsys.print("NetConnect: User %" PRId64 " timed out!\n", userId);
							break;
						case kNetStateClosed:
							rsys.print("NetConnect: User %" PRId64 " has left the building!\n", userId);
							s_gameData.roomData.userArray[userIndex].netState = kNetStateClosed;
							break;
						}
						break;
					case kNetCallbackPing:
						rsys.print("NetConnect: User %" PRId64 " pinged = %" PRId64 "\n", userId, callbackData->ping);
						s_gameData.roomData.userArray[userIndex].lastPing = callbackData->ping;
						if (s_gameData.roomData.roomOwner == s_gameData.userId && s_gameData.roomData.state != kRSCreatedNetRunning)
						{
							uint64_t userId = s_gameData.roomData.userArray[userIndex].userId;
							NetStartPacket netStartPacket = {
								.header.packetType = kNPNetStart,
								.serverClock = s_gameData.roomData.netClock,
								.pingTime = s_gameData.roomData.userArray[userIndex].lastPing,
							};
							rvrs.netSendPacket(&(rvrp_netPacketDesc) {
								.userId = userId,
								.packetSize = sizeof(NetStartPacket),
								.packetData = &netStartPacket,
								.sendPolicy = kNetSendReliable
							});
							s_gameData.roomData.state = kRSCreatedNetRunning;
						}
						break;
					default:
						assert(0);
					}
				}
			}
		}
	});
}

void netSendPlayerAvatarPacket(void)
{
	int32_t playerCount = sb_count(s_gameData.roomData.userArray);
	for (int32_t i = 0; i < playerCount; ++i)
	{
		if (s_gameData.roomData.userArray[i].userId != s_gameData.userId)
		{
			uint32_t flags = (rvrs.isHmdMounted() ? 0x4 : 0);
			uint32_t buttonsPacked = (s_gameData.playerButtons & 0xf00 >> 4) | (s_gameData.playerButtons & 0xf);
			uint32_t touches = s_gameData.playerTouches & 0xffff;
			uint32_t flagsButtTouches = (flags << 24) | (buttonsPacked << 16) | touches;
			GamePlayerAvatarPacket gameAvatarPacket = {
				.header.packetType = kAANPPlayerAvatar,
				.timeStamp = s_gameData.roomData.netClock,
				.flagsButtTouch = flagsButtTouches,
				.headRot[0] = (int16_t)(s_gameData.headPose.pose.orientation.x * 32767.0f),
				.headRot[1] = (int16_t)(s_gameData.headPose.pose.orientation.y * 32767.0f),
				.headRot[2] = (int16_t)(s_gameData.headPose.pose.orientation.z * 32767.0f),
				.headRot[3] = (int16_t)(s_gameData.headPose.pose.orientation.w * 32767.0f),
				.leftHandRot[0] = (int16_t)(s_gameData.handPoses[0].pose.orientation.x * 32767.0f),
				.leftHandRot[1] = (int16_t)(s_gameData.handPoses[0].pose.orientation.y * 32767.0f),
				.leftHandRot[2] = (int16_t)(s_gameData.handPoses[0].pose.orientation.z * 32767.0f),
				.leftHandRot[3] = (int16_t)(s_gameData.handPoses[0].pose.orientation.w * 32767.0f),
				.rightHandRot[0] = (int16_t)(s_gameData.handPoses[1].pose.orientation.x * 32767.0f),
				.rightHandRot[1] = (int16_t)(s_gameData.handPoses[1].pose.orientation.y * 32767.0f),
				.rightHandRot[2] = (int16_t)(s_gameData.handPoses[1].pose.orientation.z * 32767.0f),
				.rightHandRot[3] = (int16_t)(s_gameData.handPoses[1].pose.orientation.w * 32767.0f),
				.headPos[0] = (int16_t)(s_gameData.headPose.pose.position.x * 4095.0f),
				.headPos[1] = (int16_t)(s_gameData.headPose.pose.position.y * 4095.0f),
				.headPos[2] = (int16_t)(s_gameData.headPose.pose.position.z * 4095.0f),
				.leftHandPos[0] = (int16_t)(s_gameData.handPoses[0].pose.position.x * 4095.0f),
				.leftHandPos[1] = (int16_t)(s_gameData.handPoses[0].pose.position.y * 4095.0f),
				.leftHandPos[2] = (int16_t)(s_gameData.handPoses[0].pose.position.z * 4095.0f),
				.rightHandPos[0] = (int16_t)(s_gameData.handPoses[1].pose.position.x * 4095.0f),
				.rightHandPos[1] = (int16_t)(s_gameData.handPoses[1].pose.position.y * 4095.0f),
				.rightHandPos[2] = (int16_t)(s_gameData.handPoses[1].pose.position.z * 4095.0f),
				.leftHandGrip = (uint8_t)(s_gameData.leftHandGripValue * 255.0f),
				.leftHandTrigger = (uint8_t)(s_gameData.leftHandTriggerValue * 255.0f),
				.rightHandGrip = (uint8_t)(s_gameData.rightHandGripValue * 255.0f),
				.rightHandTrigger = (uint8_t)(s_gameData.rightHandTriggerValue * 255.0f),
			};
			rvrs.netSendPacket(&(rvrp_netPacketDesc) {
				.userId = s_gameData.roomData.userArray[i].userId,
				.packetSize = sizeof(GamePlayerAvatarPacket),
				.packetData = &gameAvatarPacket
			});
		}
	}
}

#define VOIP_PCM_LENGTH 4800
#define VOIP_BUFFER_SIZE 96000
static uint32_t s_canary1 = 0xdeadbeef;
static int16_t s_voipBuffer[VOIP_BUFFER_SIZE] = { 0 };
static uint32_t s_canary2 = 0xdeadbeef;
static int32_t s_voipBytesWritten = 0;
int16_t* s_voipReadPtr = s_voipBuffer;
int16_t* s_voipWritePtr = s_voipBuffer;
size_t s_voipBufferSize = sizeof(s_voipBuffer);

void voipConnect(uint64_t userId)
{
	rvrs.voipStart(&(rvrs_ovrVoipStartDesc) {
		.userId = userId,
		.voipCallback = ^ (const rvrs_voipCallbackData * callbackData) {
			if (callbackData != NULL)
			{
				uint64_t userId = callbackData->userId;
				uint32_t hashId = hashData(&userId, sizeof(userId));
				int32_t userIndex = rsys_hm_find(&s_gameData.roomData.userLookup, hashId);
				if (userIndex >= 0)
				{
					switch (callbackData->voipState)
					{
					case kNetStateConnected:
						rsys.print("VoipConnect: Connected with user %" PRId64 "\n", userId);
						s_gameData.roomData.userArray[userIndex].voipState = kVoipStateConnected;
						s_voipReadPtr = s_voipBuffer;
						s_voipWritePtr = s_voipBuffer;
						s_voipBytesWritten = 0;
						s_gameData.roomData.userArray[userIndex].voipStream = raud.createStream(&(raud_streamDesc) {
							.sampleRate = 48000,
							.numChannels = 1,
							.decodeBufferSize = VOIP_PCM_LENGTH,
							.length = VOIP_PCM_LENGTH,
							.pcmreadcallback = voipRead,
							.volume = kVolumeScale,
							.userData = &s_gameData.roomData.userArray[userIndex],
						});
						break;
					case kNetStateTimeout:
						rsys.print("VoipConnect: User %" PRId64 " timed out!\n", userId);
						s_gameData.roomData.userArray[userIndex].voipState = kVoipStateTimeout;
						raud.stopStream(s_gameData.roomData.userArray[userIndex].voipStream);
						s_gameData.roomData.userArray[userIndex].voipStream = NULL;
						break;
					case kNetStateClosed:
						rsys.print("VoipConnect: User %" PRId64 " has left the building!\n", userId);
						s_gameData.roomData.userArray[userIndex].voipState = kVoipStateClosed;
						raud.stopStream(s_gameData.roomData.userArray[userIndex].voipStream);
						s_gameData.roomData.userArray[userIndex].voipStream = NULL;
						break;
					}
				}
			}
		}
	});
}

void voipUpdate(void)
{
	int16_t voipBuffer[9600];
	size_t dataSize = 0;
	int32_t playerCount = sb_count(s_gameData.roomData.userArray);
	if (playerCount < 2)
		return;
	for (int32_t i = 0; i < playerCount; ++i)
	{
		if (s_gameData.roomData.userArray[i].userId != s_gameData.userId && s_gameData.roomData.userArray[i].voipState == kVoipStateConnected)
		{
			dataSize = rvrs.voipGetPCM(s_gameData.roomData.userArray[i].userId, voipBuffer, sizeof(voipBuffer));
			dataSize *= 2;
	//		rsys.print("VOIP: %d bytes read\n", dataSize);
			if (dataSize > 0)
			{
				int32_t bytesRemaining = (int32_t)dataSize;
				uint8_t* bufferEndPtr = (uint8_t*)s_voipBuffer + s_voipBufferSize;
				uint8_t* voipSrc = (uint8_t*)voipBuffer;
				uint8_t* voipDst = (uint8_t*)s_voipWritePtr;
				uint8_t* voipRead = (uint8_t*)s_voipReadPtr;
				while (bytesRemaining)
				{
					int32_t spaceAvailable = 0;
					if (voipDst >= voipRead)
						spaceAvailable = bufferEndPtr - voipDst;
					else
						spaceAvailable = voipRead - voipDst;
					assert(spaceAvailable > 0);
					assert(spaceAvailable <= (sizeof(int16_t) * VOIP_BUFFER_SIZE));
					int32_t bytesToCopy = spaceAvailable >= bytesRemaining ? bytesRemaining : spaceAvailable;
					memcpy(voipDst, voipSrc, bytesToCopy);
					voipSrc += bytesToCopy;
					voipDst += bytesToCopy;
					bytesRemaining -= bytesToCopy;
					if (voipDst >= bufferEndPtr)
					{
						voipDst = (uint8_t*)s_voipBuffer;
//						rsys.print("VOIP: Write ptr wrapped.\n");
					}
				}
				s_voipWritePtr = (int16_t*)voipDst;
				s_voipBytesWritten += dataSize;
			}

			vec4 camEye = kPlayerCamPos[1 - s_gameData.playerIndex];
			vec4 camAt = { 0.0f, 0.0f, 0.0f, 1.0f };
			vec4 camUp = { 0.0f, 1.0f, 0.0f, 0.0f };

			mat4x4 opponentHeadMatrix = mat4x4_create(s_gameData.opponentHeadRot, s_gameData.opponentHeadPos);
			mat4x4 camWorldMatrix = mat4x4_lookAtWorld(camEye, camAt, camUp);
			mat4x4 opponentHeadMatrixWS = mat4x4_mul(camWorldMatrix, opponentHeadMatrix);
//			vec4 voipDirection = !vec4_isZero(s_gameData.opponentHeadRot) ? s_gameData.opponentHeadRot : (vec4) { 0.0f, 0.0f, 1.0f, 0.0f };
			raud.updateSoundPosition(s_gameData.roomData.userArray[i].voipStream, (float*)&opponentHeadMatrixWS.wAxis, NULL, (float*)&opponentHeadMatrixWS.zAxis);
			raud.updateSoundVolume(s_gameData.roomData.userArray[i].voipStream, s_gameData.roomData.userArray[i].flags & kUserMuted ? 0.0f : kVolumeScale);
//			rsys.print("Pos: %.3f, %.3f, %.3f -> Dir: %.3f, %.3f, %.3f\n", opponentHeadMatrixWS.wAxis.x, opponentHeadMatrixWS.wAxis.y, opponentHeadMatrixWS.wAxis.z, opponentHeadMatrixWS.zAxis.x, opponentHeadMatrixWS.zAxis.y, opponentHeadMatrixWS.zAxis.z);
		}
	}
	assert(s_canary1 == 0xdeadbeef);
	assert(s_canary2 == 0xdeadbeef);
}

int voipRead(void* sound, void* data, unsigned int length)
{
	rvrs_userOvr* user = NULL;
	raud.getSoundUserData(sound, &user);
	assert(user != NULL);

	uint8_t* bufferEndPtr = (uint8_t*)s_voipBuffer + s_voipBufferSize;
	int32_t bytesRemaining = (int32_t)length;
	uint8_t* voipSrc = (uint8_t*)s_voipReadPtr;
	uint8_t* voipDst = (uint8_t*)data;
	uint8_t* voipWrite = (uint8_t*)s_voipWritePtr;

	int32_t bytesAvailable = 0;
	if (voipSrc <= voipWrite) {
		bytesAvailable = voipWrite - voipSrc;
	}
	else {
		bytesAvailable = (bufferEndPtr - voipSrc) + (voipWrite - (uint8_t*)s_voipBuffer);
	}
	while (bytesRemaining > 0)
	{
		assert(bytesRemaining > 0);
		if (bytesAvailable >= bytesRemaining)
		{
			int32_t bytesToCopy = (voipSrc <= voipWrite) ? bytesRemaining : (bufferEndPtr - voipSrc);
			bytesToCopy = bytesToCopy > length ? length : bytesToCopy;
			assert(bytesRemaining > 0);
			assert(bytesToCopy <= length);
			memcpy(voipDst, voipSrc, bytesToCopy);
			voipSrc += bytesToCopy;
			voipDst += bytesToCopy;
			bytesRemaining -= bytesToCopy;
			if (voipSrc >= bufferEndPtr)
			{
				voipSrc = (uint8_t*)s_voipBuffer;
			}
		}
		else
		{
			int32_t bytesToPad = bytesRemaining - bytesAvailable;
			assert(bytesToPad > 0);
			memset(voipDst, 0, bytesToPad);
			voipDst += bytesToPad;
			bytesRemaining -= bytesToPad;
		}
	}
	s_voipReadPtr = (int16_t*)voipSrc;
	s_voipBytesWritten = 0;

	return 0;
}

void updateAvatar(float dt)
{
	const float kOpponentFreeHandHeight = kDefaultTableHeight + 0.05f;
	const vec4 kVecUp = { 0.0f, 1.0f, 0.0f, 0.0f };
	const vec4 kAvatarOrigin[2] = { { 0.0f, 0.0f, -1.5f, 1.0f }, { 0.0f, 0.0f, 1.5f, 1.0f } };
	const vec4 kAvatarTarget = { 0.0f, 0.0f, 0.0f, 1.0f };
	const vec4 kAvatarHeight = { 0.0f, 1.5f, 0.0f, 0.0f };
	const vec4 kOpponentRightHandPosition[2] = { { -0.25f, kOpponentFreeHandHeight, -1.2f, 1.0f }, { 0.25f, kOpponentFreeHandHeight, 1.2f, 1.0f } };
	const vec4 kOpponentLeftHandOffset[2] =  { { -0.03f, 0.0f,  0.015f, 0.0f }, {  0.03f, 0.0f, -0.015,  0.0f } };
	const vec4 kOpponentRightHandOffset[2] = { {  0.03f, 0.0f, -0.015,  0.0f }, { -0.03f, 0.0f,  0.015f, 0.0f } };
	const vec4 kPaddlePos[2] = { {0.0f, 0.0f,  7.0f, 1.0f}, {0.0f, 0.0f, -7.0f, 1.0f} };

	bool isMultiPlayer = sb_count(s_gameData.roomData.userArray);
	for (int32_t idx = 0; idx < 2; ++idx)
	{
		if (isMultiPlayer && idx == 0 && s_gameData.opponentAvatarFlags & 0x4) {
			updateNetworkAvatar(dt);
			continue;
		}
#if 1
		int32_t playerIndex = s_gameData.playerIndex == 0 ? 1 - idx : idx;			//CLR: is the AirHockey game playerIndex, for this players opponent,
		int32_t opponentIndex = s_gameData.playerIndex == 0 ? idx : 1 - idx;		//CLR: is the avatar position index of this avatar's opponent.

		mat4x4 avatarWorldMatrix = mat4x4_lookAtWorld(kAvatarOrigin[opponentIndex], kAvatarTarget, kVecUp);
		mat4x4 avatarViewMatrix = mat4x4_orthoInverse(avatarWorldMatrix);
		//Update Opponent Hands.

		//Update Opponent Hands.
		vec4 opponentLeftHandPosition = (vec4){ GAME_TO_WORLD(kPaddlePos[playerIndex].x), kOpponentFreeHandHeight, GAME_TO_WORLD(kPaddlePos[playerIndex].z), 1.0f };
		opponentLeftHandPosition += kOpponentLeftHandOffset[opponentIndex];
		opponentLeftHandPosition = vec4_transform(avatarWorldMatrix, opponentLeftHandPosition - kAvatarOrigin[opponentIndex]);
		vec4 opponentRightHandPosition = kOpponentRightHandPosition[opponentIndex]; // { -0.25f, 0.83f, -1.2f, 1.0f };
//		opponentRightHandPosition += kOpponentRightHandOffset[opponentIndex];
		opponentRightHandPosition = vec4_transform(avatarWorldMatrix, opponentRightHandPosition - kAvatarOrigin[opponentIndex]);

		vec4 avatarElbowPos = { 0.1f, kOpponentFreeHandHeight, -1.5f, 1.0f };
		vec4 avatarArmTarget = opponentLeftHandPosition * (vec4) { -1.0f, 1.0f, 1.0f, 1.0f }; // Not sure why I need this right now.
		vec4 avatarArmVec = -avatarWorldMatrix.zAxis;

		float armRoll = opponentIndex == 0 ? 90.0f : -90.0f;
		mat4x4 armPointRotMat = mat4x4_lookAt(avatarArmTarget, avatarElbowPos, kVecUp);
		mat4x4 armRollRotMat = mat4x4_rotate(RADIANS(armRoll), avatarArmVec);
		mat4x4 combinedRotMat = mat4x4_mul(armRollRotMat, armPointRotMat);

		vec4 opponentLeftHandOrientation = quat_fromMat4x4(&combinedRotMat);
		vec4 opponentRightHandOrientation = quat_createFromAxisAngle(avatarArmVec, RADIANS(armRoll));

		//Update Opponent Head
		vec4 opponentHeadPosition = kAvatarOrigin[opponentIndex] + kAvatarHeight;
		opponentHeadPosition = vec4_transform(avatarWorldMatrix, opponentHeadPosition - kAvatarOrigin[opponentIndex]);

		vec4 opponentEyeTarget = (vec4){ GAME_TO_WORLD(0.0f), kDefaultTableHeight, GAME_TO_WORLD(5.0f), 1.0f };
		opponentEyeTarget = vec4_transform(avatarWorldMatrix, opponentEyeTarget - kAvatarOrigin[opponentIndex]);
		mat4x4 headRotMat = mat4x4_lookAt(opponentHeadPosition, opponentEyeTarget, kVecUp);
		vec4 opponentHeadOrientation = quat_fromMat4x4(&headRotMat);

		uint32_t leftButtonMask = 0;
		uint32_t leftTouchMask = 0;
		float leftJoystickX = 0.0f;
		float leftJoystickY = 0.0f;
		float leftIndexTrigger = 0.0f;
		float leftHandTrigger = 0.2f;

		uint32_t rightButtonMask = 0;
		uint32_t rightTouchMask = 0;
		float rightJoystickX = 0.0f;
		float rightJoystickY = 0.0f;
		float rightIndexTrigger = 0.0f;
		float rightHandTrigger = 0.0f;
		bool isAvatar = true;
		if (idx == 1 && rvrs.isHmdMounted())
		{
			if (rvrs.isTrackingControllers())
			{
				rvrs_rigidBodyPose leftControllerPose;
				rvrs.getTrackedControllerPose(0, &leftControllerPose);
				rvrs_rigidBodyPose rightControllerPose;
				rvrs.getTrackedControllerPose(1, &rightControllerPose);

				opponentLeftHandPosition = (vec4) { leftControllerPose.pose.position.x, leftControllerPose.pose.position.y, leftControllerPose.pose.position.z, 1.0f };
				opponentLeftHandOrientation = (vec4){ leftControllerPose.pose.orientation.x, leftControllerPose.pose.orientation.y, leftControllerPose.pose.orientation.z, leftControllerPose.pose.orientation.w };
				opponentRightHandPosition = (vec4) { rightControllerPose.pose.position.x, rightControllerPose.pose.position.y, rightControllerPose.pose.position.z, 1.0f };
				opponentRightHandOrientation = (vec4){ rightControllerPose.pose.orientation.x, rightControllerPose.pose.orientation.y, rightControllerPose.pose.orientation.z, rightControllerPose.pose.orientation.w };

				rvrs_controllerState leftControllerState;
				rvrs.getControllerState(0, &leftControllerState);
				leftButtonMask = 0;
				if (leftControllerState.buttons & ovrButton_X/* || leftControllerState.buttons & ovrButton_A*/)
					leftButtonMask |= kAvatarButton_One;
				if (leftControllerState.buttons & ovrButton_Y/* || leftControllerState.buttons & ovrButton_B*/)
					leftButtonMask |= kAvatarButton_Two;
#ifdef VRSYSTEM_OCULUS_WIN
				if (leftControllerState.buttons & ovrButton_Enter ||
					leftControllerState.buttons & ovrButton_Home)
#else
				if (leftControllerState.buttons & ovrButton_Enter)
#endif
					leftButtonMask |= kAvatarButton_Three;
				if (leftControllerState.buttons & ovrButton_LThumb /*||
					leftControllerState.buttons & ovrButton_RThumb*/)
					leftButtonMask |= kAvatarButton_Joystick;

				leftTouchMask = 0;
				if (leftControllerState.touches & ovrTouch_X /*|| leftControllerState.touches & ovrTouch_A*/)
					leftTouchMask |= kAvatarTouch_One;
				if (leftControllerState.touches & ovrTouch_Y /*|| leftControllerState.touches & ovrTouch_B*/)
					leftTouchMask |= kAvatarTouch_Two;
				if (leftControllerState.touches & ovrTouch_LThumb /*||
					leftControllerState.touches & ovrTouch_RThumb*/)
					leftTouchMask |= kAvatarTouch_Joystick;
#ifdef VRSYSTEM_OCULUS_WIN
				if (leftControllerState.touches & ovrTouch_LThumbRest /*||
					leftControllerState.touches & ovrTouch_RThumbRest*/)
					leftTouchMask |= kAvatarTouch_ThumbRest;
				if (leftControllerState.touches & ovrTouch_LIndexTrigger /*||
					leftControllerState.touches & ovrTouch_RIndexTrigger*/)
#else
				if (leftControllerState.touches & ovrTouch_IndexTrigger)
#endif
					leftTouchMask |= kAvatarTouch_Index;
#ifdef VRSYSTEM_OCULUS_WIN
				if (leftControllerState.touches & ovrTouch_LIndexPointing /*||
					leftControllerState.touches & ovrTouch_RIndexPointing*/)
#else
				if (leftControllerState.touches & ovrTouch_IndexPointing)
#endif
					leftTouchMask |= kAvatarTouch_Pointing;
#ifdef VRSYSTEM_OCULUS_WIN
				if (leftControllerState.touches & ovrTouch_LThumbUp /*||
					leftControllerState.touches & ovrTouch_RThumbUp*/)
#else
				if (leftControllerState.touches & ovrTouch_ThumbUp)
#endif
					leftTouchMask |= kAvatarTouch_ThumbUp;

				leftJoystickX = 0.0f;
				leftJoystickY = 0.0f;
				leftButtonMask = leftButtonMask;
				leftTouchMask = leftTouchMask;
				leftIndexTrigger = leftControllerState.trigger;
				leftHandTrigger = s_gameData.leftHandGripValue;

				rvrs_controllerState rightControllerState;
				rvrs.getControllerState(1, &rightControllerState);
				rightButtonMask = 0;
				if (/*rightControllerState.buttons & ovrButton_X ||*/ rightControllerState.buttons & ovrButton_A)
					rightButtonMask |= kAvatarButton_One;
				if (/*rightControllerState.buttons & ovrButton_Y ||*/ rightControllerState.buttons & ovrButton_B)
					rightButtonMask |= kAvatarButton_Two;
#ifdef VRSYSTEM_OCULUS_WIN
				if (rightControllerState.buttons & ovrButton_Enter ||
					rightControllerState.buttons & ovrButton_Home)
#else
				if (rightControllerState.buttons & ovrButton_Enter)
#endif
					rightButtonMask |= kAvatarButton_Three;
				if (/*rightControllerState.buttons & ovrButton_LThumb ||*/
					rightControllerState.buttons & ovrButton_RThumb)
					rightButtonMask |= kAvatarButton_Joystick;

				rightTouchMask = 0;
				if (rightControllerState.touches & ovrTouch_A)
					rightTouchMask |= kAvatarTouch_One;
				if (rightControllerState.touches & ovrTouch_B)
					rightTouchMask |= kAvatarTouch_Two;
				if (rightControllerState.touches & ovrTouch_RThumb)
					rightTouchMask |= kAvatarTouch_Joystick;
#ifdef VRSYSTEM_OCULUS_WIN
				if (rightControllerState.touches & ovrTouch_RThumbRest)
					rightTouchMask |= kAvatarTouch_ThumbRest;
				if (rightControllerState.touches & ovrTouch_RIndexTrigger)
#else
				if (rightControllerState.touches & ovrTouch_IndexTrigger)
#endif
					rightTouchMask |= kAvatarTouch_Index;
#ifdef VRSYSTEM_OCULUS_WIN
				if (rightControllerState.touches & ovrTouch_RIndexPointing)
#else
				if (rightControllerState.touches & ovrTouch_IndexPointing)
#endif
					rightTouchMask |= kAvatarTouch_Pointing;
#ifdef VRSYSTEM_OCULUS_WIN
				if (rightControllerState.touches & ovrTouch_RThumbUp)
#else
				if (rightControllerState.touches & ovrTouch_ThumbUp)
#endif
					rightTouchMask |= kAvatarTouch_ThumbUp;

				rightJoystickX = 0.0f;
				rightJoystickY = 0.0f;
				rightButtonMask = rightButtonMask;
				rightTouchMask = rightTouchMask;
				rightIndexTrigger = rightControllerState.trigger;
				rightHandTrigger = s_gameData.rightHandGripValue;
			}
			else
			{
			//Hand Tracking
				isAvatar = false;
			}
		}

		rvrs.updateAvatarFrameParams(1-idx, dt, &(rvrs_avatarFrameParams) {
			.eyePos = opponentHeadPosition,
			.eyeRot = opponentHeadOrientation,
			.hands[0] = {
				.position = opponentLeftHandPosition,
				.rotation = opponentLeftHandOrientation,
				.buttonMask = leftButtonMask,
				.touchMask = leftTouchMask,
				.joystickX = leftJoystickX,
				.joystickY = leftJoystickY,
				.indexTrigger = leftIndexTrigger,
				.handTrigger = leftHandTrigger,
				.isActive = true,
				.isAvatar = isAvatar,
			},
			.hands[1] = {
				.position = opponentRightHandPosition,
				.rotation = opponentRightHandOrientation,
				.buttonMask = rightButtonMask,
				.touchMask = rightTouchMask,
				.joystickX = rightJoystickX,
				.joystickY = rightJoystickY,
				.indexTrigger = rightIndexTrigger,
				.handTrigger = rightHandTrigger,
				.isActive = true,
				.isAvatar = isAvatar,
			},
			.avatarWorldMatrix = /*idx == 0 ? mat4x4_identity() :*/ avatarWorldMatrix,
			.playerIndex = idx,
		});
#endif
	}
}

void updateNetworkAvatar(float dt)
{
	const vec4 kVecUp = { 0.0f, 1.0f, 0.0f, 0.0f };
	const vec4 kAvatarOrigin[2] = { { 0.0f, 0.0f, -1.5f, 1.0f }, { 0.0f, 0.0f, 1.5f, 1.0f } };
	const vec4 kAvatarTarget = { 0.0f, 0.0f, 0.0f, 1.0f };
	const float kOpponentFreeHandHeight = kDefaultTableHeight + 0.05f;
	const vec4 kOpponentLeftHandOffset[2] =  { { -0.03f, 0.0f,  0.015f, 0.0f }, {  0.03f, 0.0f, -0.015,  0.0f } };
	const vec4 kOpponentRightHandOffset[2] = { {  0.03f, 0.0f, -0.015,  0.0f }, { -0.03f, 0.0f,  0.015f, 0.0f } };

	int32_t opponentIndex = 1 - s_gameData.playerIndex;
	mat4x4 avatarWorldMatrix = mat4x4_lookAtWorld(kAvatarOrigin[s_gameData.playerIndex], kAvatarTarget, kVecUp);

	vec4 opponentHeadPosition = s_gameData.opponentHeadPos;
	quat opponentHeadOrientation = s_gameData.opponentHeadRot;
	vec4 opponentLeftHandPosition = s_gameData.opponentHandsPos[0];
	vec4 opponentRightHandPosition = s_gameData.opponentHandsPos[1];
	vec4 opponentLeftHandOrientation = s_gameData.opponentHandsRot[0];
	vec4 opponentRightHandOrientation = s_gameData.opponentHandsRot[1];

	uint32_t leftButtonMask = 0;
	uint32_t leftTouchMask = 0;
	float leftJoystickX = 0.0f;
	float leftJoystickY = 0.0f;
	float leftIndexTrigger = 0.0f;
	float leftHandTrigger = 0.2f;

	uint32_t rightButtonMask = 0;
	uint32_t rightTouchMask = 0;
	float rightJoystickX = 0.0f;
	float rightJoystickY = 0.0f;
	float rightIndexTrigger = 0.0f;
	float rightHandTrigger = 0.0f;
	bool isAvatar = true;

	leftButtonMask = 0;
	if (s_gameData.opponentButtons & ovrButton_X)
		leftButtonMask |= kAvatarButton_One;
	if (s_gameData.opponentButtons & ovrButton_Y)
		leftButtonMask |= kAvatarButton_Two;
#ifdef VRSYSTEM_OCULUS_WIN
	if (s_gameData.opponentButtons & ovrButton_Enter ||
		s_gameData.opponentButtons & ovrButton_Home)
#else
	if (s_gameData.opponentButtons & ovrButton_Enter)
#endif
		leftButtonMask |= kAvatarButton_Three;
	if (s_gameData.opponentButtons & ovrButton_LThumb)
		leftButtonMask |= kAvatarButton_Joystick;

	leftTouchMask = 0;
	if (s_gameData.opponentTouches & ovrTouch_X)
		leftTouchMask |= kAvatarTouch_One;
	if (s_gameData.opponentTouches & ovrTouch_Y)
		leftTouchMask |= kAvatarTouch_Two;
	if (s_gameData.opponentTouches & ovrTouch_LThumb)
		leftTouchMask |= kAvatarTouch_Joystick;
#ifdef VRSYSTEM_OCULUS_WIN
	if (s_gameData.opponentTouches & ovrTouch_LThumbRest)
		leftTouchMask |= kAvatarTouch_ThumbRest;
	if (s_gameData.opponentTouches & ovrTouch_LIndexTrigger)
#else
	if (s_gameData.opponentTouches & ovrTouch_IndexTrigger)
#endif
		leftTouchMask |= kAvatarTouch_Index;
#ifdef VRSYSTEM_OCULUS_WIN
	if (s_gameData.opponentTouches & ovrTouch_LIndexPointing)
#else
	if (s_gameData.opponentTouches & ovrTouch_IndexPointing)
#endif
		leftTouchMask |= kAvatarTouch_Pointing;
#ifdef VRSYSTEM_OCULUS_WIN
	if (s_gameData.opponentTouches & ovrTouch_LThumbUp)
#else
	if (s_gameData.opponentTouches & ovrTouch_ThumbUp)
#endif
		leftTouchMask |= kAvatarTouch_ThumbUp;

	leftJoystickX = 0.0f;
	leftJoystickY = 0.0f;
	leftButtonMask = leftButtonMask;
	leftTouchMask = leftTouchMask;
	leftIndexTrigger = s_gameData.opponentTrigger[0];
	leftHandTrigger = s_gameData.opponentGrip[0];

	rightButtonMask = 0;
	if (s_gameData.opponentButtons & ovrButton_A)
		rightButtonMask |= kAvatarButton_One;
	if (s_gameData.opponentButtons & ovrButton_B)
		rightButtonMask |= kAvatarButton_Two;
#ifdef VRSYSTEM_OCULUS_WIN
	if (s_gameData.opponentButtons & ovrButton_Enter ||
		s_gameData.opponentButtons & ovrButton_Home)
#else
	if (s_gameData.opponentButtons & ovrButton_Enter)
#endif
		rightButtonMask |= kAvatarButton_Three;
	if (s_gameData.opponentButtons & ovrButton_RThumb)
		rightButtonMask |= kAvatarButton_Joystick;

	rightTouchMask = 0;
	if (s_gameData.opponentTouches & ovrTouch_A)
		rightTouchMask |= kAvatarTouch_One;
	if (s_gameData.opponentTouches & ovrTouch_B)
		rightTouchMask |= kAvatarTouch_Two;
	if (s_gameData.opponentTouches & ovrTouch_RThumb)
		rightTouchMask |= kAvatarTouch_Joystick;
#ifdef VRSYSTEM_OCULUS_WIN
	if (s_gameData.opponentTouches & ovrTouch_RThumbRest)
		rightTouchMask |= kAvatarTouch_ThumbRest;
	if (s_gameData.opponentTouches & ovrTouch_RIndexTrigger)
#else
	if (s_gameData.opponentTouches & ovrTouch_IndexTrigger)
#endif
		rightTouchMask |= kAvatarTouch_Index;
#ifdef VRSYSTEM_OCULUS_WIN
	if (s_gameData.opponentTouches & ovrTouch_RIndexPointing)
#else
	if (s_gameData.opponentTouches & ovrTouch_IndexPointing)
#endif
		rightTouchMask |= kAvatarTouch_Pointing;
#ifdef VRSYSTEM_OCULUS_WIN
	if (/*rightControllerState.touches & ovrTouch_LThumbUp ||*/
		s_gameData.opponentTouches & ovrTouch_RThumbUp)
#else
	if (s_gameData.opponentTouches & ovrTouch_ThumbUp)
#endif
		rightTouchMask |= kAvatarTouch_ThumbUp;

	rightJoystickX = 0.0f;
	rightJoystickY = 0.0f;
	rightButtonMask = rightButtonMask;
	rightTouchMask = rightTouchMask;
	rightIndexTrigger = s_gameData.opponentTrigger[1];
	rightHandTrigger = s_gameData.opponentGrip[1];

	rvrs.updateAvatarFrameParams(1, dt, &(rvrs_avatarFrameParams) {
		.eyePos = opponentHeadPosition,
		.eyeRot = opponentHeadOrientation,
		.hands[0] = {
			.position = opponentLeftHandPosition,
			.rotation = opponentLeftHandOrientation,
			.buttonMask = leftButtonMask,
			.touchMask = leftTouchMask,
			.joystickX = leftJoystickX,
			.joystickY = leftJoystickY,
			.indexTrigger = leftIndexTrigger,
			.handTrigger = leftHandTrigger,
			.isActive = true,
			.isAvatar = isAvatar,
		},
		.hands[1] = {
			.position = opponentRightHandPosition,
			.rotation = opponentRightHandOrientation,
			.buttonMask = rightButtonMask,
			.touchMask = rightTouchMask,
			.joystickX = rightJoystickX,
			.joystickY = rightJoystickY,
			.indexTrigger = rightIndexTrigger,
			.handTrigger = rightHandTrigger,
			.isActive = true,
			.isAvatar = isAvatar,
		},
		.avatarWorldMatrix = avatarWorldMatrix,
		.playerIndex = 0,
	});
}

void showGUI(float dt)
{
	mat4x4 localHeadMatrix = rvrs.isHmdMounted() ? mat4x4_create(s_gameData.headOrientation, s_gameData.headPosition) : mat4x4_identity();
	mat4x4 leftControllerLocalMatrix = rvrs.getTrackedControllerMatrix(0);
	mat4x4 rightControllerLocalMatrix = rvrs.getTrackedControllerMatrix(1);
	mat4x4 worldMatrix = mat4x4_mul(s_gameData.camWorldMatrix, localHeadMatrix);
	mat4x4 leftControllerWorldMatrix = mat4x4_mul(s_gameData.camWorldMatrix, leftControllerLocalMatrix);
	mat4x4 rightControllerWorldMatrix = mat4x4_mul(s_gameData.camWorldMatrix, rightControllerLocalMatrix);

	quat controllerOrientation = (quat){ s_gameData.handPoses[0].pose.orientation.x, s_gameData.handPoses[0].pose.orientation.y, s_gameData.handPoses[0].pose.orientation.z, s_gameData.handPoses[0].pose.orientation.w };
	vec4 controllerPosition = (vec4){ s_gameData.handPoses[0].pose.position.x, s_gameData.handPoses[0].pose.position.y, s_gameData.handPoses[0].pose.position.z, 1.0f };

	rvrs_controllerState leftControllerState;
	rvrs.getControllerState(0, &leftControllerState);

	rvrs_controllerState rightControllerState;
	rvrs.getControllerState(1, &rightControllerState);

	vec4 pointerStartPosition = s_gameData.lastUsedHand == kLeftHand ? leftControllerWorldMatrix.wAxis : rightControllerWorldMatrix.wAxis;
	vec4 pointerDelta = s_gameData.lastUsedHand == kLeftHand ? leftControllerWorldMatrix.zAxis : rightControllerWorldMatrix.zAxis;
	vec4 pointerEndPosition = pointerStartPosition - 2.0f * pointerDelta;
	float triggerValue = s_gameData.lastUsedHand == kLeftHand ? leftControllerState.trigger : rightControllerState.trigger;

	rgui.newFrame(&(rgui_frameDesc) {
		.hmdTransform = worldMatrix,
		.controllerTransform = s_gameData.lastUsedHand == kLeftHand ? leftControllerWorldMatrix : rightControllerWorldMatrix,
		.controllerTrigger = triggerValue,
		.pointerHand = (int32_t)s_gameData.lastUsedHand
	});

	if (vec4_isZero(s_gameData.guiTransformMain.wAxis))
	{
		float guiAngle = s_gameData.playerIndex == 0 ? 0.0f : 180.0f;
		quat rot1 = quat_createFromAxisAngle((vec4) { 0.0f, 1.0f, 0.0f, 0.0f }, RADIANS(guiAngle));
		quat rot2 = quat_createFromAxisAngle((vec4) { 1.0f, 0.0f, 0.0f, 0.0f }, RADIANS(-5.0f));
		quat rot = quat_mul(rot1, rot2);
		vec4 pos = s_gameData.playerIndex == 0 ? (vec4){ 0.0f, 0.90f, 1.2f, 1.0f } : (vec4){ 0.0f, 0.90f, -1.2f, 1.0f };
		s_gameData.guiTransformMain = mat4x4_create(rot, pos);
	}
	float menuPosZ = rvrs.isHmdMounted() ? 0.0f : s_gameData.playerIndex == 0 ? 1.0f : -1.0f;
	s_gameData.guiTransformMain.wAxis.z = menuPosZ;

	switch (s_gameData.guiMode)
	{
	case kGUIOptions:
		showOptionsPanel();
		break;
	case kGUICredits:
		showCreditsPanel();
		break;
	case kGUISettings:
		showSettingsPanel();
		break;
	case kGUIOnline:
		showOnlinePanel();
		break;
	case kGUIWarning:
		showWarningPanel();
		break;
	case kGUIOnlineWarning:
		showOnlineWarningPanel();
		break;
	default:
		assert(0); // CLR - Shouldn't hit this.
	}

	if (rvrs.isHmdMounted())
	{
		rgui.pointer(&(rgui_pointerDesc) {
			.transform = worldMatrix,
			.startPosition = pointerStartPosition,
			.endPosition = pointerEndPosition,
			.color = (vec4){ 1.0f, 1.0f, 1.0f, 1.0f },
			.thickness = 0.01f,
		});
	}
	else
	{
		vec4 scrPos0 = (vec4){ s_gameData.mousePosNdcX, s_gameData.mousePosNdcY, 0.0f, 1.0f };
		vec4 scrPos1 = (vec4){ s_gameData.mousePosNdcX, s_gameData.mousePosNdcY, 1.0f, 1.0f };

		mat4x4 viewProj = mat4x4_mul(s_gameData.projectionMatrix, s_gameData.camViewMatrix);
		mat4x4 invViewProj = mat4x4_inverse(viewProj);
		vec4 worldPos0 = vec4_transform(invViewProj, scrPos0);
		vec4 worldPos1 = vec4_transform(invViewProj, scrPos1);
		worldPos0 = worldPos0.xyzw / worldPos0.w;
		worldPos1 = worldPos1.xyzw / worldPos1.w;
		rgui.mouseRay(worldPos0, worldPos1);
		if (s_gameData.mousePresses > 0)
		{
			triggerValue = 1.0f;
		}
	}

	rgui.update(triggerValue > 0.1f);
}

void showOptionsPanel(void)
{
	const bool bGameInProgress = false;
	const float kEndGameButtonHeight = bGameInProgress ? 0.099f : 0.0f;
	const float kButtonLayerHeight = 0.150f;
	const float kButtonPosY = -0.036f;									//CLR - Y position that buttons are drawn at.
	const float kPanelWidth = 1.0f;
	const float kPanelHeight = 0.678f + kButtonLayerHeight + kEndGameButtonHeight;
	const float kBannerHeight = 0.08f;
	const float kPanelLeft = -kPanelWidth * 0.5;
	rgui_panel mainPanel = rgui.beginPanel(&(rgui_panelDesc) {
		.transform = s_gameData.guiTransformMain,
		.color = (vec4){ 0.2f, 0.2f, 0.2f, 1.0f },
		.width = kPanelWidth,
		.height = kPanelHeight,
		.bannerColor = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
		.bannerHeight = kBannerHeight,
		.bannerText = "OPTIONS",
		.bannerFont = s_gameData.font,
		.layoutFlags = kAlignHCenter,
		.flags = kCollidable,
	});

	float menuBannerPosY = kPanelHeight - kBannerHeight;

	float cursorPosX = kPanelLeft + 0.4f;
	float cursorPosY = menuBannerPosY - (kBannerHeight + 0.064f);		//cursorPosY = 0.604
	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Difficulty:",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHRight | kAlignVCenter,
	});

	float sliderLeft = cursorPosX + 0.064f;
	float sliderRight = kPanelLeft + kPanelWidth - 0.064;
	float sliderWidth = sliderRight - sliderLeft;
	float sliderTextPosX = (sliderLeft + sliderWidth * 0.5f);
	rgui.slider(&(rgui_sliderDesc) {
		.x = sliderLeft,
		.y = cursorPosY,
		.color = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
		.action = ^ (void) {
			raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
		},
		.value = &s_gameData.difficultySlider,
		.minValue = 1.0f,
		.maxValue = 3.0f,
		.step = 1.0f,
		.width = sliderWidth,
		.height = 0.064f,
		.thickness = 0.016f,
		.panel = mainPanel,
		.sliderTexture = s_gameData.sliderTexture,
		.layoutFlags = kAlignHLeft,
	});

	float labelPosY = cursorPosY - 0.050f;

	static char difficultyTextBuffer[8] = { 0 };
	const char* kDIfficultTextStrings[3] = {
		"Easy", "Medium", "Hard"
	};
	strcpy(difficultyTextBuffer, kDIfficultTextStrings[(int32_t)(s_gameData.difficultySlider - 1.0f)]);
	rgui.text3D(&(rgui_textDesc) {
		.x = sliderTextPosX,
		.y = labelPosY, //cursorPosY,
		.scale = 50.0f,
		.text = difficultyTextBuffer,
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});
	cursorPosY -= 0.128f;												//cursorPosY = 0.476

	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Volume:",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHRight | kAlignVCenter,
	});

	rgui.slider(&(rgui_sliderDesc) {
		.x = sliderLeft,
		.y = cursorPosY,
		.color = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
//		.action = ^ (void) {
//			s_gameData.settingsDataDirty = true;
//		},
		.value = &s_gameData.masterVolume,
		.minValue = 0.0f,
		.maxValue = 11.0f,
		.width = sliderWidth,
		.height = 0.064f,
		.thickness = 0.016f,
		.panel = mainPanel,
		.sliderTexture = s_gameData.sliderTexture,
		.layoutFlags = kAlignHLeft,
	});
	labelPosY = cursorPosY - 0.050f;

	float dummy = 0.0f;
	float volumeSliderValue = s_gameData.masterVolume;
	float volumeFrac = modf(s_gameData.masterVolume, &dummy);
	static char volumeTextBuffer[4] = { 0 };
	sprintf(volumeTextBuffer, "%2d", (int32_t)(volumeFrac > 0.5f ? ceilf(volumeSliderValue) : floorf(volumeSliderValue)));
	rgui.text3D(&(rgui_textDesc) {
		.x = sliderTextPosX,
		.y = labelPosY,
		.scale = 50.0f,
		.text = volumeTextBuffer,
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});
	kVolumeScale = volumeSliderValue;
	cursorPosY -= 0.128f;												//cursorPosY = 0.348

	{
		rgui.text3D(&(rgui_textDesc) {
			.x = cursorPosX,
				.y = cursorPosY,
				.scale = 64.0f,
				.text = "Table Height:",
				.font = s_gameData.font,
				.panel = mainPanel,
				.layoutFlags = kAlignHRight | kAlignVCenter,
		});

		static float s_tableOffset = 0.0f;
		rgui.slider(&(rgui_sliderDesc) {
			.x = sliderLeft,
			.y = cursorPosY,
			.color = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
//			.action = ^ (void) {
//				s_gameData.settingsDataDirty = true;
//			},
			.value = &s_tableOffset,
			.minValue = 0.0f,
			.maxValue = 5.0f,
			.width = sliderWidth,
			.height = 0.064f,
			.thickness = 0.016f,
			.panel = mainPanel,
			.sliderTexture = s_gameData.sliderTexture,
			.layoutFlags = kAlignHLeft,
		});
		labelPosY = cursorPosY - 0.050f;

		static char tableHeightTextBuffer[32] = { 0 };
		float offsetCM = (kDefaultTableHeight * 100.0f) + s_tableOffset * kTableOffsetScale * 100.0f;
		float offsetFrac = modf(offsetCM, &dummy);
		float offsetInch = offsetCM / 2.54f;
		sprintf(tableHeightTextBuffer, "%.1fcm (%.1f\")", offsetCM , offsetInch > 0.1f ? offsetInch : 0.0f);
		rgui.text3D(&(rgui_textDesc) {
			.x = sliderTextPosX,
			.y = labelPosY,
			.scale = 50.0f,
			.text = tableHeightTextBuffer,
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});
		cursorPosY -= 0.128f;											//cursorPosY = 0.22
	}

	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Lights:",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHRight | kAlignVCenter,
	});

	rgui.slider(&(rgui_sliderDesc) {
		.x = sliderLeft,
		.y = cursorPosY,
		.color = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
//		.action = ^ (void) {
//			s_gameData.settingsDataDirty = true;
//		},
		.value = &s_gameData.brightness,
		.minValue = 0.01f,
		.maxValue = 1.0f,
		.width = sliderWidth,
		.height = 0.064f,
		.thickness = 0.016f,
		.panel = mainPanel,
		.sliderTexture = s_gameData.sliderTexture,
		.layoutFlags = kAlignHLeft,
	});
	labelPosY = cursorPosY - 0.050f;
	s_gameData.brightness = s_gameData.brightness < 0.01f ? 0.01f : s_gameData.brightness;
	s_gameData.brightness = s_gameData.brightness > 0.995f ? 1.0f : s_gameData.brightness;

	static char brightnessTextBuffer[5] = { 0 };
	sprintf(brightnessTextBuffer, "%d%%", (int32_t)(s_gameData.brightness * 100.0f));
	rgui.text3D(&(rgui_textDesc) {
		.x = sliderTextPosX,
		.y = labelPosY,
		.scale = 50.0f,
		.text = brightnessTextBuffer,
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});
	cursorPosY -= 0.128f;												//cursorPosY = 0.092

	{
		cursorPosY -= (0.128);											//cursorPosY = -0.036
		const float kButtonWidth = 0.250f;
#ifdef DISABLE_ROOM_BUTTON
		const float creditsButtonPos = kPanelLeft + (kPanelWidth * 0.5f) + kPanelWidth * 0.275f;;
		const float settingsButtonPos = kPanelLeft + (kPanelWidth * 0.5f) - kPanelWidth * 0.275f;;
#else
		const float inviteButtonPos = kPanelLeft + (kPanelWidth * 0.5f) - kPanelWidth * 0.30f;
		const float creditsButtonPos = kPanelLeft + (kPanelWidth * 0.5f) + kPanelWidth * 0.30f;
		const float settingsButtonPos = kPanelLeft + (kPanelWidth * 0.5f);
		rgui.button(&(rgui_buttonDesc) {
			.x = inviteButtonPos,
			.y = kButtonPosY,
			.width = kButtonWidth,
			.height = 0.256f,
			.panel = mainPanel,
			.texture = s_gameData.tableButtonBlueTexture,
			.layoutFlags = kAlignHCenter,
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
				s_gameData.guiMode = kGUIOnline;
				//				s_gameData.bShowCredits = true;
			},
		});
		rgui.text3D(&(rgui_textDesc) {
			.x = inviteButtonPos,
			.y = kButtonPosY + 0.09f,
			.z = 2.0f,
			.scale = 64.0f,
			.text = "Online",
			.color = (rgfx_debugColor){0xFF9C5121}, 
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});
#endif
		rgui.button(&(rgui_buttonDesc) {
			.x = settingsButtonPos,
			.y = kButtonPosY,
			.width = kButtonWidth,
			.height = 0.256f,
			.panel = mainPanel,
			.texture = s_gameData.tableButtonTexture,
			.layoutFlags = kAlignHCenter,
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
				s_gameData.guiMode = kGUISettings;
			}
		});
		rgui.text3D(&(rgui_textDesc) {
			.x = settingsButtonPos,
			.y = kButtonPosY + 0.09f,
			.z = 2.0f,
			.scale = 64.0f,
			.text = "Settings",
			.color = (rgfx_debugColor){ 0xFF9C5121 }, // kDebugColorBlue,
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});

		rgui.button(&(rgui_buttonDesc) {
			.x = creditsButtonPos,
			.y = kButtonPosY,
			.width = kButtonWidth,
			.height = 0.256f,
			.panel = mainPanel,
			.texture = s_gameData.tableButtonRedTexture,
			.layoutFlags = kAlignHCenter,
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
				s_gameData.guiMode = kGUICredits;
//				s_gameData.bShowCredits = true;
			}
		});
		rgui.text3D(&(rgui_textDesc) {
			.x = creditsButtonPos,
			.y = kButtonPosY + 0.09f,
			.z = 2.0f,
			.scale = 64.0f,
			.text = "Credits",
			.color = (rgfx_debugColor){ 0xFF9C5121 }, // kDebugColorBlue,
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});
	}

/*
	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Avatar:",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHRight | kAlignVCenter,
	});

	rgui.button(&(rgui_buttonDesc) {
		.x = sliderLeft + 0.064f,
		.y = cursorPosY,
		.width = 0.064f,
		.height = 0.064f,
		.panel = mainPanel,
		.texture = s_gameData.prevButtonTexture,
		.layoutFlags = kAlignHLeft,
		.action = ^(void) {
			uint64_t userId = cycleAvatars(-1);
			if (rvrs.requestAvatarSpecOvr(userId)) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
			}
		}
	});

	rgui.button(&(rgui_buttonDesc) {
		.x = sliderLeft + sliderWidth - (0.064f * 2.0f),
		.y = cursorPosY,
		.width = 0.064f,
		.height = 0.064f,
		.panel = mainPanel,
		.texture = s_gameData.nextButtonTexture,
		.layoutFlags = kAlignHLeft,
		.action = ^(void) {
			uint64_t userId = cycleAvatars(1);
			if (rvrs.requestAvatarSpecOvr(userId)) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
			}
		}
	});
*/
}

void showSettingsPanel(void)
{
	const float kButtonLayerHeight = 0.150f;
	const float kButtonPosY = -0.036f;									//CLR - Y position that buttons are drawn at.
	const float kPanelWidth = 1.0f;
	const float kPanelHeight = 0.678f + kButtonLayerHeight;
	const float kBannerHeight = 0.08f;
	const float kPanelLeft = -kPanelWidth * 0.5;
	rgui_panel mainPanel = rgui.beginPanel(&(rgui_panelDesc) {
		.transform = s_gameData.guiTransformMain,
		.color = (vec4){ 0.2f, 0.2f, 0.2f, 1.0f },
		.width = kPanelWidth,
		.height = kPanelHeight,
		.bannerColor = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
		.bannerHeight = kBannerHeight,
		.bannerText = "SETTINGS",
		.bannerFont = s_gameData.font,
		.layoutFlags = kAlignHCenter,
		.flags = kCollidable,
	});

	float menuBannerPosY = kPanelHeight - kBannerHeight;

	float cursorPosX = kPanelLeft + 0.3f;
	float cursorPosY = menuBannerPosY - (kBannerHeight + 0.064f);

	float sliderLeft = cursorPosX + 0.064f;
	float sliderRight = kPanelLeft + kPanelWidth - 0.064;
	float sliderWidth = sliderRight - sliderLeft;
	float sliderTextPosX = (sliderLeft + sliderWidth * 0.5f);
	float labelPosY = cursorPosY - 0.050f;

	if (s_gameData.numSupportedRefreshRates > 1)
	{
		rgui.text3D(&(rgui_textDesc) {
			.x = cursorPosX,
			.y = cursorPosY,
			.scale = 64.0f,
			.text = "FPS:",
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHRight | kAlignVCenter,
		});

		rgui.slider(&(rgui_sliderDesc) {
			.x = sliderLeft,
			.y = cursorPosY,
			.color = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
			},
			.value = &s_gameData.requestedFramerateSlider,
			.minValue = 0.0f,
			.maxValue = s_gameData.numSupportedRefreshRates - 1,
			.step = 1.0f,
			.width = sliderWidth,
			.height = 0.064f,
			.thickness = 0.016f,
			.panel = mainPanel,
			.sliderTexture = s_gameData.sliderTexture,
			.layoutFlags = kAlignHLeft,
		});
		labelPosY = cursorPosY - 0.050f;

		static char frameRateTextBuffer[6] = { 0 };
		//	strcpy_s(difficultyTextBuffer, sizeof(difficultyTextBuffer), kDIfficultTextStrings[(int32_t)(s_gameData.difficultySlider-1.0f)]);
		sprintf(frameRateTextBuffer, "%dHz", (int32_t)s_gameData.supportedRefreshRates[(int32_t)s_gameData.requestedFramerateSlider]);
		rgui.text3D(&(rgui_textDesc) {
			.x = sliderTextPosX,
			.y = labelPosY,
			.scale = 50.0f,
			.text = frameRateTextBuffer,
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});
		cursorPosY -= 0.128f;
	}
	else
	{
		if (1)
		{
			const float kButtonWidth = 0.250f;
			const float buttonPos = 0.0f;

			rgui.text3D(&(rgui_textDesc) {
				.x = cursorPosX + kButtonWidth * 0.3f,
				.y = cursorPosY - 0.05f + 0.09f,
				.scale = 64.0f,
				.text = "Press to ",
				.font = s_gameData.font,
				.panel = mainPanel,
				.layoutFlags = kAlignHRight | kAlignVCenter,
			});

			rgui.text3D(&(rgui_textDesc) {
				.x = cursorPosX + kButtonWidth + kButtonWidth * 0.35f,
				.y = cursorPosY - 0.05f + 0.09f,
				.scale = 64.0f,
				.text = "settings",
				.font = s_gameData.font,
				.panel = mainPanel,
				.layoutFlags = kAlignHLeft | kAlignVCenter,
			});

			rgui.button(&(rgui_buttonDesc) {
				.x = buttonPos,
				.y = cursorPosY - 0.05f,
				.width = kButtonWidth,
				.height = 0.256f,
				.panel = mainPanel,
				.texture = s_gameData.tableButtonTexture,
				.layoutFlags = kAlignHCenter,
				.action = ^ (void) {
					raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
					s_gameData.requestedEyeScaleSlider = kSuggestedEyeScaleForRefreshRateQuest[0];
					s_gameData.requestedFoveationSlider = kSuggestedFFRForRefreshRateQuest[0];
				}
			});
			rgui.text3D(&(rgui_textDesc) {
				.x = buttonPos,
				.y = cursorPosY - 0.05f + 0.09f,
				.z = 2.0f,
				.scale = 64.0f,
				.text = "Restore",
				.color = (rgfx_debugColor){ 0xFF9C5121 }, // kDebugColorBlue,
				.font = s_gameData.font,
				.panel = mainPanel,
				.layoutFlags = kAlignHCenter | kAlignVCenter,
			});
		}
	}

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY - 0.050f,
		.scale = 64.0f,
		.text = "ADVANCED SETTINGS!",
		.font = s_gameData.font,
		.color = kDebugColorRed,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.128f;

	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Eye Scale:",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHRight | kAlignVCenter,
	});

//	float restoreEyeScaleSlider = s_gameData.currentEyeScaleSlider;
	rgui.slider(&(rgui_sliderDesc) {
		.x = sliderLeft,
		.y = cursorPosY,
		.color = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
		.action = ^ (void) {
			if (!s_gameData.bSettingsWarningHeeded)
			{
				s_gameData.guiMode = kGUIWarning;
				s_gameData.requestedEyeScaleSlider = s_gameData.currentEyeScaleSlider;
			}
			else
			{
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
			}
		},
		.value = &s_gameData.requestedEyeScaleSlider,
		.minValue = 0.0f,
		.maxValue = rvrs.getDeviceType() == kVrDeviceTypeQuest2 ? 4.0f : 2.0f,
		.step = 1.0f,
		.width = sliderWidth,
		.height = 0.064f,
		.thickness = 0.016f,
		.panel = mainPanel,
		.sliderTexture = s_gameData.sliderTexture,
		.layoutFlags = kAlignHLeft,
	});
	labelPosY = cursorPosY - 0.050f;

	static char eyeScaleTextBuffer[6] = { 0 };
	//	strcpy_s(difficultyTextBuffer, sizeof(difficultyTextBuffer), kDIfficultTextStrings[(int32_t)(s_gameData.difficultySlider-1.0f)]);
	sprintf(eyeScaleTextBuffer, "%d%%", 100 + (int32_t)s_gameData.requestedEyeScaleSlider * 10);
	rgui.text3D(&(rgui_textDesc) {
		.x = sliderTextPosX,
		.y = labelPosY,
		.scale = 50.0f,
		.text = eyeScaleTextBuffer,
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});
	cursorPosY -= 0.128f;

	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Foveation:",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHRight | kAlignVCenter,
	});

	rgui.slider(&(rgui_sliderDesc) {
		.x = sliderLeft,
		.y = cursorPosY,
		.color = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
		.action = ^ (void) {
			if (!s_gameData.bSettingsWarningHeeded)
			{
				s_gameData.guiMode = kGUIWarning;
				s_gameData.requestedFoveationSlider = s_gameData.currentFoveationSlider;
			}
			else
			{
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
			}
		},
		.value = &s_gameData.requestedFoveationSlider,
		.minValue = rvrs.getDeviceType() == kVrDeviceTypeQuest ? 1.0f : 0.0f,
		.maxValue = 4.0f,
		.step = 1.0f,
		.width = sliderWidth,
		.height = 0.064f,
		.thickness = 0.016f,
		.panel = mainPanel,
		.sliderTexture = s_gameData.sliderTexture,
		.layoutFlags = kAlignHLeft,
	});
	labelPosY = cursorPosY - 0.050f;

	static char foveationTextBuffer[8] = { 0 };
	const char* kFoveationTextStrings[5] = {
		"Off", "Low", "Medium", "High", "Max"
	};
	//	strcpy_s(difficultyTextBuffer, sizeof(difficultyTextBuffer), kDIfficultTextStrings[(int32_t)(s_gameData.difficultySlider-1.0f)]);
	strcpy(foveationTextBuffer, kFoveationTextStrings[(int32_t)(s_gameData.requestedFoveationSlider)]);
	rgui.text3D(&(rgui_textDesc) {
		.x = sliderTextPosX,
		.y = labelPosY,
		.scale = 50.0f,
		.text = foveationTextBuffer,
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});
	cursorPosY -= 0.128f;

	{
		cursorPosY -= (0.128);
		const float kButtonWidth = 0.250f;
		const float buttonPos = 0.0f; // kPanelLeft + kPanelWidth - (0.048f + kButtonWidth * 0.5f);

		rgui.button(&(rgui_buttonDesc) {
			.x = buttonPos,
			.y = kButtonPosY,
			.width = kButtonWidth,
			.height = 0.256f,
			.panel = mainPanel,
			.texture = s_gameData.tableButtonTexture,
			.layoutFlags = kAlignHCenter,
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
				s_gameData.guiMode = kGUIOptions;
			}
		});
		rgui.text3D(&(rgui_textDesc) {
			.x = buttonPos,
			.y = kButtonPosY + 0.09f,
			.z = 2.0f,
			.scale = 64.0f,
			.text = "Back",
			.color = (rgfx_debugColor){ 0xFF9C5121 }, // kDebugColorBlue,
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});
	}
}

void showWarningPanel(void)
{
	const bool bGameInProgress = false;
	const float kPanelWidth = 1.0f;
	const float kPanelHeight = 0.55f + (s_gameData.numSupportedRefreshRates > 1.0f ? 0.128f : 0.0f) + 0.150f;
	const float kBannerHeight = 0.08f;
	const float kPanelLeft = -kPanelWidth * 0.5;
	rgui_panel mainPanel = rgui.beginPanel(&(rgui_panelDesc) {
		.transform = s_gameData.guiTransformMain,
		.color = (vec4){ 0.2f, 0.2f, 0.2f, 1.0f },
		.width = kPanelWidth,
		.height = kPanelHeight,
		.bannerColor = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
		.bannerHeight = kBannerHeight,
		.bannerText = "WARNING!",
		.bannerFont = s_gameData.font,
		.layoutFlags = kAlignHCenter,
		.flags = kCollidable,
	});

	float menuBannerPosY = kPanelHeight - kBannerHeight;

	float cursorPosX = kPanelLeft + 0.3f;
	float cursorPosY = menuBannerPosY - (kBannerHeight + (s_gameData.numSupportedRefreshRates > 1.0f ? 0.064f : 0.0f));

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Adjusting these settings can cause",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.064f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "dropped frames, so the image may",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.064f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "stutter which may cause discomfort",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.064f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = s_gameData.numSupportedRefreshRates > 1.0f ? "or nausea. You can use the FPS slider" : "or nausea. You can press 'Restore'",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.064f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "to return to a comfortable state.",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.128f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Only click OK if you understand!",
		.font = s_gameData.font,
		.color = kDebugColorRed,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.128f;
	cursorPosY -= (s_gameData.numSupportedRefreshRates > 1.0f ? 0.064f : 0.0f);
	{
		//		cursorPosY -= (0.024);
		cursorPosY -= (0.064);
		const float kButtonWidth = 0.250f;
		const float okButtonPos = kPanelLeft + (kPanelWidth * 0.5f) + kPanelWidth * 0.275f;;
		const float cancelButtonPos = kPanelLeft + (kPanelWidth * 0.5f) - kPanelWidth * 0.275f;;

		rgui.button(&(rgui_buttonDesc) {
			.x = okButtonPos,
			.y = cursorPosY,
			.width = kButtonWidth,
			.height = 0.256f,
			.panel = mainPanel,
			.texture = s_gameData.tableButtonRedTexture,
			.layoutFlags = kAlignHCenter,
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
				s_gameData.guiMode = kGUISettings;
				s_gameData.bSettingsWarningHeeded = true;
			}
		});
		rgui.text3D(&(rgui_textDesc) {
			.x = okButtonPos,
			.y = cursorPosY + 0.09f,
			.z = 2.0f,
			.scale = 64.0f,
			.text = "OK",
			.color = kDebugColorRed,
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});

		rgui.button(&(rgui_buttonDesc) {
			.x = cancelButtonPos,
			.y = cursorPosY,
			.width = kButtonWidth,
			.height = 0.256f,
			.panel = mainPanel,
			.texture = s_gameData.tableButtonBlueTexture,
			.layoutFlags = kAlignHCenter,
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
				s_gameData.guiMode = kGUISettings;
				s_gameData.bSettingsWarningHeeded = false;
			}
		});
		rgui.text3D(&(rgui_textDesc) {
			.x = cancelButtonPos,
			.y = cursorPosY + 0.09f,
			.z = 2.0f,
			.scale = 64.0f,
			.text = "Cancel",
			.color = (rgfx_debugColor){ 0xFF9C5121 }, // kDebugColorBlue,
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});

		cursorPosY -= (0.035f * 1.0f);
	}
}

void showOnlineWarningPanel(void)
{
	const bool bGameInProgress = false;
	const float kPanelWidth = 1.1f;
	const float kPanelHeight = 0.55f + (s_gameData.numSupportedRefreshRates > 1.0f ? 0.128f : 0.0f) + 0.150f;
	const float kBannerHeight = 0.08f;
	const float kPanelLeft = -kPanelWidth * 0.5;
	rgui_panel mainPanel = rgui.beginPanel(&(rgui_panelDesc) {
		.transform = s_gameData.guiTransformMain,
		.color = (vec4){ 0.2f, 0.2f, 0.2f, 1.0f },
		.width = kPanelWidth,
		.height = kPanelHeight,
		.bannerColor = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
		.bannerHeight = kBannerHeight,
		.bannerText = "WARNING!",
		.bannerFont = s_gameData.font,
		.layoutFlags = kAlignHCenter,
		.flags = kCollidable,
	});

	float menuBannerPosY = kPanelHeight - kBannerHeight;

	float cursorPosX = kPanelLeft + 0.3f;
	float cursorPosY = menuBannerPosY - (kBannerHeight + (s_gameData.numSupportedRefreshRates > 1.0f ? 0.064f : 0.0f));

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Online Play is currently 'experimental'", //"Adjusting these settings can cause",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.064f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "bugs will occur and the game may", //"dropped frames, so the image may",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.064f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "crash! By releasing early I'm hoping", //"stutter which may cause discomfort",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.064f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "you can help me find & fix the issues :)",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.096f; // 064f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Please report issues via 'Support'.",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.096f; // 128f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Click OK if you want to proceed.",
		.font = s_gameData.font,
		.color = kDebugColorRed,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.128f;
	cursorPosY -= (s_gameData.numSupportedRefreshRates > 1.0f ? 0.064f : 0.0f);
	{
		//		cursorPosY -= (0.024);
		cursorPosY -= (0.064);
		const float kButtonWidth = 0.250f;
		const float okButtonPos = kPanelLeft + (kPanelWidth * 0.5f) + kPanelWidth * 0.275f;;
		const float cancelButtonPos = kPanelLeft + (kPanelWidth * 0.5f) - kPanelWidth * 0.275f;;

		rgui.button(&(rgui_buttonDesc) {
			.x = okButtonPos,
			.y = cursorPosY,
			.width = kButtonWidth,
			.height = 0.256f,
			.panel = mainPanel,
			.texture = s_gameData.tableButtonRedTexture,
			.layoutFlags = kAlignHCenter,
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
				s_gameData.guiMode = kGUIOnline;
				s_gameData.bOnlineWarningHeeded = true;
			}
		});
		rgui.text3D(&(rgui_textDesc) {
			.x = okButtonPos,
			.y = cursorPosY + 0.09f,
			.z = 2.0f,
			.scale = 64.0f,
			.text = "OK",
			.color = kDebugColorRed,
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});

		rgui.button(&(rgui_buttonDesc) {
			.x = cancelButtonPos,
			.y = cursorPosY,
			.width = kButtonWidth,
			.height = 0.256f,
			.panel = mainPanel,
			.texture = s_gameData.tableButtonBlueTexture,
			.layoutFlags = kAlignHCenter,
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
				s_gameData.guiMode = kGUIOnline;
				s_gameData.roomTypeSlider = 0.0f;
				s_gameData.bOnlineWarningHeeded = false;
			}
		});
		rgui.text3D(&(rgui_textDesc) {
			.x = cancelButtonPos,
			.y = cursorPosY + 0.09f,
			.z = 2.0f,
			.scale = 64.0f,
			.text = "Cancel",
			.color = (rgfx_debugColor){ 0xFF9C5121 }, // kDebugColorBlue,
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});
		cursorPosY -= (0.035f * 1.0f);
	}
}

void showSupportPanel(void)
{
	const bool bGameInProgress = false;
	const float kPanelWidth = 1.08f;
	const float kPanelHeight = 0.55f + (s_gameData.numSupportedRefreshRates > 1.0f ? 0.128f : 0.0f) + 0.150f;
	const float kBannerHeight = 0.08f;
	const float kPanelLeft = -kPanelWidth * 0.5;
	rgui_panel mainPanel = rgui.beginPanel(&(rgui_panelDesc) {
		.transform = s_gameData.guiTransformMain,
		.color = (vec4){ 0.2f, 0.2f, 0.2f, 1.0f },
		.width = kPanelWidth,
		.height = kPanelHeight,
		.bannerColor = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
		.bannerHeight = kBannerHeight,
		.bannerText = "SUPPORT!",
		.bannerFont = s_gameData.font,
		.layoutFlags = kAlignHCenter,
		.flags = kCollidable,
	});

	float menuBannerPosY = kPanelHeight - kBannerHeight;

	float cursorPosX = kPanelLeft + 0.3f;
	float cursorPosY = menuBannerPosY - (kBannerHeight + 0.016f); // (s_gameData.numSupportedRefreshRates > 1.0f ? 0.064f : 0.0f));

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "If you find any bugs or other issues", //"Adjusting these settings can cause",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.064f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "or you just want to tell me what you", //"dropped frames, so the image may",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.064f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "like / don't like, you can get in touch", //"stutter which may cause discomfort",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.064f;

	rgui.text3D(&(rgui_textDesc) {
		.x = 0.0f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "in the following ways:",
		.font = s_gameData.font,
		.color = kDebugColorWhite,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	cursorPosY -= 0.096f; // 96f; // 064f;

	rgui.text3D(&(rgui_textDesc) {
		.x = -0.24f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Discord:",
		.font = s_gameData.font,
		.color = (rgfx_debugColor){ 0xFF9C5121 },
		.panel = mainPanel,
		.layoutFlags = kAlignHRight | kAlignVCenter,
	});

	rgui.text3D(&(rgui_textDesc) {
		.x = -0.2f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "discord.gg/rpC95Dgtdd",
		.font = s_gameData.font,
		.color = kDebugColorRed,
		.panel = mainPanel,
		.layoutFlags = kAlignHLeft | kAlignVCenter,
	});

	cursorPosY -= 0.080f; // 64f; // 128f;

	rgui.text3D(&(rgui_textDesc) {
		.x = -0.24f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "EMail:",
		.font = s_gameData.font,
		.color = (rgfx_debugColor){ 0xFF9C5121 },
		.panel = mainPanel,
		.layoutFlags = kAlignHRight | kAlignVCenter,
	});

	rgui.text3D(&(rgui_textDesc) {
		.x = -0.2f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "support@guiltydog.co.uk",
		.font = s_gameData.font,
		.color = kDebugColorRed,
		.panel = mainPanel,
		.layoutFlags = kAlignHLeft | kAlignVCenter,
	});

	cursorPosY -= 0.128f;
	cursorPosY -= (s_gameData.numSupportedRefreshRates > 1.0f ? 0.064f : 0.0f);
	{
		//		cursorPosY -= (0.024);
		cursorPosY -= (0.064);
		const float kButtonWidth = 0.250f;
		const float okButtonPos = kPanelLeft + (kPanelWidth * 0.5f) + kPanelWidth * 0.25f;;
		const float cancelButtonPos = kPanelLeft + (kPanelWidth * 0.5f) - kPanelWidth * 0.25f;;

		rgui.button(&(rgui_buttonDesc) {
			.x = okButtonPos,
			.y = cursorPosY,
			.width = kButtonWidth,
			.height = 0.256f,
			.panel = mainPanel,
			.texture = s_gameData.tableButtonRedTexture,
			.layoutFlags = kAlignHCenter,
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
				s_gameData.guiMode = kGUICredits;
			}
		});
		rgui.text3D(&(rgui_textDesc) {
			.x = okButtonPos,
			.y = cursorPosY + 0.09f,
			.z = 2.0f,
			.scale = 64.0f,
			.text = "Credits",
			.color = (rgfx_debugColor){ 0xFF9C5121 }, // kDebugColorBlue,
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});

		rgui.button(&(rgui_buttonDesc) {
			.x = cancelButtonPos,
			.y = cursorPosY,
			.width = kButtonWidth,
			.height = 0.256f,
			.panel = mainPanel,
			.texture = s_gameData.tableButtonBlueTexture,
			.layoutFlags = kAlignHCenter,
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
				s_gameData.guiMode = kGUIOptions;
			}
		});
		rgui.text3D(&(rgui_textDesc) {
			.x = cancelButtonPos,
			.y = cursorPosY + 0.09f,
			.z = 2.0f,
			.scale = 64.0f,
			.text = "Back",
			.color = (rgfx_debugColor){ 0xFF9C5121 }, // kDebugColorBlue,
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});
		cursorPosY -= (0.035f * 1.0f);
	}
}

void showCreditsPanel(void)
{
	const float kButtonLayerHeight = 0.150f;
	const float kButtonPosY = -0.036f;									//CLR - Y position that buttons are drawn at.
	const float kPanelWidth = 1.5f;
	const float kPanelHeight = 0.678f + kButtonLayerHeight;
	const float kBannerHeight = 0.08f;
	const float kPanelLeft = -kPanelWidth * 0.5;
	rgui_panel mainPanel = rgui.beginPanel(&(rgui_panelDesc) {
		.transform = s_gameData.guiTransformMain,
		.color = (vec4){ 0.2f, 0.2f, 0.2f, 1.0f },
		.width = kPanelWidth,
		.height = kPanelHeight,
		.bannerColor = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
		.bannerHeight = kBannerHeight,
		.bannerText = "CREDITS",
		.bannerFont = s_gameData.font,
		.layoutFlags = kAlignHCenter,
		.flags = kCollidable,
	});

	float menuBannerPosY = kPanelHeight - kBannerHeight;

	float cursorPosX = kPanelLeft + kPanelWidth * 0.5f; // .3f;
	float cursorPosY = menuBannerPosY - (kBannerHeight + 0.064f);

	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX-0.008f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Design & Programming:",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHRight | kAlignVCenter,
	});
	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX+0.008f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Claire Rogers",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHLeft | kAlignVCenter,
	});
	cursorPosY -= (0.072f * 1.0f);

	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX-0.008f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Art:",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHRight | kAlignVCenter,
	});
	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX+0.008f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Nicola Cavala",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHLeft | kAlignVCenter,
	});
	cursorPosY -= (0.072f * 1.0f);
	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX + 0.008f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Christian Lavoie",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHLeft | kAlignVCenter,
	});
	cursorPosY -= (0.072f * 1.0f);
	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX + 0.008f,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Claire Rogers",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHLeft | kAlignVCenter,
	});
	cursorPosY -= (0.096f * 1.0f);

	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX,
		.y = cursorPosY,
		.scale = 56.0f,
		.text = "Made with FMOD Studio by Firelight Technologies Pty Ltd.",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});
	cursorPosY -= (0.104f * 1.0f);

	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX,
		.y = cursorPosY,
		.scale = 56.0f,
		.text = "Sound content by AudioSparx",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});
	cursorPosY -= (0.132f * 1.0f);

	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX,
		.y = cursorPosY,
		.scale = 56.0f,
		.text = "(c) Guilty Dog Productions Ltd. 2021",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});

	{
		cursorPosY -= (0.024);
		cursorPosY -= (0.064);
		const float kButtonWidth = 0.250f;
		const float buttonPos = kPanelLeft + kPanelWidth - (0.048f + kButtonWidth * 0.5f);
		rgui.button(&(rgui_buttonDesc) {
			.x = buttonPos,
			.y = kButtonPosY,
			.width = kButtonWidth,
			.height = 0.256f,
			.panel = mainPanel,
			.texture = s_gameData.tableButtonTexture,
			.layoutFlags = kAlignHCenter,
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
				s_gameData.guiMode = kGUIOptions;
			}
		});
		rgui.text3D(&(rgui_textDesc) {
			.x = buttonPos,
			.y = kButtonPosY + 0.09f,
			.z = 2.0f,
			.scale = 64.0f,
			.text = "Back",
			.color = (rgfx_debugColor){ 0xFF9C5121 }, // kDebugColorBlue,
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});
	}
}

void showOnlinePanel(void)
{
	const float kButtonLayerHeight = 0.150f;
	const float kButtonPosY = -0.036f;									//CLR - Y position that buttons are drawn at.
	const float kPanelWidth = 1.0f;
	const float kPanelHeight = 0.678f + kButtonLayerHeight; // 0.55f + 0.16f;
	const float kBannerHeight = 0.08f;
	const float kPanelLeft = -kPanelWidth * 0.5;
	rgui_panel mainPanel = rgui.beginPanel(&(rgui_panelDesc) {
		.transform = s_gameData.guiTransformMain,
		.color = (vec4){ 0.2f, 0.2f, 0.2f, 1.0f },
		.width = kPanelWidth,
		.height = kPanelHeight,
		.bannerColor = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
		.bannerHeight = kBannerHeight,
		.bannerText = "ONLINE SETTINGS",
		.bannerFont = s_gameData.font,
		.layoutFlags = kAlignHCenter,
		.flags = kCollidable,
	});

	float menuBannerPosY = kPanelHeight - kBannerHeight;

	float cursorPosX = kPanelLeft + 0.4f;
	float cursorPosY = menuBannerPosY - (kBannerHeight + 0.064f);

	rgui.text3D(&(rgui_textDesc) {
		.x = cursorPosX,
		.y = cursorPosY,
		.scale = 64.0f,
		.text = "Room Type:",
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHRight | kAlignVCenter,
	});

	float sliderLeft = cursorPosX + 0.064f;
	float sliderRight = kPanelLeft + kPanelWidth - 0.064;
	float sliderWidth = sliderRight - sliderLeft;
	float sliderTextPosX = (sliderLeft + sliderWidth * 0.5f);

	if (s_gameData.roomTypeSlider > 0.0f && s_gameData.roomData.state == kRSSolo)
	{
		assert(s_gameData.bOnlineWarningHeeded == true);
		createPrivateRoom(2);
	}
	rgui.slider(&(rgui_sliderDesc) {
		.x = sliderLeft,
		.y = cursorPosY,
		.color = (vec4){ 0.15f, 0.15f, 0.15f, 1.0f },
		.action = ^ (void) {
			raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
			if (s_gameData.roomTypeSlider > 0.0f)
			{
				if (!s_gameData.bOnlineWarningHeeded)
				{
					s_gameData.guiMode = kGUIOnlineWarning;
				}
			}
			else
			{
				s_gameData.roomData.state = kRSSolo;
				s_gameData.playerCount = 1;
				s_gameData.roomPanelUserListScrollPos = 0;
				//CLR - Note: I was clearing up the friends list here but it could cause a crash because friend names were in the process of being displayed and the memory deallocated 
				//		in the process. To fix this, I defer the deallocation until after processing of the RoomType slider.  See below!
			}
		},
		.value = &s_gameData.roomTypeSlider,
		.minValue = 0.0f,
		.maxValue = 1.0f,
		.step = 1.0f,
		.width = sliderWidth,
		.height = 0.064f,
		.thickness = 0.016f,
		.panel = mainPanel,
		.sliderTexture = s_gameData.sliderTexture,
		.layoutFlags = kAlignHLeft,
	});

	float labelPosY = cursorPosY - 0.050f;

	static char multiplayerTextBuffer[32] = { 0 };
	const char* kMultiplayerTextStrings[2] = {
		"Solo", "Private"
	};
	strcpy(multiplayerTextBuffer, kMultiplayerTextStrings[(int32_t)(s_gameData.roomTypeSlider)]);
	rgui.text3D(&(rgui_textDesc) {
		.x = sliderTextPosX,
		.y = labelPosY,
		.scale = 50.0f,
		.text = multiplayerTextBuffer,
		.font = s_gameData.font,
		.panel = mainPanel,
		.layoutFlags = kAlignHCenter | kAlignVCenter,
	});
	cursorPosY -= 0.128f;

	const int32_t kMaxDisplayableFriends = 4;

	switch (s_gameData.roomData.state)
	{
	case kRSSolo:
		cursorPosY -= 0.244f;
		break;
	case kRSCreatedAwaitingFriendsList:
		rgui.text3D(&(rgui_textDesc) {
			.x = 0.0f,
			.y = cursorPosY,
			.scale = 50.0f,
			.text = "Please wait! Retrieving Friends List",
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});
		cursorPosY -= 0.244f;
		break;
	case kRSCreatedWithFriendsList:
		rgui.text3D(&(rgui_textDesc) {
			.x = kPanelLeft + 0.090f,
			.y = cursorPosY,
			.scale = 64.0f,
			.text = "Friends:",
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHLeft | kAlignVCenter,
		});
		cursorPosY -= 0.080f;

		if (s_gameData.roomData.numFriends > kMaxDisplayableFriends && s_gameData.roomPanelUserListScrollPos > 0)
		{
			rgui.button(&(rgui_buttonDesc) {
				.x = 0.0f,
				.y = cursorPosY + 0.04f,
				.width = 0.064f,
				.height = 0.064f,
				.panel = mainPanel,
				.texture = s_gameData.scrollUpButtonTexture,
				.layoutFlags = kAlignHCenter,
				.action = ^ (void) {
					raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
					s_gameData.roomPanelUserListScrollPos--;
				}
			});
		}
		int32_t displayFriendCount = s_gameData.roomData.numFriends <= kMaxDisplayableFriends ? s_gameData.roomData.numFriends : kMaxDisplayableFriends;
		for (int32_t i = 0; i < displayFriendCount; i++)
		{
			int32_t friendIndex = i + s_gameData.roomPanelUserListScrollPos;
			assert(friendIndex >= 0);
			assert(friendIndex < s_gameData.roomData.numFriends);
			rgui.text3D(&(rgui_textDesc) {
				.x = kPanelLeft + 0.128f,
				.y = cursorPosY,
				.scale = 50.0f,
				.text = &s_gameData.roomData.userNameBuffer[s_gameData.roomData.friendsList[friendIndex].userNameBufferIndex],
				.font = s_gameData.font,
				.color = s_gameData.roomData.friendsList[friendIndex].flags & kUserOnline ? kDebugColorGreen : kDebugColorRed,
				.panel = mainPanel,
				.layoutFlags = kAlignHLeft | kAlignVCenter,
			});

			const float kBigButtonWidth = 0.250f;
			const float kButtonWidth = 0.160f;
			const float buttonPos = kPanelLeft + kPanelWidth - (0.048f + kButtonWidth * 0.5f);
			bool bInvited = s_gameData.roomData.friendsList[friendIndex].flags & kUserInvited;
			if (bInvited && s_gameData.roomData.friendsList[friendIndex].inviteTime > 0)
			{
				int64_t currentTime = time(NULL);
				int64_t inviteTime = s_gameData.roomData.friendsList[friendIndex].inviteTime;
				if ((currentTime - inviteTime) > kInviteTimeout) {
					s_gameData.roomData.friendsList[friendIndex].flags &= ~kUserInvited;
					s_gameData.roomData.friendsList[friendIndex].inviteTime = 0;
					bInvited = false;
				}
			}
			rgui.button(&(rgui_buttonDesc) {
				.x = buttonPos,
				.y = cursorPosY - 0.032f,
				.width = kButtonWidth,
				.height = 0.128f,
				.panel = mainPanel,
				.texture = s_gameData.tableButtonTexture,
				.layoutFlags = kAlignHCenter | kAlignVTop,
				.action = ^ (void) {
					raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
					rvrs.inviteUserOvr(&(rvrs_ovrInviteUserDesc) {
						.roomID = s_gameData.roomData.roomID,
						.inviteToken = s_gameData.roomData.friendsList[friendIndex].inviteToken
					});
					s_gameData.roomData.friendsList[friendIndex].flags |= kUserInvited;
					s_gameData.roomData.friendsList[friendIndex].inviteTime = time(NULL);
				},
				.bDisabled = bInvited,
			});
			rgui.text3D(&(rgui_textDesc) {
				.x = buttonPos,
				.y = cursorPosY + 0.004f,
				.z = 2.0f,
				.scale = 50.0f,
				.text = "Invite",
				.color = bInvited == false ? (rgfx_debugColor){ 0xFF9C5121 } : kDebugColorDarkGrey,
				.font = s_gameData.font,
				.panel = mainPanel,
				.layoutFlags = kAlignHCenter | kAlignVCenter,
			});
			if (s_gameData.roomData.friendsList[friendIndex].flags & kUserInviteReceived)
			{
				rgui.button(&(rgui_buttonDesc) {
					.x = buttonPos - 0.18f,
					.y = cursorPosY - 0.032f,
					.width = kButtonWidth,
					.height = 0.128f,
					.panel = mainPanel,
					.texture = s_gameData.tableButtonTexture,
					.layoutFlags = kAlignHCenter | kAlignVTop,
					.action = ^ (void) {
						raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
						rvrs.joinRoomOvr(s_gameData.roomData.friendsList[friendIndex].roomId);
					}
				});
				rgui.text3D(&(rgui_textDesc) {
					.x = buttonPos - 0.18f,
					.y = cursorPosY + 0.004f,
					.z = 2.0f,
					.scale = 50.0f,
					.text = "Join",
					.color = (rgfx_debugColor){ 0xFF9C5121 },
					.font = s_gameData.font,
					.panel = mainPanel,
					.layoutFlags = kAlignHCenter | kAlignVCenter,
				});
			}
			cursorPosY -= 0.064f;
		}
		if (s_gameData.roomData.numFriends > kMaxDisplayableFriends && (s_gameData.roomPanelUserListScrollPos + kMaxDisplayableFriends) < s_gameData.roomData.numFriends)
		{
			rgui.button(&(rgui_buttonDesc) {
				.x = 0.0f,
				.y = cursorPosY, // +0.04f,
				.width = 0.064f,
				.height = 0.064f,
				.panel = mainPanel,
				.texture = s_gameData.scrollDownButtonTexture,
				.layoutFlags = kAlignHCenter,
				.action = ^ (void) {
					raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
					s_gameData.roomPanelUserListScrollPos++;
				}
			});
		}
		cursorPosY -= (0.036f);
		break;
	case kRSCreatedNetRunning:
		rgui.text3D(&(rgui_textDesc) {
			.x = kPanelLeft + 0.090f,
			.y = cursorPosY,
			.scale = 64.0f,
			.text = "Players:",
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHLeft | kAlignVCenter,
		});
		cursorPosY -= 0.080f;

		if (s_gameData.roomData.numFriends > kMaxDisplayableFriends && s_gameData.roomPanelUserListScrollPos > 0)
		{
			rgui.button(&(rgui_buttonDesc) {
				.x = 0.0f,
				.y = cursorPosY + 0.04f,
				.width = 0.064f,
				.height = 0.064f,
				.panel = mainPanel,
				.texture = s_gameData.scrollUpButtonTexture,
				.layoutFlags = kAlignHCenter,
				.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
					s_gameData.roomPanelUserListScrollPos--;
				}
			});
		}
		int32_t numActiveUsers = sb_count(s_gameData.roomData.userArray);
		int32_t displayUserCount = numActiveUsers <= kMaxDisplayableFriends ? numActiveUsers : kMaxDisplayableFriends;
		for (int32_t i = 0; i < displayUserCount; i++)
		{
			int32_t friendIndex = i + s_gameData.roomPanelUserListScrollPos;
			assert(friendIndex >= 0);
			assert(friendIndex < numActiveUsers);
// CLR - Uncomment the line below to not display us in Players list.
//			if (s_gameData.roomData.userArray[friendIndex].userId != s_gameData.userId)
			{
				rgui.text3D(&(rgui_textDesc) {
					.x = kPanelLeft + 0.128f,
					.y = cursorPosY,
					.scale = 50.0f,
					.text = &s_gameData.roomData.userNameBuffer[s_gameData.roomData.userArray[friendIndex].userNameBufferIndex],
					.font = s_gameData.font,
					.panel = mainPanel,
					.layoutFlags = kAlignHLeft | kAlignVCenter,
				});

				const float kBigButtonWidth = 0.250f;
				const float kButtonWidth = 0.160f;
				const float buttonPos = kPanelLeft + kPanelWidth - (0.048f + kButtonWidth * 0.5f);
				if (s_gameData.userId == s_gameData.roomData.roomOwner && s_gameData.roomData.userArray[friendIndex].userId != s_gameData.userId)
				{
					rgui.button(&(rgui_buttonDesc) {
						.x = buttonPos - 0.18f,
						.y = cursorPosY - 0.032f,
						.width = kButtonWidth,
						.height = 0.128f,
						.panel = mainPanel,
						.texture = s_gameData.tableButtonTexture,
						.layoutFlags = kAlignHCenter | kAlignVTop,
						.action = ^ (void) {
							raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
							rvrs.kickUserOvr(s_gameData.roomData.roomID, s_gameData.roomData.userArray[friendIndex].userId);
						}
					});
					rgui.text3D(&(rgui_textDesc) {
						.x = buttonPos - 0.18f,
						.y = cursorPosY + 0.004f,
						.z = 2.0f,
						.scale = 50.0f,
						.text = "Kick",
						.color = (rgfx_debugColor){ 0xFF9C5121 },
						.font = s_gameData.font,
						.panel = mainPanel,
						.layoutFlags = kAlignHCenter | kAlignVCenter,
					});
				}
				else if (s_gameData.roomData.userArray[friendIndex].userId == s_gameData.userId)
				{
					rgui.button(&(rgui_buttonDesc) {
						.x = buttonPos - 0.18f,
						.y = cursorPosY - 0.032f,
						.width = kButtonWidth,
						.height = 0.128f,
						.panel = mainPanel,
						.texture = s_gameData.tableButtonTexture,
						.layoutFlags = kAlignHCenter | kAlignVTop,
						.action = ^ (void) {
							raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
							rvrs.leaveRoomOvr(s_gameData.roomData.roomID);
						}
					});
					rgui.text3D(&(rgui_textDesc) {
						.x = buttonPos - 0.18f,
						.y = cursorPosY + 0.004f,
						.z = 2.0f,
						.scale = 50.0f,
						.text = "Leave",
						.color = (rgfx_debugColor){ 0xFF9C5121 },
						.font = s_gameData.font,
						.panel = mainPanel,
						.layoutFlags = kAlignHCenter | kAlignVCenter,
					});
				}
				{
					rgui.button(&(rgui_buttonDesc) {
						.x = buttonPos /* - 0.18f*/,
						.y = cursorPosY - 0.032f,
						.width = kButtonWidth,
						.height = 0.128f,
						.panel = mainPanel,
						.texture = s_gameData.tableButtonTexture,
						.layoutFlags = kAlignHCenter | kAlignVTop,
						.action = ^ (void) {
							raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
							s_gameData.roomData.userArray[friendIndex].flags ^= kUserMuted;
							if (s_gameData.roomData.userArray[friendIndex].userId == s_gameData.userId)
							{
								rvrs.voipMute(s_gameData.roomData.userArray[friendIndex].flags & kUserMuted);
							}
						}
					});
					rgui.text3D(&(rgui_textDesc) {
						.x = buttonPos /* - 0.18f*/,
						.y = cursorPosY + 0.004f,
						.z = 2.0f,
						.scale = 50.0f,
						.text = "Mute",
						.color = s_gameData.roomData.userArray[friendIndex].flags & kUserMuted ? kDebugColorRed : (rgfx_debugColor){ 0xFF9C5121 },
						.font = s_gameData.font,
						.panel = mainPanel,
						.layoutFlags = kAlignHCenter | kAlignVCenter,
					});
				}
				cursorPosY -= 0.064f;
			}
		}
		if (s_gameData.roomData.numFriends > kMaxDisplayableFriends && (s_gameData.roomPanelUserListScrollPos + kMaxDisplayableFriends) < s_gameData.roomData.numFriends)
		{
			rgui.button(&(rgui_buttonDesc) {
				.x = 0.0f,
				.y = cursorPosY, // +0.04f,
				.width = 0.064f,
				.height = 0.064f,
				.panel = mainPanel,
				.texture = s_gameData.scrollDownButtonTexture,
				.layoutFlags = kAlignHCenter,
				.action = ^ (void) {
					raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
					s_gameData.roomPanelUserListScrollPos++;
				}
			});
		}
		cursorPosY -= (0.036f);
		break;
		case kRSDestroyRoom:
			destroyRoom();
			break;
	}

	{
		cursorPosY -= (0.148);
		const float kButtonWidth = 0.250f;
		const float buttonPos = 0.0f;

		rgui.button(&(rgui_buttonDesc) {
			.x = buttonPos,
			.y = kButtonPosY,
			.width = kButtonWidth,
			.height = 0.256f,
			.panel = mainPanel,
			.texture = s_gameData.tableButtonTexture,
			.layoutFlags = kAlignHCenter,
			.action = ^ (void) {
				raud.playSound(s_gameData.guiClickSound, (float*)&s_gameData.guiTransformMain.wAxis, NULL, kVolumeScale, 128);
				s_gameData.guiMode = kGUIOptions;
			}
		});
		rgui.text3D(&(rgui_textDesc) {
			.x = buttonPos,
			.y = kButtonPosY + 0.09f,
			.z = 2.0f,
			.scale = 64.0f,
			.text = "Back",
			.color = (rgfx_debugColor){ 0xFF9C5121 },
			.font = s_gameData.font,
			.panel = mainPanel,
			.layoutFlags = kAlignHCenter | kAlignVCenter,
		});
	}
}

void createPrivateRoom(int32_t maxUsers)
{
	s_gameData.roomData.state = kRSCreatingRoom;						// CLR - Immediately change room state, so we don't re-enter on the next game loop.
	rvrs.createPrivateRoomOvr(&(rvrs_ovrCreateRoomDesc) {
		.maxUsers = maxUsers,
		.roomCallback = ^ (const rvrs_roomCallbackData * callbackData) {
			if (callbackData == NULL)
				return;
			switch (callbackData->type)
			{
			case kRoomCallbackCreate:
				assert(s_gameData.roomData.state == kRSCreatingRoom);
				s_gameData.roomData.roomID = callbackData->roomID;
				s_gameData.roomData.state = kRSCreatedAwaitingFriendsList;
				s_gameData.roomData.numFriends = 0;
				s_gameData.roomData.friendsList = NULL;
				break;
			case kRoomCallbackUpdateFriends:
			{
				size_t numFriends = callbackData->numInvitableUsers;
				rvrs_userOvr* friendsList = callbackData->invitableUsers;
				buildFriendsList(numFriends, friendsList);
				break;
			}
			case kRoomCallbackInviteReceived:
			{
				int32_t numFriends = s_gameData.roomData.numFriends;
				for (int32_t i = 0; i < (int32_t)numFriends; i++)
				{
					if (s_gameData.roomData.friendsList[i].userId == callbackData->inviteFromUserID)
					{
						s_gameData.roomData.friendsList[i].roomId = callbackData->roomID;
						s_gameData.roomData.friendsList[i].flags |= kUserInviteReceived;
						break;
					}
				}
				break;
			}
			case kRoomCallbackJoinRoom:
			case kRoomCallbackUpdateRoom:
			case kRoomCallbackUserKicked:
			case kRoomCallbackUserLeft:
			{
				uint64_t roomId = callbackData->roomID;
				size_t numUsers = callbackData->numJoinedUsers;
				rvrs_userOvr* userList = callbackData->joinedUsers;
				roomUpdateMemberList(roomId, numUsers, userList, callbackData->type);
				break;
			}
			default:
				assert(0);
			}
		}
	});
}

void destroyRoom(void)
{
	int32_t numActiveUsers = sb_count(s_gameData.roomData.userArray);
	for (int32_t i = 0; i < numActiveUsers; i++)
	{
		if (s_gameData.roomData.userArray[i].userId != s_gameData.userId) 
		{
#ifndef DISABLE_VOIP
			if (s_gameData.roomData.userArray[i].voipStream != NULL)
			{
				raud.stopStream(s_gameData.roomData.userArray[i].voipStream);
				rvrs.voipStop(s_gameData.roomData.userArray[i].userId);
				s_gameData.roomData.userArray[i].voipStream = NULL;
			}
#endif
			rvrs.netClose(s_gameData.roomData.userArray[i].userId);
		}
	}
	rvrs.netClose(0);
	rvrs.destroyAvatarOvr(kOpponentAvatarSlot);					// WIP - Destroy avatar in Opponent avatar slot (a little bit hacky).
	s_gameData.roomData.numFriends = 0;
	free(s_gameData.roomData.inviteTokenBuffer);
	s_gameData.roomData.inviteTokenBuffer = NULL;
	free(s_gameData.roomData.userNameBuffer);
	s_gameData.roomData.userNameBuffer = NULL;
	free(s_gameData.roomData.friendsList);
	s_gameData.roomData.friendsList = NULL;
	rsys__hmap_free(&s_gameData.roomData.friendLookup);
	sb_free(s_gameData.roomData.userArray);
	s_gameData.roomData.userArray = NULL;
	rsys__hmap_free(&s_gameData.roomData.userLookup);
	s_gameData.roomData.userNameBufferSize = 0;

	s_gameData.roomData.state = kRSSolo;
	s_gameData.playerCount = 1;
}

void buildFriendsList(int32_t numFriends, rvrs_userOvr* friendsList)
{
	s_gameData.roomData.state = kRSCreatedWithFriendsList;
	s_gameData.roomData.numFriends = numFriends;
	s_gameData.roomData.friendsList = (rvrs_userOvr*)realloc(s_gameData.roomData.friendsList, sizeof(rvrs_userOvr) * numFriends);
	memset(s_gameData.roomData.friendsList, 0, sizeof(rvrs_userOvr) * numFriends);
	size_t nameBufferSize = 0;
	size_t inviteBufferSize = 0;
	for (int32_t i = 0; i < (int32_t)numFriends; i++)
	{
		nameBufferSize += (strlen(friendsList[i].userName) + 1);
		inviteBufferSize += (strlen(friendsList[i].inviteToken) + 1);
	}
	s_gameData.roomData.userNameBuffer = (char*)realloc(s_gameData.roomData.userNameBuffer, nameBufferSize);
	s_gameData.roomData.inviteTokenBuffer = (char*)realloc(s_gameData.roomData.inviteTokenBuffer, inviteBufferSize);
	s_gameData.roomData.userNameBufferSize = nameBufferSize;
	char* nameBufferPtr = s_gameData.roomData.userNameBuffer;
	char* inviteBufferPtr = s_gameData.roomData.inviteTokenBuffer;
	for (int32_t i = 0; i < (int32_t)numFriends; i++)
	{
		uint64_t userId = friendsList[i].userId;
		uint32_t hashId = hashData(&userId, sizeof(userId));
		rsys_hm_insert(&s_gameData.roomData.friendLookup, hashId, i);
		size_t userNameLen = strlen(friendsList[i].userName);
		strcpy(nameBufferPtr, friendsList[i].userName);
		s_gameData.roomData.friendsList[i].userId = friendsList[i].userId;
		s_gameData.roomData.friendsList[i].hashId = hashId;
		s_gameData.roomData.friendsList[i].flags = friendsList[i].flags;
		s_gameData.roomData.friendsList[i].userNameBufferIndex = nameBufferPtr - s_gameData.roomData.userNameBuffer;
		nameBufferPtr += (userNameLen + 1);
		size_t inviteTokenLen = strlen(friendsList[i].inviteToken);
		strcpy(inviteBufferPtr, friendsList[i].inviteToken);
		s_gameData.roomData.friendsList[i].inviteToken = inviteBufferPtr;
		inviteBufferPtr += (inviteTokenLen + 1);
	}
}

void roomUpdateMemberList(uint64_t roomId, int32_t numUsers, rvrs_userOvr* userList, rvrp_roomCallbackType type)
{
	int32_t currentUserCount = MAX(sb_count(s_gameData.roomData.userArray), 1);
	if (numUsers > currentUserCount)
	{
		if (type == kRoomCallbackJoinRoom && roomId != s_gameData.roomData.roomID) {
			s_gameData.roomData.roomID = roomId;
		}
		size_t nameBufferSize = 0;
		for (int32_t i = 0; i < (int32_t)numUsers; i++)
		{
			uint64_t userId = userList[i].userId;
			//								if (userId != s_gameData.userId)										// Don't include ourself.
			{
				uint32_t hashId = hashData(&userId, sizeof(userId));
				int32_t friendIndex = rsys_hm_find(&s_gameData.roomData.friendLookup, hashId);
				if (friendIndex < 0)
				{
					nameBufferSize += (strlen(userList[i].userName) + 1);			// Calc amount to increase nameBuffer by fir users not already in your friends list.
				}
			}
		}
		int32_t totalNameBufferSize = s_gameData.roomData.userNameBufferSize;
		if (nameBufferSize > 0)
		{
			totalNameBufferSize += nameBufferSize;									// Calc total new buffer size.
			s_gameData.roomData.userNameBuffer = (char*)realloc(s_gameData.roomData.userNameBuffer, totalNameBufferSize);
		}
		char* nameBufferPtr = s_gameData.roomData.userNameBuffer + s_gameData.roomData.userNameBufferSize;
		s_gameData.roomData.userNameBufferSize = totalNameBufferSize;				// Store new name buffer size.
		for (int32_t i = 0; i < (int32_t)numUsers; i++)
		{
			uint64_t userId = userList[i].userId;
			uint32_t hashId = hashData(&userId, sizeof(userId));
			int32_t friendIndex = rsys_hm_find(&s_gameData.roomData.friendLookup, hashId);
			rvrs_userOvr newUser = { 0 };
			if (friendIndex >= 0)
			{
				newUser = s_gameData.roomData.friendsList[friendIndex];
				newUser.flags = userList[i].flags;
			}
			else
			{
				size_t userNameLen = strlen(userList[i].userName);
				strcpy(nameBufferPtr, userList[i].userName);
				const char* userName = nameBufferPtr;
				nameBufferPtr += (userNameLen + 1);

				newUser.userId = userList[i].userId;
				newUser.userNameBufferIndex = userName - s_gameData.roomData.userNameBuffer;
				newUser.hashId = hashId;
				newUser.flags = userList[i].flags;
			}
			int32_t index = rsys_hm_find(&s_gameData.roomData.userLookup, hashId);
			if (index < 0)
			{
				//									rvrs_userOvr newUser = {
				//										.userId = userList[i].userId,
				//										.userName = userName,
				//										.hashId = hashId,
				//										.flags = userList[i].flags,
				//									};
				int32_t pushIndex = sb_count(s_gameData.roomData.userArray);
				sb_push(s_gameData.roomData.userArray, newUser);
				rsys_hm_insert(&s_gameData.roomData.userLookup, hashId, pushIndex);
				rsys.print("User %" PRId64 ": added to slot %d\n", userId, pushIndex);
				if (userList[i].flags &= kUserHost) {
					s_gameData.roomData.roomOwner = userId;
				}
				if (userId != s_gameData.userId)
				{
					rvrs.requestAvatarSpecOvr(userId, kOpponentAvatarSlot);
					s_gameData.playerCount++;
					//										s_gameData.guiMode = kGUINone;			//CLR - Temp, when enabled this closes the menu when a player joins.
					netConnect(userId);
#ifndef DISABLE_VOIP
					voipConnect(userId);
#endif
				}
				else
				{
					int32_t prevPlayerIndex = s_gameData.playerIndex;
					s_gameData.playerIndex = s_gameData.roomData.userArray[i].flags & kUserHost ? 0 : 1;
					if (s_gameData.playerIndex != prevPlayerIndex)
					{
						s_gameData.guiTransformMain.wAxis = (vec4){ 0.0f, 0.0f, 0.0f, 0.0f };	// Player position has changed Force GUI transform reset.
					}
				}
			}
		}
	}
	else if (numUsers < currentUserCount)
	{
		//CLR - Do proper delete user BUT.... as we currently only handle two player rooms, we can delete the room here.
		s_gameData.roomData.state = kRSDestroyRoom;
	}
}
