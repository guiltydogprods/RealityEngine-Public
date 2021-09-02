#include "stdafx.h"
#include "vrsystem.h"
#include "system.h"
#include "../rvrs_ovr_platform/rvrs_platform_ovr.h"
#include "../rvrs_ovr_platform/rvrs_avatar_ovr.h"

static __attribute__((aligned(64))) rvrs_data s_data = { 0 };
static rvrs_trackingInfo s_tracking = { 0 };

//#define DISABLE_AVATAR

#ifdef VRSYSTEM_OCULUS_WIN
#include "OVR_Platform.h"


#define OCULUS_APP_ID "1869952619797041"

typedef struct rvrs_textureSwapChainInfo
{
	ovrTextureSwapChain colorSwapChain;
	ovrTextureSwapChain depthSwapChain;
	ovrTimewarpProjectionDesc projectionDesc;
	GLuint swapChainFBO;
	GLenum swapChainTextureTarget;
}rvrs_textureSwapChainInfo;
static rvrs_textureSwapChainInfo s_swapChainInfo[2];
static int32_t s_swapChainCount = 0;

#endif

void rvrs_initialize(const rvrs_initParams* params)
{
	memset(&s_data, 0, sizeof(rvrs_data));
	memset(&s_tracking, 0, sizeof(rvrs_trackingInfo));

#ifdef VRSYSTEM_OCULUS_WIN
	// Initializes LibOVR, and the Rift
//	ovrInitParams initParams = { ovrInit_RequestVersion | ovrInit_FocusAware, OVR_MINOR_VERSION, NULL, 0, 0 };
	ovrResult result = ovr_Initialize(&params->ovrInitParams);
	if (result >= 0)
	{
		ovrGraphicsLuid luid;
		ovrResult result = ovr_Create(&s_data.native.session, &luid);
		if (result >= 0)
		{
			ovr_GetSessionStatus(s_data.native.session, &s_data.native.sessionStatus);
			if (!s_data.native.sessionStatus.HmdPresent)
				return;

			s_data.native.hmdDesc = ovr_GetHmdDesc(s_data.native.session);
			ovr_SetTrackingOriginType(s_data.native.session, ovrTrackingOrigin_FloorLevel);

			ovrSizei idealTextureSize = ovr_GetFovTextureSize(s_data.native.session, ovrEye_Left, s_data.native.hmdDesc.DefaultEyeFov[0], 1);
#if 0
			// Make eye render buffers
			for (uint32_t eye = 0; eye < 2; ++eye)
			{
				ovrSizei idealTextureSize = ovr_GetFovTextureSize(s_data.native.session, eye == 0 ? ovrEye_Left : ovrEye_Right, s_data.native.hmdDesc.DefaultEyeFov[eye], 1);
				(void)idealTextureSize;
				assert(0); // CLR: Port Me
//				m_eyeRenderTexture[eye] = allocator.newObject<OculusTextureBuffer>(m_session, idealTextureSize, 1);
				/*
									if (!m_eyeRenderTexture[eye]->ColorTextureChain || !m_eyeRenderTexture[eye]->DepthTextureChain)
									{
										if (retryCreate) goto Done;
										VALIDATE(false, "Failed to create texture.");
									}
				*/
			}
#endif
			// Create mirror texture and an FBO used to copy mirror texture to back buffer
			result = ovr_CreateMirrorTextureWithOptionsGL(s_data.native.session, &params->ovrMirrorTextureDesc, &s_data.native.mirrorTexture);
			if (result >= 0)
			{
				// Configure the mirror read buffer
				GLuint texId;
				ovr_GetMirrorTextureBufferGL(s_data.native.session, s_data.native.mirrorTexture, &texId);

				glGenFramebuffers(1, &s_data.native.mirrorFBO);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, s_data.native.mirrorFBO);
				glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
				glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			}
			s_data.eyeWidth = idealTextureSize.w;
			s_data.eyeHeight = idealTextureSize.h;
			s_data.mirrorTextureWidth = params->ovrMirrorTextureDesc.Width;
			s_data.mirrorTextureHeight = params->ovrMirrorTextureDesc.Height;
			s_data.bInitialized = true;
		}
/*
		if (s_data.bInitialized) //params->appId != NULL)
		{
			ovrPlatformInitializeResult outResult;
			//		if (ovr_PlatformInitializeWindowsAsynchronousEx(OCULUS_APP_ID, &outResult, PLATFORM_PRODUCT_VERSION, PLATFORM_MAJOR_VERSION) != ovrPlatformInitialize_Success)
			if (ovr_PlatformInitializeWindowsAsynchronous(OCULUS_APP_ID, &outResult) != ovrPlatformInitialize_Success)
			{
				// Exit.  Initialization failed which means either the oculus service isn’t
				// on the machine or they’ve hacked their DLL
			}
			ovr_Entitlement_GetIsViewerEntitled();
			//		ovrAvatar_Initialize(OCULUS_APP_ID);
		}
*/
	}
