#pragma once

#include "math/mat4x4.h"
#include "math/quat.h"
#include "hash.h"
#include "rgfx/renderer.h"
#include "rvrs_ovr_platform/rvrs_platform_ovr.h"

typedef enum rvrs_deviceType
{
	kVrDeviceTypeUnknown = 0,
	kVrDeviceTypeQuest,
	kVrDeviceTypeQuest2,
	kVrDeviceTypeRift,
} rvrs_deviceType;

typedef enum rvrs_textureType
{
	kVrTextureType2D = 0,	//< 2D textures.
	kVrTextureType2DArray = 2,	//< Texture array.
	kVrTextureType2DCube = 3,	//< Cube maps.
	kVrTextureTypeMax = 4,
} rvrs_textureType;

#define MAX_SUPPORTED_REFRESH_RATES 6

#ifdef VRSYSTEM_OCULUS_WIN
#ifdef NDEBUG
#define RENDER_DIRECT_TO_SWAPCHAIN
#endif
#include "OVR_CAPI_GL.h"
//#include "OVR_M"
//#include "OVR_platform.h"

#include <GL/glew.h>
//#include <GL/wglew.h>
#include <GL/gl.h>
//#include <GLFW/glfw3.h>

typedef struct OculusTextureBuffer
{
	ovrSession          Session;
	ovrTextureSwapChain ColorTextureChain;
	ovrTextureSwapChain DepthTextureChain;
	GLuint              fboId;
	//	OVR::Sizei          texSize;
	/*
		OculusTextureBuffer(ovrSession session, OVR::Sizei size, int sampleCount) :
			Session(session),
			ColorTextureChain(nullptr),
			DepthTextureChain(nullptr),
			fboId(0),
			texSize(0, 0)
		{
			assert(sampleCount <= 1); // The code doesn't currently handle MSAA textures.

			texSize = size;

			// This texture isn't necessarily going to be a rendertarget, but it usually is.
			assert(session); // No HMD? A little odd.

			ovrTextureSwapChainDesc desc = {};
			desc.Type = ovrTexture_2D;
			desc.ArraySize = 1;
			desc.Width = size.w;
			desc.Height = size.h;
			desc.MipLevels = 1;
			desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
			desc.SampleCount = sampleCount;
			desc.StaticImage = ovrFalse;

			{
				ovrResult result = ovr_CreateTextureSwapChainGL(Session, &desc, &ColorTextureChain);

				int length = 0;
				ovr_GetTextureSwapChainLength(session, ColorTextureChain, &length);

				if (OVR_SUCCESS(result))
				{
					for (int i = 0; i < length; ++i)
					{
						GLuint chainTexId;
						ovr_GetTextureSwapChainBufferGL(Session, ColorTextureChain, i, &chainTexId);
						glBindTexture(GL_TEXTURE_2D, chainTexId);

						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					}
				}
			}

			desc.Format = OVR_FORMAT_D32_FLOAT;

			{
				ovrResult result = ovr_CreateTextureSwapChainGL(Session, &desc, &DepthTextureChain);

				int length = 0;
				ovr_GetTextureSwapChainLength(session, DepthTextureChain, &length);

				if (OVR_SUCCESS(result))
				{
					for (int i = 0; i < length; ++i)
					{
						GLuint chainTexId;
						ovr_GetTextureSwapChainBufferGL(Session, DepthTextureChain, i, &chainTexId);
						glBindTexture(GL_TEXTURE_2D, chainTexId);

						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					}
				}
			}

			glGenFramebuffers(1, &fboId);
		}

		~OculusTextureBuffer()
		{
			if (ColorTextureChain)
			{
				ovr_DestroyTextureSwapChain(Session, ColorTextureChain);
				ColorTextureChain = nullptr;
			}
			if (DepthTextureChain)
			{
				ovr_DestroyTextureSwapChain(Session, DepthTextureChain);
				DepthTextureChain = nullptr;
			}
			if (fboId)
			{
				glDeleteFramebuffers(1, &fboId);
				fboId = 0;
			}
		}

		OVR::Sizei GetSize() const
		{
			return texSize;
		}

		void SetAndClearRenderSurface()
		{
			GLuint curColorTexId;
			GLuint curDepthTexId;
			{
				int curIndex;
				ovr_GetTextureSwapChainCurrentIndex(Session, ColorTextureChain, &curIndex);
				ovr_GetTextureSwapChainBufferGL(Session, ColorTextureChain, curIndex, &curColorTexId);
			}
			{
				int curIndex;
				ovr_GetTextureSwapChainCurrentIndex(Session, DepthTextureChain, &curIndex);
				ovr_GetTextureSwapChainBufferGL(Session, DepthTextureChain, curIndex, &curDepthTexId);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, fboId);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curColorTexId, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, curDepthTexId, 0);

			glViewport(0, 0, texSize.w, texSize.h);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_FRAMEBUFFER_SRGB);
		}

		void UnsetRenderSurface()
		{
			glBindFramebuffer(GL_FRAMEBUFFER, fboId);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
		}

		void Commit()
		{
			ovr_CommitTextureSwapChain(Session, ColorTextureChain);
			ovr_CommitTextureSwapChain(Session, DepthTextureChain);
		}
	*/
}OculusTextureBuffer;

