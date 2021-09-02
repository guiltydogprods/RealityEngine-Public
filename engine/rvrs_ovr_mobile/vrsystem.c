#include "stdafx.h"
#include "vrsystem.h"
#include "rgfx/renderer.h"
#include "rgfx/vertexPacker.h"
#include "stb/stretchy_buffer.h"

#define ION_PI (3.14159265359f)
#define ION_180_OVER_PI (180.0f / ION_PI)
#define ION_PI_OVER_180 (ION_PI / 180.0f)

#define DEGREES(x) ((x) * ION_180_OVER_PI)
#define RADIANS(x) ((x) * ION_PI_OVER_180)

ovrApp* s_appState = NULL;

static __attribute__((aligned(64))) rvrs_data s_data = { 0 };
static rvrs_trackingInfo s_tracking = { 0 };

typedef struct rvrs_textureSwapChainInfo
{
	ovrTextureSwapChain* swapChain;
}rvrs_textureSwapChainInfo;

static rvrs_textureSwapChainInfo s_swapChainInfo[2] = { NULL, NULL };
static int32_t s_swapChainCount = 0;

void rvrs_initialize(const rvrs_initParams* params)
{
	memset(&s_data, 0, sizeof(rvrs_data));
	memset(&s_tracking, 0, sizeof(rvrs_trackingInfo));
	memset(&s_swapChainInfo, 0, sizeof(rvrs_textureSwapChainInfo) * 2);
	s_swapChainCount = 0;

	s_appState = params->appState;
	int32_t initResult = vrapi_Initialize(&params->ovrInitParams);

	if (initResult != VRAPI_INITIALIZE_SUCCESS)
	{
		// If intialization failed, vrapi_* function calls will not be available.
		exit(0);
	}
	int32_t oversamplePercentage = params->oversamplePercentage > 100 ? params->oversamplePercentage : 100;
	int32_t eyeWidth = vrapi_GetSystemPropertyInt(&params->ovrInitParams.Java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH);
	int32_t eyeHeight = vrapi_GetSystemPropertyInt(&params->ovrInitParams.Java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT);
	int32_t scaledEyeWidth = (eyeWidth * oversamplePercentage) / 100;
	int32_t scaledEyeHeight = (eyeHeight * oversamplePercentage) / 100;
	s_data.eyeWidth = scaledEyeWidth;
	s_data.eyeHeight = scaledEyeHeight;
	s_data.oversamplePercentage = oversamplePercentage;

	ovrDeviceType deviceTypeOvr = (ovrDeviceType)vrapi_GetSystemPropertyInt(&params->ovrInitParams.Java, VRAPI_SYS_PROP_DEVICE_TYPE);
	if (deviceTypeOvr >= VRAPI_DEVICE_TYPE_OCULUSQUEST_START && deviceTypeOvr <= VRAPI_DEVICE_TYPE_OCULUSQUEST_END)
		s_data.deviceType = kVrDeviceTypeQuest;
	else if (deviceTypeOvr >= VRAPI_DEVICE_TYPE_OCULUSQUEST2_START && deviceTypeOvr <= VRAPI_DEVICE_TYPE_OCULUSQUEST2_END)
		s_data.deviceType = kVrDeviceTypeQuest2;

	int32_t numSupportedRefreshRates = vrapi_GetSystemPropertyInt(&params->ovrInitParams.Java, VRAPI_SYS_PROP_NUM_SUPPORTED_DISPLAY_REFRESH_RATES);
	assert(numSupportedRefreshRates < MAX_SUPPORTED_REFRESH_RATES);
	s_data.numSuportedRefreshRates = vrapi_GetSystemPropertyFloatArray(&params->ovrInitParams.Java, VRAPI_SYS_PROP_SUPPORTED_DISPLAY_REFRESH_RATES, s_data.supportedRefreshRates, MAX_SUPPORTED_REFRESH_RATES);

	s_data.bInitialized = true;
	rvrs_initializePlatformOvr(NULL);
}

void rvrs_terminate()
{
	rvrs_terminateOvrPlatform();
	vrapi_Shutdown();
	s_data.bInitialized = false;
}

bool rvrs_isInitialized()
{
	return s_data.bInitialized;
}

bool rvrs_isHmdMounted()
{
	int32_t mounted = vrapi_GetSystemStatusInt(&s_appState->Java, VRAPI_SYS_STATUS_MOUNTED);
	return mounted == 1;
}