#endif
}

bool rvrs_isInitialized()
{
	return s_data.bInitialized;
}

bool rvrs_isHmdMounted()
{
#ifdef VRSYSTEM_OCULUS_WIN
	return s_data.bInitialized ? s_data.native.sessionStatus.HmdMounted == ovrTrue : false;
#else
	return false;
#endif
}

bool rvrs_isTrackingControllers()
{
	return true;
}

bool rvrs_shouldPause()
{
	return false;
}

bool rvrs_shouldQuit()
{
	return s_data.native.sessionStatus.ShouldQuit == ovrTrue;
}

rvrs_deviceType rvrs_getDeviceType(void)
{
	return kVrDeviceTypeRift;
}

float rvrs_getRefreshRate()
{
	return 1.0f / s_data.native.hmdDesc.DisplayRefreshRate;
}

bool rvrs_setRefreshRate(float refreshRate)
{
	(void)refreshRate;
	return true;
}

bool rvrs_setFoveationLevel(int32_t foveationLevel)
{
	return false;
}

rvrs_eyeSize rvrs_getScaledEyeSize(int32_t newScale)
{
	assert(newScale >= 100);
	assert(newScale <= 200);

	int32_t eyeWidth = s_data.eyeWidth;
	int32_t eyeHeight = s_data.eyeHeight;

	int newWidth = eyeWidth;
	int newHeight = eyeHeight;

	s_data.oversamplePercentage = newScale;

	return (rvrs_eyeSize) {
		newWidth, newHeight
	};
}

int32_t rvrs_getSupportedRefreshRates(const float** ppSupportedRefreshRates)
{
	*ppSupportedRefreshRates = &s_data.native.hmdDesc.DisplayRefreshRate;
	return 1;
}

void rvrs_updateTracking(const rvrs_appInfo* appInfo)
{
	if (!s_data.bInitialized)
		return;

	ovr_GetSessionStatus(s_data.native.session, &s_data.native.sessionStatus);
	if (s_data.native.sessionStatus.ShouldRecenter)
	{
		ovr_RecenterTrackingOrigin(s_data.native.session);
	}

	// Call ovr_GetRenderDesc each frame to get the ovrEyeRenderDesc, as the returned values (e.g. HmdToEyePose) may change at runtime.
	ovrEyeRenderDesc eyeRenderDesc[2];
	eyeRenderDesc[0] = ovr_GetRenderDesc(s_data.native.session, ovrEye_Left, s_data.native.hmdDesc.DefaultEyeFov[0]);
	eyeRenderDesc[1] = ovr_GetRenderDesc(s_data.native.session, ovrEye_Right, s_data.native.hmdDesc.DefaultEyeFov[1]);

	// Get eye poses, feeding in correct IPD offset
	ovrPosef HmdToEyePose[2] = { eyeRenderDesc[0].HmdToEyePose,
								 eyeRenderDesc[1].HmdToEyePose };

	ovr_GetEyePoses(s_data.native.session, s_data.frameIndex, ovrTrue, HmdToEyePose, s_data.native.eyeRenderPose, &s_data.native.sensorSampleTime);
	double displayMidpointSeconds = 0.0;
	s_tracking.trackState = ovr_GetTrackingState(s_data.native.session, displayMidpointSeconds, ovrTrue);

	ovr_GetInputState(s_data.native.session, ovrControllerType_Touch, &s_tracking.controllerState);

#ifndef DISABLE_AVATAR
	rvrs_processOvrMessages();
#endif
}