static const int32_t kEyeCount = 2;
#elif VRSYSTEM_OCULUS_MOBILE

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"
typedef struct ANativeWindow ANativeWindow;

typedef struct
{
	EGLint		MajorVersion;
	EGLint		MinorVersion;
	EGLDisplay	Display;
	EGLConfig	Config;
	EGLSurface	TinySurface;
	EGLSurface	MainSurface;
	EGLContext	Context;
} ovrEgl;

typedef struct ovrApp
{
	ovrJava				Java;
	ovrEgl				Egl;
	ANativeWindow* NativeWindow;
	bool				Resumed;
	ovrMobile* Ovr;
	ovrDeviceID			ControllerIDs[2];
	ovrInputStateTrackedRemote ControllerState[2];
	ovrDeviceID			HandIDs[2];
	ovrInputStateHand	HandState[2];
	//	ovrScene			Scene;
	//	ovrSimulation		Simulation;
	long long			FrameIndex;
	double 				DisplayTime;
	int					SwapInterval;
	int					CpuLevel;
	int					GpuLevel;
	int					MainThreadTid;
	int					RenderThreadTid;
	float				RefreshRate;
	bool				BackButtonDownLastFrame;
#if MULTI_THREADED
	ovrRenderThread		RenderThread;
#else
	//	ovrRenderer			Renderer;
#endif
	bool				UseMultiview;
	bool				ShouldPause;
} ovrApp;

static const int32_t kEyeCount = 2;

#else // VRSYSTEM_NONE
static const int32_t kEyeCount = 1;
#endif

typedef struct rvrs_initParams
{
#ifdef VRSYSTEM_OCULUS_WIN
	ovrInitParams ovrInitParams;
	const ovrMirrorTextureDesc ovrMirrorTextureDesc;
#elif VRSYSTEM_OCULUS_MOBILE
	ovrInitParms ovrInitParams;
	void* appState;
	void* ovrLibrary;
#endif
	const char* appId;
	int32_t oversamplePercentage;		// Scaling factor as a percentage. i.e. 100 = 1.0f, 140 = 1.4f etc.
}rvrs_initParams;

typedef struct rvrs_trackingInfo
{
#ifdef VRSYSTEM_OCULUS_WIN
	ovrTrackingState			trackState;
	ovrInputState				controllerState;
#elif VRSYSTEM_OCULUS_MOBILE
	ovrTracking2				hmdTracking;
	ovrTracking					controllerTracking[2];
	ovrInputStateTrackedRemote	controllerState[2];
	uint32_t					handIDs[2];
	ovrInputStateHand			handState[2];
	ovrHandPose					handPose[2];
	ovrHandSkeleton				handSkeleton[2];
	ovrHandMesh					handMesh[2];
	rgfx_renderable				debugCapsules[19];
#else //VRSYSTEM_NONE
#endif
	bool						hmdMounted;
	bool						trackingControllers;
}rvrs_trackingInfo;

typedef struct rvrs_vec2
{
	float x;
	float y;
}rvrs_vec2;

typedef struct rvrs_vec3
{
	float x;
	float y;
	float z;
}rvrs_vec3;

typedef struct rvrs_pose
{
	quat orientation;
	rvrs_vec3 position;
}rvrs_pose;

typedef struct rvrs_rigidBodyPose
{
	rvrs_pose pose;					//28 bytes
	rvrs_vec3 angularVelocity;		//12 bytes
	rvrs_vec3 linearVelocity;		//12 bytes
	rvrs_vec3 angularAcceleration;	//12 bytes
	rvrs_vec3 linearAcceleration;	//12 bytes
	uint32_t pad;					//4 bytes pads to 80 bytes
	double timeInSeconds; //< Absolute time of this pose.					//88 bytes
	double predictionInSeconds; //< Seconds this pose was predicted ahead.	//96 bytes
}rvrs_rigidBodyPose;

typedef struct rvrs_appInfo
{
	void* appState;
}rvrs_appInfo;

typedef struct rvrs_eyeSize
{
	int32_t width;
	int32_t height;
}rvrs_eyeSize;