bool rvrs_isTrackingControllers()
{
	if (s_tracking.handIDs[0] == 0 || s_tracking.handIDs[1] == 0)
	{
		return true;
	}
	return false;
}

bool rvrs_shouldPause()
{
	bool shouldPause = false;
	if (s_appState != NULL)
	{
		shouldPause = s_appState->ShouldPause;
	}
	return shouldPause;
}

bool rvrs_shouldQuit()
{
	return false;
}

rvrs_deviceType rvrs_getDeviceType(void)
{
	return s_data.deviceType;
}

float rvrs_getRefreshRate()
{
	float currentRefreshRate = 72.0f;
	if (s_appState)
	{
		currentRefreshRate = vrapi_GetSystemPropertyFloat(&s_appState->Java, VRAPI_SYS_PROP_DISPLAY_REFRESH_RATE);
	}
}

bool rvrs_setRefreshRate(float refreshRate)
{
	if (s_appState)
	{
		ovrResult err = -1;
		{
			if (refreshRate > 80.0f)
			{
				err = vrapi_SetExtraLatencyMode(s_appState->Ovr, VRAPI_EXTRA_LATENCY_MODE_ON);
				if (err != ovrSuccess)
				{
					rsys_print("Enable vrapi_SetExtraLatencyMode failed!\n");
				}
				if (refreshRate > 90.0f)
				{
//					vrapi_SetPropertyInt(&s_appState->Java, VRAPI_FOVEATION_LEVEL, 3);
//					vrapi_SetPropertyInt(&s_appState->Java, VRAPI_DYNAMIC_FOVEATION_ENABLED, false);
				}
				else
				{
//					vrapi_SetPropertyInt(&s_appState->Java, VRAPI_FOVEATION_LEVEL, 4);
//					vrapi_SetPropertyInt(&s_appState->Java, VRAPI_DYNAMIC_FOVEATION_ENABLED, false);
				}
			}
			else
			{
//				err = vrapi_SetExtraLatencyMode(s_appState->Ovr, VRAPI_EXTRA_LATENCY_MODE_OFF);
				err = vrapi_SetExtraLatencyMode(s_appState->Ovr, VRAPI_EXTRA_LATENCY_MODE_OFF);
				if (err != ovrSuccess)
				{
					rsys_print("Disable vrapi_SetExtraLatencyMode failed!\n");
				}
//				vrapi_SetPropertyInt(&s_appState->Java, VRAPI_FOVEATION_LEVEL, 2);
//				vrapi_SetPropertyInt(&s_appState->Java, VRAPI_DYNAMIC_FOVEATION_ENABLED, false);
			}
			err = vrapi_SetDisplayRefreshRate(s_appState->Ovr, refreshRate);
			if (err	== ovrSuccess)
			{
				rsys_print("Successfully SetDisplayRefreshRate to %.3f\n", refreshRate);
			}
			else
			{
				rsys_print("SetDisplayRefreshRate to %.3f failed.\n", refreshRate);
			}
//			s_appState->RefreshRate = vrapi_GetSystemPropertyFloat(&s_appState->Java, VRAPI_SYS_PROP_DISPLAY_REFRESH_RATE);
		}
		return true;
	}
	return false;
}

bool rvrs_setFoveationLevel(int32_t foveationLevel)
{
	if (s_appState)
	{
		assert(foveationLevel >= 0);
		assert(foveationLevel < 5);

		vrapi_SetPropertyInt(&s_appState->Java, VRAPI_FOVEATION_LEVEL, foveationLevel);		
		return true;
	}
	return false;
}

rvrs_eyeSize rvrs_getScaledEyeSize(int32_t newScale)
{
	assert(s_appState != NULL);
	assert(newScale >= 100);
	assert(newScale <= 200);

	int32_t eyeWidth = vrapi_GetSystemPropertyInt(&s_appState->Java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH);
	int32_t eyeHeight = vrapi_GetSystemPropertyInt(&s_appState->Java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT);

	int newWidth = (eyeWidth * newScale) / 100;
	int newHeight = (eyeHeight * newScale) / 100;

	s_data.oversamplePercentage = newScale;

	return (rvrs_eyeSize) {
		newWidth, newHeight
	};
}