void rvrs_updateAvatarFrameParams(int32_t avatarIdx, float dt, const rvrs_avatarFrameParams* params)
{
#if 0
	if (params != 0)
		s_data.avatarFrameParams = *params;
	else
		memset(&s_data.avatarFrameParams, 0, sizeof(rvrs_avatarFrameParams));

#ifndef DISABLE_AVATAR
	if (vec4_isZero(s_data.avatarFrameParams.eyePos)) {
		float px = s_tracking.trackState.HeadPose.ThePose.Position.x;
		float py = s_tracking.trackState.HeadPose.ThePose.Position.y;
		float pz = s_tracking.trackState.HeadPose.ThePose.Position.z;
		s_data.avatarFrameParams.eyePos = (vec4){ px, py, pz, 1.0f };
	}
	if (vec4_isZero(s_data.avatarFrameParams.eyeRot)) {
		float rx = s_tracking.trackState.HeadPose.ThePose.Orientation.x;
		float ry = s_tracking.trackState.HeadPose.ThePose.Orientation.y;
		float rz = s_tracking.trackState.HeadPose.ThePose.Orientation.z;
		float rw = s_tracking.trackState.HeadPose.ThePose.Orientation.w;
		s_data.avatarFrameParams.eyeRot = (vec4){ rx, ry, rz, rw };
	}
	if (vec4_isZero(s_data.avatarFrameParams.hands[0].position)) {
		float px = s_tracking.trackState.HandPoses[0].ThePose.Position.x;
		float py = s_tracking.trackState.HandPoses[0].ThePose.Position.y;
		float pz = s_tracking.trackState.HandPoses[0].ThePose.Position.z;
		s_data.avatarFrameParams.hands[0].position = (vec4){ px, py, pz, 1.0f };
		s_data.avatarFrameParams.hands[0].isActive = true;
	}
	if (vec4_isZero(s_data.avatarFrameParams.hands[0].rotation)) {
		float rx = s_tracking.trackState.HandPoses[0].ThePose.Orientation.x;
		float ry = s_tracking.trackState.HandPoses[0].ThePose.Orientation.y;
		float rz = s_tracking.trackState.HandPoses[0].ThePose.Orientation.z;
		float rw = s_tracking.trackState.HandPoses[0].ThePose.Orientation.w;
		s_data.avatarFrameParams.hands[0].rotation = (vec4){ rx, ry, rz, rw };
	}
	if (vec4_isZero(s_data.avatarFrameParams.hands[1].position)) {
		float px = s_tracking.trackState.HandPoses[1].ThePose.Position.x;
		float py = s_tracking.trackState.HandPoses[1].ThePose.Position.y;
		float pz = s_tracking.trackState.HandPoses[1].ThePose.Position.z;
		s_data.avatarFrameParams.hands[1].position = (vec4){ px, py, pz, 1.0f };
		s_data.avatarFrameParams.hands[1].isActive = true;
	}
	if (vec4_isZero(s_data.avatarFrameParams.hands[1].rotation)) {
		float rx = s_tracking.trackState.HandPoses[1].ThePose.Orientation.x;
		float ry = s_tracking.trackState.HandPoses[1].ThePose.Orientation.y;
		float rz = s_tracking.trackState.HandPoses[1].ThePose.Orientation.z;
		float rw = s_tracking.trackState.HandPoses[1].ThePose.Orientation.w;
		s_data.avatarFrameParams.hands[1].rotation = (vec4){ rx, ry, rz, rw };
	}
#endif
#endif
	memcpy(&s_data.avatarFrameParams, params, sizeof(rvrs_avatarFrameParams));
	rvrs_updateAvatarInstance(avatarIdx, dt, params); // &s_data.avatarFrameParams);
}