typedef struct rvrs_controllerState
{
	uint32_t buttons;
	uint32_t buttonsPressed;
	uint32_t touches;
	float trigger;
	float grip;
	vec2 joystick;
	vec2 JoystickNoDeadZone;
	bool triggerPressed;
}rvrs_controllerState;

typedef struct nativeSystem
{
#ifdef VRSYSTEM_OCULUS_WIN
	ovrSession session;
	ovrSessionStatus sessionStatus;
	ovrHmdDesc hmdDesc;
	OculusTextureBuffer* eyeRenderTexture[2];
	ovrPosef eyeRenderPose[2];
	ovrTimewarpProjectionDesc posTimewarpProjectionDesc[2];
	ovrMirrorTexture mirrorTexture;
	uint32_t mirrorFBO;
	double sensorSampleTime;    // sensorSampleTime is fed into the layer later
	bool bInitialized;
	//	bool bPlatformInitialized;
#else
	bool isMounted;
#endif
}nativeSystem;

#define kAvatarNumHands (2)

typedef struct rvrs_avatarHandParams
{
	vec4 position;
	vec4 rotation;
	uint32_t buttonMask;
	uint32_t touchMask;
	float joystickX;
	float joystickY;
	float indexTrigger;
	float handTrigger;
	bool isActive;
	bool isAvatar;				// CLR - Should this use avatar information (true) or hand tracking information (false).
	uint8_t pad[6];
}rvrs_avatarHandParams;

typedef struct rvrs_avatarFrameParams
{
	vec4 eyePos;
	vec4 eyeRot;
	rvrs_avatarHandParams hands[kAvatarNumHands];
	mat4x4 avatarWorldMatrix;
	int32_t playerIndex;
}rvrs_avatarFrameParams;

typedef struct rvrs_avatarTransforms
{
	mat4x4* worldMatrive;
	vec4	headPos;
	vec4	headRot;
}rvrs_avatarTransformsDesc;

typedef struct rvrs_handsVertParamsOvr
{
	mat4x4 bones[28];
}rvrs_handsVertParamsOvr;

typedef struct rvrs_handRenderData
{
	rgfx_pipeline pipeline;
	rgfx_vertexBuffer vertexBuffer;
	int32_t uniformIndexHand;
	rgfx_renderable rend[2];
	rgfx_buffer vertexParams;
	mat4x4* inverseBindPoseArrays[2];
}rvrs_handRenderData;

typedef struct rvrs_data
{
	mat4x4 eyeMatrices[2];
	mat4x4 handMatrices[2];
	uint32_t prevButtons[2];
	bool prevTrigger[2];
	float supportedRefreshRates[MAX_SUPPORTED_REFRESH_RATES];
	rvrs_deviceType deviceType;
	rvrs_avatarFrameParams avatarFrameParams;
/*
	rgfx_pipeline handsPipeline;
	rgfx_vertexBuffer handsVertexBuffer;
	int32_t handsUniformIndexHand;
	mat4x4* handsInverseBindPoseArrays[2];
*/
	rvrs_handRenderData handRenderData;
	mat4x4 avatarWorldMatrix;
	vec4 avatarLeftHandPos;
	vec4 avatarLeftHandRot;
	vec4 avatarRightHandPos;
	vec4 avatarRightHandRot;
	int32_t eyeWidth;
	int32_t eyeHeight;
	int32_t oversamplePercentage;
	int32_t mirrorTextureWidth;
	int32_t mirrorTextureHeight;
	int32_t numSuportedRefreshRates;
	uint64_t frameIndex;
	nativeSystem native;
	bool bInitialized;
}rvrs_data;


void rvrs_initialize(const rvrs_initParams* params);
void rvrs_terminate();

bool rvrs_isInitialized();
bool rvrs_isHmdMounted();
bool rvrs_isTrackingControllers();
bool rvrs_shouldPause();
bool rvrs_shouldQuit();
rvrs_deviceType rvrs_getDeviceType(void);
float rvrs_getRefreshRate();
bool rvrs_setRefreshRate(float refreshRate);
bool rvrs_setFoveationLevel(int32_t foveationLevel);

rvrs_eyeSize rvrs_getScaledEyeSize(int32_t scale);
int32_t rvrs_getSupportedRefreshRates(const float**);