int32_t rvrs_getSupportedRefreshRates(const float** ppSupportedRefreshRates)
{
	*ppSupportedRefreshRates = s_data.supportedRefreshRates;
	return s_data.numSuportedRefreshRates;
}



void rvrs_updateTracking(const rvrs_appInfo *appInfo)
{
	ovrApp* appState = (ovrApp*)appInfo->appState;
	assert(appState != NULL);
	// Get the HMD pose, predicted for the middle of the time period during which
	// the new eye images will be displayed. The number of frames predicted ahead
	// depends on the pipeline depth of the engine and the synthesis rate.
	// The better the prediction, the less black will be pulled in at the edges.
	const double predictedDisplayTime = vrapi_GetPredictedDisplayTime(appState->Ovr, appState->FrameIndex);
	s_tracking.hmdTracking = vrapi_GetPredictedTracking2(appState->Ovr, predictedDisplayTime);
	ovrTracking contorllerTracking[2];
	for (int32_t i = 0; i < 2; ++i)
	{
		vrapi_GetInputTrackingState(appState->Ovr, appState->ControllerIDs[i], predictedDisplayTime, &s_tracking.controllerTracking[i]);
		memcpy(&s_tracking.controllerState[i], &appState->ControllerState[i], sizeof(ovrInputStateTrackedRemote));
		s_tracking.handIDs[i] = appState->HandIDs[i];
		if (appState->HandIDs[i] != 0)
		{
			memcpy(&s_tracking.handState[i], &appState->HandState[i], sizeof(ovrInputStateHand));
			s_tracking.handPose[i].Header.Version = ovrHandVersion_1;
			ovrResult r = vrapi_GetHandPose(appState->Ovr, appState->HandIDs[i], predictedDisplayTime, &s_tracking.handPose[i].Header);
			if (r != ovrSuccess) {
//				rsys_consoleLog("Hand %d - failed to get hand pose", i);
//				return false;
			}
			else 
			{
//				rsys_consoleLog("Hand %d - got hand pose", i);
/*
				SampleConfiguration.HandScaleFactor = RealHandPose.HandScale;
				{
					VRMenuObject* scaleDisplay = IsLeftHand() ? HandScaleDisplayL : HandScaleDisplayR;
					std::ostringstream ss;
					ss << (IsLeftHand() ? "size L " : "size R ") << RealHandPose.HandScale;
					scaleDisplay->SetText(ss.str().c_str());
				}

				/// Get the root pose from the API
				HandPose = RealHandPose.RootPose;
				/// Pointer poses
				PointerPose = InputStateHand.PointerPose;
				/// update based on hand pose
				HandModel.Update(RealHandPose);
				UpdateSkeleton(HandPose);
*/
			}
			ovrHandedness hand = (ovrHandedness)(i + 1);
			if (s_tracking.handSkeleton[i].Header.Version != ovrHandVersion_1)
			{
				s_tracking.handSkeleton[i].Header.Version = ovrHandVersion_1;
				r = vrapi_GetHandSkeleton(appState->Ovr, hand, &s_tracking.handSkeleton[i].Header);
				if (r != ovrSuccess)
				{
//					s_tracking.handSkeleton[i].Header.Version = (ovrHandVersion)0;
				}
				else
				{
					if (i == 0)
					{
//						rsys_consoleClear();
						for (uint32_t cap = 0; cap < 16/*s_tracking.handSkeleton[i].NumCapsules*/; ++cap)
						{
							uint32_t boneIndex = s_tracking.handSkeleton[i].Capsules[cap].BoneIndex;
							float p1x = s_tracking.handSkeleton[i].Capsules[cap].Points[0].x;
							float p1y = s_tracking.handSkeleton[i].Capsules[cap].Points[0].y;
							float p1z = s_tracking.handSkeleton[i].Capsules[cap].Points[0].z;
							float p2x = s_tracking.handSkeleton[i].Capsules[cap].Points[1].x;
							float p2y = s_tracking.handSkeleton[i].Capsules[cap].Points[1].y;
							float p2z = s_tracking.handSkeleton[i].Capsules[cap].Points[1].z;
							float radius = s_tracking.handSkeleton[i].Capsules[cap].Radius;
							vec4 p1 = { p1x, p1y, p1z, 1.0f };
							vec4 p2 = { p2x, p2y, p2z, 1.0f };
							float height = vec4_length(p1 - p2).x;
							vec4 capsuleOffset = (vec4){ i == 0 ? 1.0f : -1.0f * height / 2.0f, 0.0f, 0.0f, 0.0f } + p1;

							/// Add the geometry transform
							mat4x4 rot = mat4x4_rotate(RADIANS(90.0f), (vec4) { 0.0f, 1.0f, 0.0f, 0.0f });
							const mat4x4 capsuleMatrix = mat4x4_mul(mat4x4_translate(capsuleOffset), rot);

							rgfx_createDebugCapsule(radius, height, 10, 7);
							/// Add the geometry transform
//							const Matrix4f capsuleMatrix =
//								Matrix4f::Translation(capsuleOffset) /// offset cylinder back to the bone
//								* Matrix4f::RotationY(OVR::DegreeToRad(90.0f)); /// restore cylinder orientation to be along the X axis

//							rsys_consoleLog("bone %d: %.3f (h), %.3f (r)", boneIndex, height, radius); // % .3f, % .3f, % .3f % .3f, % .3f, % .3f (% .3f)", boneIndex, p1x, p1y, p1z, p2x, p2y, p2z, radius);
						}
					}
				}
			}

			if (s_tracking.handMesh[i].Header.Version != ovrHandVersion_1)
			{
				s_tracking.handMesh[i].Header.Version = ovrHandVersion_1;
				r = vrapi_GetHandMesh(appState->Ovr, hand, &s_tracking.handMesh[i].Header);
				if (r != ovrSuccess)
				{
					//				rsys_consoleLog("Hand %d - failed to get mesh", i);
									//				return false;
				}
				else
				{
					//				rsys_consoleLog("Hand %d - got mesh", i);
					if (s_data.handRenderData.rend[i].id == 0)
					{
						rvrs_initializeHandMesh(i, &s_tracking.handSkeleton[i], &s_tracking.handMesh[i]);
					}
				}
			}
		}
	}
	appState->DisplayTime = predictedDisplayTime;
	s_data.native.isMounted = vrapi_GetSystemStatusInt(&appState->Java, VRAPI_SYS_STATUS_MOUNTED) != 0;

#ifndef DISABLE_AVATAR
	rvrs_processOvrMessages();
#endif
}