#include "OVR_Avatar.h"

void rvrs_renderAvatar(int32_t eye)
{
#ifndef DISABLE_AVATAR
	if (vec4_isZero(s_data.avatarFrameParams.eyePos)) {
		float px = s_tracking.trackState.HeadPose.ThePose.Position.x;
		float py = s_tracking.trackState.HeadPose.ThePose.Position.y;
		float pz = s_tracking.trackState.HeadPose.ThePose.Position.z;
		s_data.avatarFrameParams.eyePos = (vec4){ px, py, pz, 1.0f };
	}
	if (vec4_isZero(s_data.avatarFrameParams.eyeRot)) {
		float rx = s_tracking.trackState.HeadPose.ThePose.Orientation.x;
		float ry = s_tracking.trackState.HeadPose.ThePose.Orientation.y;
		float rz = s_tracking.trackState.HeadPose.ThePose.Orientation.z;
		float rw = s_tracking.trackState.HeadPose.ThePose.Orientation.w;
		s_data.avatarFrameParams.eyeRot = (vec4){ rx, ry, rz, rw };
	}
	if (vec4_isZero(s_data.avatarFrameParams.hands[0].position)) {
		float px = s_tracking.trackState.HandPoses[0].ThePose.Position.x;
		float py = s_tracking.trackState.HandPoses[0].ThePose.Position.y;
		float pz = s_tracking.trackState.HandPoses[0].ThePose.Position.z;
		s_data.avatarFrameParams.hands[0].position = (vec4){ px, py, pz, 1.0f };
		s_data.avatarFrameParams.hands[0].isActive = true;
	}
	if (vec4_isZero(s_data.avatarFrameParams.hands[0].rotation)) {
		float rx = s_tracking.trackState.HandPoses[0].ThePose.Orientation.x;
		float ry = s_tracking.trackState.HandPoses[0].ThePose.Orientation.y;
		float rz = s_tracking.trackState.HandPoses[0].ThePose.Orientation.z;
		float rw = s_tracking.trackState.HandPoses[0].ThePose.Orientation.w;
		s_data.avatarFrameParams.hands[0].rotation = (vec4){ rx, ry, rz, rw };
	}
	if (vec4_isZero(s_data.avatarFrameParams.hands[1].position)) {
		float px = s_tracking.trackState.HandPoses[1].ThePose.Position.x;
		float py = s_tracking.trackState.HandPoses[1].ThePose.Position.y;
		float pz = s_tracking.trackState.HandPoses[1].ThePose.Position.z;
		s_data.avatarFrameParams.hands[1].position = (vec4){ px, py, pz, 1.0f };
		s_data.avatarFrameParams.hands[1].isActive = true;
	}
	if (vec4_isZero(s_data.avatarFrameParams.hands[1].rotation)) {
		float rx = s_tracking.trackState.HandPoses[1].ThePose.Orientation.x;
		float ry = s_tracking.trackState.HandPoses[1].ThePose.Orientation.y;
		float rz = s_tracking.trackState.HandPoses[1].ThePose.Orientation.z;
		float rw = s_tracking.trackState.HandPoses[1].ThePose.Orientation.w;
		s_data.avatarFrameParams.hands[1].rotation = (vec4){ rx, ry, rz, rw };
	}

	rvrs_renderAvatarOvr(eye, &s_data.avatarFrameParams);
#endif
}