void rvrs_updateTracking();
mat4x4 rvrs_getHeadMatrix();
int32_t rvrs_getHeadPose(rvrs_rigidBodyPose* pose);
mat4x4 rvrs_getViewMatrixForEye(int32_t index);
mat4x4 rvrs_getProjectionMatrixForEye(int32_t index);
mat4x4 rvrs_getTrackedControllerMatrix(int32_t index);
int32_t rvrs_getTrackedControllerPose(int32_t index, rvrs_rigidBodyPose* pose);
int32_t rvrs_getControllerState(int32_t index, rvrs_controllerState* controllerState);
void rvrs_setControllerHapticVibration(int32_t index, const float intensity);

ovrPosef* rvrs_getHandPose(int32_t index);
ovrQuatf* rvrs_getHandBoneRotations(int32_t index);
#ifdef VRSYSTEM_OCULUS_MOBILE
ovrHandSkeleton* rvrs_getHandSkeleton(int32_t index);
mat4x4 rvrs_getHandScale(int32_t index);
rvrs_handRenderData* rvrs_getHandRenderData();
#endif

void rvrs_beginFrame();
void rvrs_submit();
void rvrs_updateAvatarFrameParams(int32_t avatarIdx, float dt, const rvrs_avatarFrameParams* params);
void rvrs_renderAvatar(int32_t eye);
#ifdef VRSYSTEM_OCULUS_MOBILE
void rvrs_initializeHandMesh(int32_t hand, ovrHandSkeleton* skeleton, ovrHandMesh* mesh);
#endif

rvrs_textureSwapChain rvrs_createTextureSwapChain(rvrs_textureType type, rgfx_format format, int32_t width, int32_t height, int32_t levels, int32_t bufferCount);
void rvrs_destroyTextureSwapChain(rvrs_textureSwapChain swapChain);
int32_t rvrs_getTextureSwapChainLength(rvrs_textureSwapChain swapChain);
uint32_t rvrs_getTextureSwapChainBufferGL(rvrs_textureSwapChain swapChain, int32_t buffer);

GLuint rvrs_bindEyeFramebuffer(int32_t eye);

void rvrs_getEyeWidthHeight(int32_t *width, int32_t *height);

typedef struct rvrsApi
{
	bool(*isInitialized)(void);
	bool(*isHmdMounted)(void);
	bool(*isTrackingControllers)(void);
	bool(*shouldPause)(void);
	bool(*shouldQuit)(void);
	rvrs_deviceType(*getDeviceType)(void);
	float(*getRefreshRate)(void);
	rvrs_eyeSize(*getScaledEyeSize)(int32_t newScale);
	int32_t(*getSupportedRefreshRates)(const float** ppSupportedRefreshRates);
	bool(*setRefreshRate)(float refreshRate);
	bool(*setFoveationLevel)(int32_t foveationLevel);
	mat4x4(*getHeadMatrix)();
	int32_t(*getHeadPose)(rvrs_rigidBodyPose* pose);
	mat4x4(*getViewMatrixForEye)(int32_t index);
	mat4x4(*getProjectionMatrixForEye)(int32_t index);
	mat4x4(*getTrackedControllerMatrix)(int32_t index);
	int32_t(*getTrackedControllerPose)(int32_t index, rvrs_rigidBodyPose* pose);
	int32_t(*getControllerState)(int32_t index, rvrs_controllerState* controllerState);
	void(*setControllerHapticVibration)(int32_t index, const float intensity);
	void(*updateAvatarFrameParams)(int32_t avatarIdx, float dt, const rvrs_avatarFrameParams* params);
	void(*initializePlatformOvr)(const rvrs_platformDescOvr* desc);
	void(*createPrivateRoomOvr)(const rvrs_ovrCreateRoomDesc* desc);
	void(*inviteUserOvr)(const rvrs_ovrInviteUserDesc* desc);
	void(*kickUserOvr)(uint64_t roomID, uint64_t userID);
	void(*joinRoomOvr)(uint64_t roomId);
	void(*leaveRoomOvr)(uint64_t roomId);
	void(*netConnect)(const rvrs_ovrNetConnectDesc* desc);
	void(*netClose)(const uint64_t userId);
	void(*netSendPacket)(const rvrp_netPacketDesc* desc);
	int32_t(*netReadPackets)(rvrp_netPacketDesc* packets, uint8_t* packetBuffer, const int32_t maxPackets);
	void(*netPing)(const uint64_t userId);
	void(*voipStart)(const rvrs_ovrVoipStartDesc* desc);
	void(*voipStop)(const uint64_t userId);
	void(*voipMute)(bool muted);
	size_t(*voipGetPCM)(const uint64_t userId, int16_t* voipBuffer, size_t voipBufferSize);

	void(*initializeAvatarOvr)(const char* appId);
	bool(*requestAvatarSpecOvr)(uint64_t userId, int32_t slot);
	bool(*destroyAvatarOvr)(int32_t slot);
}rvrsApi;