mat4x4 rvrs_getHeadMatrix()
{
	float px = s_tracking.hmdTracking.HeadPose.Pose.Position.x;
	float py = s_tracking.hmdTracking.HeadPose.Pose.Position.y;
	float pz = s_tracking.hmdTracking.HeadPose.Pose.Position.z;

	float qx = s_tracking.hmdTracking.HeadPose.Pose.Orientation.x;
	float qy = s_tracking.hmdTracking.HeadPose.Pose.Orientation.y;
	float qz = s_tracking.hmdTracking.HeadPose.Pose.Orientation.z;
	float qw = s_tracking.hmdTracking.HeadPose.Pose.Orientation.w;

	return mat4x4_create((vec4) { qx, qy, qz, qw }, (vec4) { px, py, pz, 1.0f });
}

int32_t rvrs_getHeadPose(rvrs_rigidBodyPose* pose)
{
	memcpy(pose, &s_tracking.hmdTracking.HeadPose, sizeof(rvrs_rigidBodyPose));
	return 0;
}

mat4x4 rvrs_getViewMatrixForEye(int32_t index)
{
	assert(index >= 0);
	assert(index < 2);
	mat4x4 viewMatrix;
	viewMatrix.xAxis = (vec4){ s_tracking.hmdTracking.Eye[index].ViewMatrix.M[0][0], s_tracking.hmdTracking.Eye[index].ViewMatrix.M[1][0], s_tracking.hmdTracking.Eye[index].ViewMatrix.M[2][0], s_tracking.hmdTracking.Eye[index].ViewMatrix.M[3][0] };
	viewMatrix.yAxis = (vec4){ s_tracking.hmdTracking.Eye[index].ViewMatrix.M[0][1], s_tracking.hmdTracking.Eye[index].ViewMatrix.M[1][1], s_tracking.hmdTracking.Eye[index].ViewMatrix.M[2][1], s_tracking.hmdTracking.Eye[index].ViewMatrix.M[3][1] };
	viewMatrix.zAxis = (vec4){ s_tracking.hmdTracking.Eye[index].ViewMatrix.M[0][2], s_tracking.hmdTracking.Eye[index].ViewMatrix.M[1][2], s_tracking.hmdTracking.Eye[index].ViewMatrix.M[2][2], s_tracking.hmdTracking.Eye[index].ViewMatrix.M[3][2] };
	viewMatrix.wAxis = (vec4){ s_tracking.hmdTracking.Eye[index].ViewMatrix.M[0][3], s_tracking.hmdTracking.Eye[index].ViewMatrix.M[1][3], s_tracking.hmdTracking.Eye[index].ViewMatrix.M[2][3], s_tracking.hmdTracking.Eye[index].ViewMatrix.M[3][3] };

	return viewMatrix;
}