mat4x4 rvrs_getHeadMatrix()
{
	float px = s_tracking.trackState.HeadPose.ThePose.Position.x;
	float py = s_tracking.trackState.HeadPose.ThePose.Position.y;
	float pz = s_tracking.trackState.HeadPose.ThePose.Position.z;

	float qx = s_tracking.trackState.HeadPose.ThePose.Orientation.x;
	float qy = s_tracking.trackState.HeadPose.ThePose.Orientation.y;
	float qz = s_tracking.trackState.HeadPose.ThePose.Orientation.z;
	float qw = s_tracking.trackState.HeadPose.ThePose.Orientation.w;

	return mat4x4_create((vec4) { qx, qy, qz, qw }, (vec4) { px, py, pz, 1.0f });
}

int32_t rvrs_getHeadPose(rvrs_rigidBodyPose* pose)
{
	memcpy(pose, &s_tracking.trackState.HeadPose, sizeof(rvrs_rigidBodyPose));
	return 0;
}

mat4x4 rvrs_getViewMatrixForEye(int32_t index)
{
#ifdef VRSYSTEM_OCULUS_WIN
	assert(index >= 0);
	assert(index < 2);

	float px = s_data.native.eyeRenderPose[index].Position.x;
	float py = s_data.native.eyeRenderPose[index].Position.y;
	float pz = s_data.native.eyeRenderPose[index].Position.z;

	float qx = s_data.native.eyeRenderPose[index].Orientation.x;
	float qy = s_data.native.eyeRenderPose[index].Orientation.y;
	float qz = s_data.native.eyeRenderPose[index].Orientation.z;
	float qw = s_data.native.eyeRenderPose[index].Orientation.w;

	mat4x4 viewMatrix = mat4x4_create((vec4) { qx, qy, qz, qw }, (vec4) { px, py, pz, 1.0f });

	mat4x4 retMat = mat4x4_orthoInverse(viewMatrix);

	return retMat;
#else
	return mat4x4_identity();
#endif
}

mat4x4 rvrs_getProjectionMatrixForEye(int32_t index)
{
#ifdef VRSYSTEM_OCULUS_WIN
	assert(index >= 0);
	assert(index < 2);

	const float nearZ = 0.01f;
	const float farZ = 100.0f;
	ovrMatrix4f nativeProjectionMatrix = ovrMatrix4f_Projection(s_data.native.hmdDesc.DefaultEyeFov[index], nearZ, farZ, ovrProjection_None | ovrProjection_ClipRangeOpenGL);
	s_swapChainInfo[index].projectionDesc = ovrTimewarpProjectionDesc_FromProjection(nativeProjectionMatrix, ovrProjection_None | ovrProjection_ClipRangeOpenGL);

	mat4x4 projectionMatrix;
	projectionMatrix.xAxis = (vec4){ nativeProjectionMatrix.M[0][0], nativeProjectionMatrix.M[1][0], nativeProjectionMatrix.M[2][0], nativeProjectionMatrix.M[3][0] };
	projectionMatrix.yAxis = (vec4){ nativeProjectionMatrix.M[0][1], nativeProjectionMatrix.M[1][1], nativeProjectionMatrix.M[2][1], nativeProjectionMatrix.M[3][1] };
	projectionMatrix.zAxis = (vec4){ nativeProjectionMatrix.M[0][2], nativeProjectionMatrix.M[1][2], nativeProjectionMatrix.M[2][2], nativeProjectionMatrix.M[3][2] };
	projectionMatrix.wAxis = (vec4){ nativeProjectionMatrix.M[0][3], nativeProjectionMatrix.M[1][3], nativeProjectionMatrix.M[2][3], nativeProjectionMatrix.M[3][3] };

	return projectionMatrix;
#else
	return mat4x4_identity();
#endif
}

mat4x4 rvrs_getTrackedControllerMatrix(int32_t index)
{
#ifdef VRSYSTEM_OCULUS_WIN
	assert(index >= 0);
	assert(index < 2);

	float px = s_tracking.trackState.HandPoses[index].ThePose.Position.x;
	float py = s_tracking.trackState.HandPoses[index].ThePose.Position.y;
	float pz = s_tracking.trackState.HandPoses[index].ThePose.Position.z;

	float qx = s_tracking.trackState.HandPoses[index].ThePose.Orientation.x;
	float qy = s_tracking.trackState.HandPoses[index].ThePose.Orientation.y;
	float qz = s_tracking.trackState.HandPoses[index].ThePose.Orientation.z;
	float qw = s_tracking.trackState.HandPoses[index].ThePose.Orientation.w;

	return mat4x4_create((vec4) { qx, qy, qz, qw }, (vec4) { px, py, pz, 1.0f });
#else
	return mat4x4_identity();
#endif
}

int32_t rvrs_getTrackedControllerPose(int32_t index, rvrs_rigidBodyPose* pose)
{
	if (index >= 0 && index < 2)
	{
		memcpy(pose, &s_tracking.trackState.HandPoses[index], sizeof(rvrs_rigidBodyPose));
		return 0;
	}
	else
	{
		return -1;
	}
}

int32_t rvrs_getControllerState(int32_t index, rvrs_controllerState* controllerState)
{
#ifdef VRSYSTEM_OCULUS_WIN
	assert(index >= 0);
	assert(index < 2);

	controllerState->buttons = s_tracking.controllerState.Buttons;
	controllerState->buttonsPressed = controllerState->buttons & ~s_data.prevButtons[index];
	controllerState->touches = s_tracking.controllerState.Touches;
	controllerState->trigger = s_tracking.controllerState.IndexTrigger[index];
	controllerState->grip = s_tracking.controllerState.HandTrigger[index];
	controllerState->joystick = (vec2){ s_tracking.controllerState.Thumbstick[index].x, s_tracking.controllerState.Thumbstick[index].y };
	controllerState->JoystickNoDeadZone = (vec2){ s_tracking.controllerState.ThumbstickNoDeadzone[index].x, s_tracking.controllerState.ThumbstickNoDeadzone[index].y };
	s_data.prevButtons[index] = controllerState->buttons;
#endif
	return 0;
}

void rvrs_setControllerHapticVibration(int32_t index, const float intensity)
{
	assert(index >= 0);
	assert(index < 2);
	ovr_SetControllerVibration(s_data.native.session, ovrControllerType_LTouch + index, 0.5f, intensity);
}

ovrPosef* rvrs_getHandPose(int32_t index)
{
	(void)index;
	return NULL;
}

ovrQuatf* rvrs_getHandBoneRotations(int32_t index)
{
	(void)index;
	return NULL;
}

void* rvrs_getHandSkeleton(int32_t index)
{
	(void)index;
	return NULL;
}

mat4x4 rvrs_getHandScale(int32_t index)
{
	(void)index;
	return mat4x4_identity();
}

rvrs_handRenderData* rvrs_getHandRenderData()
{
	return NULL;
}

void rvrs_beginFrame()
{
#ifdef VRSYSTEM_OCULUS_WIN
//	ovr_WaitToBeginFrame(s_data.native.session, s_data.frameIndex);
//	ovr_BeginFrame(s_data.native.session, s_data.frameIndex);
#endif
}