mat4x4 rvrs_getProjectionMatrixForEye(int32_t index)
{
	assert(index >= 0);
	assert(index < 2);

	mat4x4 projectionMatrix;
	projectionMatrix.xAxis = (vec4){ s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[0][0], s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[1][0], s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[2][0], s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[3][0] };
	projectionMatrix.yAxis = (vec4){ s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[0][1], s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[1][1], s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[2][1], s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[3][1] };
	projectionMatrix.zAxis = (vec4){ s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[0][2], s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[1][2], s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[2][2], s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[3][2] };
	projectionMatrix.wAxis = (vec4){ s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[0][3], s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[1][3], s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[2][3], s_tracking.hmdTracking.Eye[index].ProjectionMatrix.M[3][3] };

	return projectionMatrix;
}

mat4x4 rvrs_getTrackedControllerMatrix(int32_t index)
{
	assert(index >= 0);
	assert(index < 2);

	float px = s_tracking.controllerTracking[index].HeadPose.Pose.Position.x;
	float py = s_tracking.controllerTracking[index].HeadPose.Pose.Position.y;
	float pz = s_tracking.controllerTracking[index].HeadPose.Pose.Position.z;

	float qx = s_tracking.controllerTracking[index].HeadPose.Pose.Orientation.x;
	float qy = s_tracking.controllerTracking[index].HeadPose.Pose.Orientation.y;
	float qz = s_tracking.controllerTracking[index].HeadPose.Pose.Orientation.z;
	float qw = s_tracking.controllerTracking[index].HeadPose.Pose.Orientation.w;

	return mat4x4_create((vec4) { qx, qy, qz, qw }, (vec4) { px, py, pz, 1.0f });
}

int32_t rvrs_getTrackedControllerPose(int32_t index, rvrs_rigidBodyPose* pose)
{
	if (index >= 0 && index < 2)
	{
		memcpy(pose, &s_tracking.controllerTracking[index].HeadPose, sizeof(rvrs_rigidBodyPose));
		return 0;
	}
	else
	{
		return -1;
	}
}

int32_t rvrs_getControllerState(int32_t index, rvrs_controllerState* controllerState)
{
	assert(index >= 0);
	assert(index < 2);
//	if (s_tracking.controllerTracking[index].Status == 0x)
	if (s_tracking.controllerState[index].Header.ControllerType == ovrControllerType_TrackedRemote)
	{
		controllerState->buttons = s_tracking.controllerState[index].Buttons;
		controllerState->buttonsPressed = controllerState->buttons & ~s_data.prevButtons[index];
		controllerState->touches = s_tracking.controllerState[index].Touches;
		controllerState->trigger = s_tracking.controllerState[index].IndexTrigger;
		controllerState->grip = s_tracking.controllerState[index].GripTrigger;
		controllerState->joystick = (vec2){ s_tracking.controllerState[index].Joystick.x, s_tracking.controllerState[index].Joystick.y };
		controllerState->JoystickNoDeadZone = (vec2){ s_tracking.controllerState[index].JoystickNoDeadZone.x, s_tracking.controllerState[index].JoystickNoDeadZone.y };
		s_data.prevButtons[index] = controllerState->buttons;
		return 0;
	}
	return -1;
}

void rvrs_setControllerHapticVibration(int32_t index, const float intensity)
{
	assert(index >= 0);
	assert(index < 2);
	if (s_appState != NULL)
	{
		vrapi_SetHapticVibrationSimple(s_appState->Ovr, s_appState->ControllerIDs[index], intensity);
	}
}


ovrPosef* rvrs_getHandPose(int32_t index)
{
	assert(index >= 0);
	assert(index < 2);

	return &s_tracking.handPose[index].RootPose;
}

ovrQuatf* rvrs_getHandBoneRotations(int32_t index)
{
	assert(index >= 0);
	assert(index < 2);

	return &s_tracking.handPose[index].BoneRotations;
}

ovrHandSkeleton* rvrs_getHandSkeleton(int32_t index)
{
	assert(index >= 0);
	assert(index < 2);

	return &s_tracking.handSkeleton[index];
}

mat4x4 rvrs_getHandScale(int32_t index)
{
	assert(index >= 0);
	assert(index < 2);

	return mat4x4_scale(s_tracking.handPose[index].HandScale);
}

rvrs_handRenderData* rvrs_getHandRenderData()
{
	return &s_data.handRenderData;
}


void rvrs_beginFrame()
{
	ovrApp* appState = s_appState;
	vrapi_WaitFrame(appState->Ovr, appState->FrameIndex);
	vrapi_BeginFrame(appState->Ovr, appState->FrameIndex);
}

void rvrs_submit()
{
	ovrLayerProjection2 worldLayer = vrapi_DefaultLayerProjection2();
	worldLayer.HeadPose = s_tracking.hmdTracking.HeadPose;
//	worldLayer.Header.ColorScale = (ovrVector4f){ 1.1f, 1.1f, 1.1f, 1.0f };
	for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
	{
		rgfx_eyeFbInfo* frameBuffer = rgfx_getEyeFrameBuffer(eye); // &s_rendererData.eyeFbInfo[s_rendererData.numEyeBuffers == 1 ? 0 : eye];
		worldLayer.Textures[eye].ColorSwapChain = s_swapChainInfo[frameBuffer->colorTextureSwapChain.id].swapChain;
		worldLayer.Textures[eye].SwapChainIndex = frameBuffer->textureSwapChainIndex;
		worldLayer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&s_tracking.hmdTracking.Eye[eye].ProjectionMatrix);

		//Advance swapcahin.
		if (eye == 1) {
			frameBuffer->textureSwapChainIndex = (frameBuffer->textureSwapChainIndex + 1) % frameBuffer->textureSwapChainLength;
		}
	}
	worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	const ovrLayerHeader2* layers[] =
	{
		&worldLayer.Header
	};

	ovrApp* appState = s_appState;
	ovrSubmitFrameDescription2 frameDesc = { 0 };
	frameDesc.Flags = 0;
	frameDesc.SwapInterval = appState->SwapInterval;
	frameDesc.FrameIndex = appState->FrameIndex;
	frameDesc.DisplayTime = appState->DisplayTime;
	frameDesc.LayerCount = 1;
	frameDesc.Layers = layers;

	// Hand over the eye images to the time warp.
	vrapi_SubmitFrame2(appState->Ovr, &frameDesc);

}

rvrs_textureSwapChain rvrs_createTextureSwapChain(rvrs_textureType type, rgfx_format format, int32_t width, int32_t height, int32_t levels, int32_t bufferCount)
{
#if VRSYSTEM_OCULUS_MOBILE
	ovrTextureType nativeType;
	switch (type)
	{
	case kVrTextureType2D:
		nativeType = VRAPI_TEXTURE_TYPE_2D;
		break;
	case kVrTextureType2DArray:
		nativeType = VRAPI_TEXTURE_TYPE_2D_ARRAY;
		break;
	case kVrTextureType2DCube:
		nativeType = VRAPI_TEXTURE_TYPE_CUBE;
		break;
	default:
		nativeType = VRAPI_TEXTURE_TYPE_2D;
	}

	GLenum nativeFormat = rgfx_getNativeTextureFormat(format);
	assert(s_swapChainCount <= 2);
	s_swapChainInfo[s_swapChainCount].swapChain = vrapi_CreateTextureSwapChain3(nativeType, nativeFormat, width, height, levels, bufferCount);

	rvrs_textureSwapChain swapChain = { .id = s_swapChainCount++ };

	return swapChain;
#else
	return (rvrs_textureSwapChain) { 0 }
#endif
}

void rvrs_destroyTextureSwapChain(rvrs_textureSwapChain swapChain)
{
	assert(swapChain.id >= 0);
	assert(swapChain.id < 2);

	vrapi_DestroyTextureSwapChain(s_swapChainInfo[swapChain.id].swapChain);
	s_swapChainCount = 0;
}