void rvrs_submit()
{
#ifdef VRSYSTEM_OCULUS_WIN
#ifdef RENDER_DIRECT_TO_SWAPCHAIN
	ovrLayerEyeFovDepth ld = {};
	ld.Header.Type = ovrLayerType_EyeFovDepth;
	ld.ProjectionDesc = s_swapChainInfo[0].projectionDesc;
#else
	ovrLayerEyeFov ld = {};
	ld.Header.Type = ovrLayerType_EyeFov;
#endif
	ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;   // Because OpenGL.

	for (int eye = 0; eye < 2; ++eye)
	{
//		m_eyeRenderTexture[eye]->UnsetRenderSurface();
		glBindFramebuffer(GL_FRAMEBUFFER, s_swapChainInfo[eye].swapChainFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
#ifdef RENDER_DIRECT_TO_SWAPCHAIN
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
#endif
//		m_eyeRenderTexture[eye]->Commit();
		ovr_CommitTextureSwapChain(s_data.native.session, s_swapChainInfo[eye].colorSwapChain);
		ovr_CommitTextureSwapChain(s_data.native.session, s_swapChainInfo[eye].depthSwapChain);

		ld.ColorTexture[eye] = s_swapChainInfo[eye].colorSwapChain;
#ifdef RENDER_DIRECT_TO_SWAPCHAIN
		ld.DepthTexture[eye] = s_swapChainInfo[eye].depthSwapChain;
#endif
		ld.Viewport[eye].Pos.x = 0;
		ld.Viewport[eye].Pos.y = 0;
		ld.Viewport[eye].Size.w = s_data.eyeWidth;
		ld.Viewport[eye].Size.h = s_data.eyeHeight;

		ld.Fov[eye] = s_data.native.hmdDesc.DefaultEyeFov[eye];
		ld.RenderPose[eye] = s_data.native.eyeRenderPose[eye];
		ld.SensorSampleTime = s_data.native.sensorSampleTime;
	}

	const ovrLayerHeader* layers = &ld.Header;
	ovrResult result = ovr_SubmitFrame(s_data.native.session, s_data.frameIndex++, NULL, &layers, 1);
	// exit the rendering loop if submit returns an error, will retry on ovrError_DisplayLost
	if (!OVR_SUCCESS(result))
		rsys_print("ovr_EndFrame() failed!\n");

	// Blit mirror texture to back buffer
	glBindFramebuffer(GL_READ_FRAMEBUFFER, s_data.native.mirrorFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	GLint w = s_data.mirrorTextureWidth; // windowSize.w;
	GLint h = s_data.mirrorTextureHeight; // windowSize.h;
	glBlitFramebuffer(0, h, w, 0,
		0, 0, w, h,
		GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

#endif
}

rvrs_textureSwapChain rvrs_createTextureSwapChain(rvrs_textureType type, rgfx_format format, int32_t width, int32_t height, int32_t levels, int32_t bufferCount)
{
#ifdef VRSYSTEM_OCULUS_WIN
	assert(s_data.native.session); // No HMD? A little odd.

	ovrTextureFormat ovrFormat = OVR_FORMAT_UNKNOWN;
	switch (format)
	{
	case kFormatARGB8888:
		ovrFormat = OVR_FORMAT_R8G8B8A8_UNORM;
		break;
	case kFormatSRGBA8:
		ovrFormat = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		break;
	default:
		assert(0);
	}

	ovrTextureSwapChainDesc desc = {};
	desc.Type = ovrTexture_2D;
	desc.ArraySize = 1;
	desc.Width = s_data.eyeWidth;
	desc.Height = s_data.eyeHeight;
	desc.MipLevels = 1;
	desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
#ifdef RENDER_DIRECT_TO_SWAPCHAIN
	desc.SampleCount = 4;
#else
	desc.SampleCount = 1;
#endif
	desc.StaticImage = ovrFalse;

	GLenum target = desc.SampleCount > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

	assert(s_swapChainCount <= 2);
	ovrResult result = ovr_CreateTextureSwapChainGL(s_data.native.session, &desc, &s_swapChainInfo[s_swapChainCount].colorSwapChain);

	int length = 0;
	ovr_GetTextureSwapChainLength(s_data.native.session, s_swapChainInfo[s_swapChainCount].colorSwapChain, &length);
	if (OVR_SUCCESS(result))
	{
		for (int i = 0; i < length; ++i)
		{
			GLuint chainTexId;
			ovr_GetTextureSwapChainBufferGL(s_data.native.session, s_swapChainInfo[s_swapChainCount].colorSwapChain, i, &chainTexId);
			glBindTexture(target, chainTexId);

			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
	}

	desc.Format = OVR_FORMAT_D32_FLOAT;

	result = ovr_CreateTextureSwapChainGL(s_data.native.session, &desc, &s_swapChainInfo[s_swapChainCount].depthSwapChain);

	ovr_GetTextureSwapChainLength(s_data.native.session, s_swapChainInfo[s_swapChainCount].depthSwapChain, &length);

	if (OVR_SUCCESS(result))
	{
		for (int i = 0; i < length; ++i)
		{
			GLuint chainTexId;
			ovr_GetTextureSwapChainBufferGL(s_data.native.session, s_swapChainInfo[s_swapChainCount].depthSwapChain, i, &chainTexId);
			glBindTexture(target, chainTexId);

			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
	}
	glGenFramebuffers(1, &s_swapChainInfo[s_swapChainCount].swapChainFBO);
	s_swapChainInfo[s_swapChainCount].swapChainTextureTarget = target;

	rvrs_textureSwapChain swapChain = { .id = s_swapChainCount++ };

	return swapChain;
#else
	return (rvrs_textureSwapChain) { 0 };
#endif
}

int32_t rvrs_getTextureSwapChainLength(rvrs_textureSwapChain swapChain)
{
	int32_t length = 0;
#ifdef VRSYSTEM_OCULUS_WIN
	assert(swapChain.id >= 0);
	assert(swapChain.id < 2);

	ovr_GetTextureSwapChainLength(s_data.native.session, s_swapChainInfo[swapChain.id].colorSwapChain, &length);
#endif
	return length;
}

uint32_t rvrs_getTextureSwapChainBufferGL(rvrs_textureSwapChain swapChain, int32_t index)
{
	uint32_t chainTexId = 0;
#ifdef VRSYSTEM_OCULUS_WIN
	assert(swapChain.id >= 0);
	assert(swapChain.id < 2);

	ovr_GetTextureSwapChainBufferGL(s_data.native.session, s_swapChainInfo[swapChain.id].colorSwapChain, index, &chainTexId);
#endif
	return chainTexId;
}

GLuint rvrs_bindEyeFramebuffer(int32_t eye)
{
#ifdef VRSYSTEM_OCULUS_WIN
	assert(eye >= 0);
	assert(eye < 2);

	GLuint curColorTexId;
	GLuint curDepthTexId;
	{
		int curIndex;
		ovr_GetTextureSwapChainCurrentIndex(s_data.native.session, s_swapChainInfo[eye].colorSwapChain, &curIndex);
		ovr_GetTextureSwapChainBufferGL(s_data.native.session, s_swapChainInfo[eye].colorSwapChain, curIndex, &curColorTexId);
	}
	{
		int curIndex;
		ovr_GetTextureSwapChainCurrentIndex(s_data.native.session, s_swapChainInfo[eye].depthSwapChain, &curIndex);
		ovr_GetTextureSwapChainBufferGL(s_data.native.session, s_swapChainInfo[eye].depthSwapChain, curIndex, &curDepthTexId);
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_swapChainInfo[eye].swapChainFBO);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, s_swapChainInfo[eye].swapChainTextureTarget, curColorTexId, 0);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, s_swapChainInfo[eye].swapChainTextureTarget, curDepthTexId, 0);
	GLint colourEncoding = 0;
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &colourEncoding);

	return s_swapChainInfo[eye].swapChainFBO;
#else
	return 0;
#endif
}

void rvrs_getEyeWidthHeight(int32_t* width, int32_t* height)
{
	*width = s_data.eyeWidth;
	*height = s_data.eyeHeight;
}