int32_t rvrs_getTextureSwapChainLength(rvrs_textureSwapChain swapChain)
{
	assert(swapChain.id >= 0);
	assert(swapChain.id < 2);

	return vrapi_GetTextureSwapChainLength(s_swapChainInfo[swapChain.id].swapChain);
}

uint32_t rvrs_getTextureSwapChainBufferGL(rvrs_textureSwapChain swapChain, int32_t index)
{
	assert(swapChain.id >= 0);
	assert(swapChain.id < 2);

	return vrapi_GetTextureSwapChainHandle(s_swapChainInfo[swapChain.id].swapChain, index);
}

void rvrs_getEyeWidthHeight(int32_t* width, int32_t* height)
{
	*width = s_data.eyeWidth;
	*height = s_data.eyeHeight;
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
		float px = s_tracking.hmdTracking.HeadPose.Pose.Position.x;
		float py = s_tracking.hmdTracking.HeadPose.Pose.Position.y;
		float pz = s_tracking.hmdTracking.HeadPose.Pose.Position.z;
		s_data.avatarFrameParams.eyePos = (vec4){ px, py, pz, 1.0f };
	}
	if (vec4_isZero(s_data.avatarFrameParams.eyeRot)) {
		float rx = s_tracking.hmdTracking.HeadPose.Pose.Orientation.x;
		float ry = s_tracking.hmdTracking.HeadPose.Pose.Orientation.y;
		float rz = s_tracking.hmdTracking.HeadPose.Pose.Orientation.z;
		float rw = s_tracking.hmdTracking.HeadPose.Pose.Orientation.w;
		s_data.avatarFrameParams.eyeRot = (vec4){ rx, ry, rz, rw };
	}
#if 0
	if (vec4_isZero(s_data.avatarFrameParams.hands[0].position)) {
		float px = s_tracking.controllerTracking[0].ThePose.Position.x;
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
	rvrs_renderAvatarOvr(eye, &s_data.avatarFrameParams);
#endif
}

mat4x4 ConvertPose(ovrPosef* pose);

void rvrs_initializeHandMesh(int32_t hand, ovrHandSkeleton* skeleton, ovrHandMesh* mesh)
{
	if (s_data.handRenderData.pipeline.id == 0) {
		s_data.handRenderData.pipeline = rgfx_createPipeline(&(rgfx_pipelineDesc) {
			.vertexShader = rgfx_loadShader("shaders/hands.vert", kVertexShader, 0),
			.fragmentShader = rgfx_loadShader("shaders/hands.frag", kFragmentShader, 0),
		});
	}
	uint32_t fragmentProgram = rgfx_getPipelineProgram(s_data.handRenderData.pipeline, kFragmentShader);
	s_data.handRenderData.uniformIndexHand = glGetProgramResourceLocation(fragmentProgram, GL_UNIFORM, "Hand");

	if (s_data.handRenderData.vertexBuffer.id == 0)											
	{
		uint32_t vertexStride = 0;
		rgfx_vertexBuffer vertexBuffer = { 0 };
		{
			rgfx_vertexElement vertexStreamElements[] =
			{
				{ kFloat, 3, false },					//Position
				{ kInt2_10_10_10_Rev, 4, true },		//Normal
				{ kFloat, 2, false },					//UV
				{ kUnsignedByte, 4, false },			//Bone Indices
				{ kUnsignedByte, 4, true },				//Bone Weights
			};
			int32_t numElements = STATIC_ARRAY_SIZE(vertexStreamElements);
			rgfx_vertexFormat vertexFormat = rgfx_registerVertexFormat(numElements, vertexStreamElements);
			vertexStride = rgfx_getVertexFormatInfo(vertexFormat)->stride;
			vertexBuffer = rgfx_createVertexBuffer(&(rgfx_vertexBufferDesc) {
				.format = vertexFormat,
					.capacity = vertexStride * 64 * 1024,
					.indexBuffer = rgfx_createBuffer(&(rgfx_bufferDesc) {
					.type = kIndexBuffer,
						.capacity = 64 * 1024 * sizeof(uint16_t),
						.stride = sizeof(uint16_t),
						.flags = kMapWriteBit | kDynamicStorageBit,
				})
			});
		}
		s_data.handRenderData.vertexBuffer = vertexBuffer;
	}
	if (s_data.handRenderData.vertexParams.id == 0)
	{
		s_data.handRenderData.vertexParams = rgfx_createBuffer(&(rgfx_bufferDesc) {
			.capacity = sizeof(rvrs_handsVertParamsOvr) * 2,	//2 hands
			.stride = sizeof(rvrs_handsVertParamsOvr),
			.flags = kMapWriteBit
		});
	}
	rgfx_bufferInfo* bufferInfo = rgfx_getBufferInfo(rgfx_getVertexBufferBuffer(s_data.handRenderData.vertexBuffer, kVertexBuffer));
	uint32_t vertexStride = bufferInfo->stride;

	int32_t indexCount = mesh->NumIndices;
	uint32_t firstIndex = rgfx_writeIndexData(s_data.handRenderData.vertexBuffer, sizeof(uint16_t) * indexCount, (uint8_t*)mesh->Indices, sizeof(ovrVertexIndex));

	uint32_t vertexCount = mesh->NumVertices;
	VertexPacker packer;
	void* memory = malloc(vertexStride * vertexCount);
	memset(memory, 0, vertexStride);
	packer.base = packer.ptr = memory;

	for (uint32_t i = 0; i < vertexCount; ++i)
	{
		packPosition(&packer, mesh->VertexPositions[i].x, mesh->VertexPositions[i].y, mesh->VertexPositions[i].z);
		packFrenet(&packer, mesh->VertexNormals[i].x, mesh->VertexNormals[i].y, mesh->VertexNormals[i].z);
		packUV(&packer, mesh->VertexUV0[i].x, 1.0f - mesh->VertexUV0[i].y);
		packJointIndices4x16s(&packer, mesh->BlendIndices[i].x, mesh->BlendIndices[i].y, mesh->BlendIndices[i].z, mesh->BlendIndices[i].w);
		packJointWeights4(&packer, mesh->BlendWeights[i].x, mesh->BlendWeights[i].y, mesh->BlendWeights[i].z, mesh->BlendWeights[i].w);
	}
	uint32_t firstVertex = rgfx_writeVertexData(s_data.handRenderData.vertexBuffer, vertexStride * vertexCount, packer.base);
	free(memory);

	//	.data = (isV2 ? assetDataV2->indexBuffer : assetData->indexBuffer),

		// now compute the inverse bind pose
	int32_t bindPoseIndex = hand;
	assert(bindPoseIndex >= 0);
	assert(bindPoseIndex < 2);
#define MAX_JOINTS 26
	if (s_data.handRenderData.inverseBindPoseArrays[bindPoseIndex] == NULL)
	{
		assert(skeleton->NumBones <= MAX_JOINTS);

		const uint32_t maxJoints = min(skeleton->NumBones, MAX_JOINTS);
		mat4x4* inverseBindPoseArray = NULL;
		sb_add(inverseBindPoseArray, maxJoints);
		s_data.handRenderData.inverseBindPoseArrays[bindPoseIndex] = inverseBindPoseArray;
		
		mat4x4 worldPose[MAX_JOINTS + 1];
		const int16_t* jointParents = skeleton->BoneParentIndices;
		assert(skeleton->NumBones < MAX_JOINTS);
		for (uint32_t i = 0; i < skeleton->NumBones; i++)
		{
			mat4x4 local = ConvertPose(&skeleton->BonePoses[i]); // &jointTransforms[i]);
			const int16_t parentIndex = jointParents[i];
			worldPose[i] = (parentIndex < 0) ? local : mat4x4_mul(worldPose[parentIndex], local);

			inverseBindPoseArray[i] = mat4x4_orthoInverse(worldPose[i]);
		}
/*
		for (uint32_t i = 0; i < maxJoints; i++)
		{
			mat4x4 joint = inverseBindPoseArray[i];
			inverseBindPoseArray[i] = mat4x4_orthoInverse(joint);
		}
*/
	}

	rgfx_renderable rend = rgfx_addRenderable(&(rgfx_renderableInfo) {
		.firstVertex = firstVertex,
		.firstIndex = firstIndex * 2,
		.indexCount = indexCount,
		.materialIndex = -1,
		.pipeline = s_data.handRenderData.pipeline,
		.vertexBuffer = s_data.handRenderData.vertexBuffer,
	});
	s_data.handRenderData.rend[hand] = rend;
}
