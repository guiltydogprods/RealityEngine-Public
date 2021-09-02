#ifndef _OCULUSSPATIALIZERSETTINGS_H_
#define _OCULUSSPATIALIZERSETTINGS_H_

#include "OVR_Audio_DynamicRoom.h"

#ifndef OVRA_FMOD_EXPORT
#  ifdef _WIN32
#    define OVRA_FMOD_EXPORT __declspec( dllexport )
#  elif defined __APPLE__
#    define OVRA_FMOD_EXPORT
#  elif defined __linux__
#    define OVRA_FMOD_EXPORT __attribute__((visibility("default")))
#  else
#    error not implemented
#  endif
#endif

#include <stdint.h>

#ifndef OVR_RESULT_DEFINED
#define OVR_RESULT_DEFINED
typedef int32_t ovrResult;
#endif

// OSP
enum
{
    OSP_PARAM_INDEX_SOUND_POSITION,           // FMOD_DSP_PARAMETER_3DATTRIBUTES - both absolute and relative to camera

    OSP_PARAM_INDEX_REFLECTIONS_ENABLED,      // bool
    OSP_PARAM_INDEX_ATTENUATION_ENABLED,      // bool
    OSP_PARAM_INDEX_SOURCE_VOLUMETRIC_RADIUS, // float
    OSP_PARAM_INDEX_SOURCE_RANGE_MIN,         // float
    OSP_PARAM_INDEX_SOURCE_RANGE_MAX,         // float
    OSP_PARAM_INDEX_SOURCE_OVERALL_GAIN,      // FMOD_DSP_PARAMETER_OVERALLGAIN - used for prioritization
    OSP_PARAM_INDEX_REVERB_SEND_LEVEL_DB,     // float

    OSP_PARAM_INDEX_OSP_NUM_PARAMETERS
};

// OSP Ambisonics
enum
{
    OSP_PARAM_INDEX_AMBISONIC_SOUND_POSITION,         // FMOD_DSP_PARAMETER_3DATTRIBUTES - both absolute and relative to camera
    OSP_PARAM_INDEX_AMBISONIC_OVERALL_GAIN,           // FMOD_DSP_PARAMETER_OVERALLGAIN - used for prioritization

    OSP_PARAM_INDEX_OSPAMBISONIC_NUM_PARAMETERS
};

// OSP Globals
enum
{
    OSP_GLOBALS_PARAM_INDEX_REFLECTION_ENGINE_ENABLED, // bool
    OSP_GLOBALS_PARAM_INDEX_REVERB_ENABLED,            // bool
    OSP_GLOBALS_PARAM_INDEX_GLOBAL_SCALE,              // float
    OSP_GLOBALS_PARAM_INDEX_BYPASS_ALL,                // bypass
    OSP_GLOBALS_PARAM_INDEX_ROOM_WIDTH,                // float
    OSP_GLOBALS_PARAM_INDEX_ROOM_HEIGHT,               // float
    OSP_GLOBALS_PARAM_INDEX_ROOM_LENGTH,               // float
    OSP_GLOBALS_PARAM_INDEX_REF_LEFT,                  // float
    OSP_GLOBALS_PARAM_INDEX_REF_RIGHT,                 // float
    OSP_GLOBALS_PARAM_INDEX_REF_UP,                    // float
    OSP_GLOBALS_PARAM_INDEX_REF_DOWN,                  // float
    OSP_GLOBALS_PARAM_INDEX_REF_NEAR,                  // float
    OSP_GLOBALS_PARAM_INDEX_REF_FAR,                   // float
    OSP_GLOBALS_PARAM_INDEX_REVERB_RANGE_MIN,          // float
    OSP_GLOBALS_PARAM_INDEX_REVERB_RANGE_MAX,          // float
    OSP_GLOBALS_PARAM_INDEX_REVERB_OVERALL_GAIN,       // FMOD_DSP_PARAMETER_OVERALLGAIN - used for prioritization
    OSP_GLOBALS_PARAM_INDEX_REVERB_WET_LEVEL_DB,       // float
    OSP_GLOBALS_PARAM_INDEX_VOICE_LIMIT,               // int

    OSP_GLOBALS_PARAM_INDEX_NUM_PARAMETERS
};

//extern "C"
//{

OVRA_FMOD_EXPORT ovrResult OSP_FMOD_Initialize(int SampleRate, int BufferLength);
OVRA_FMOD_EXPORT ovrResult OSP_FMOD_SetProfilerEnabled(bool enabled);

OVRA_FMOD_EXPORT ovrResult OSP_FMOD_AssignRayCastCallback(OVRA_RAYCAST_CALLBACK callback, void* pctx);
OVRA_FMOD_EXPORT ovrResult OSP_FMOD_SetDynamicRoomRaysPerSecond(int RaysPerSecond);
OVRA_FMOD_EXPORT ovrResult OSP_FMOD_SetDynamicRoomInterpSpeed(float InterpSpeed);
OVRA_FMOD_EXPORT ovrResult OSP_FMOD_SetDynamicRoomMaxWallDistance(float MaxWallDistance);
OVRA_FMOD_EXPORT ovrResult OSP_FMOD_SetDynamicRoomRaysRayCacheSize(int RayCacheSize);
OVRA_FMOD_EXPORT ovrResult OSP_FMOD_UpdateRoomModel(float wetLevel);
OVRA_FMOD_EXPORT ovrResult OSP_FMOD_GetRoomDimensions(float roomDimensions[], float reflectionsCoefs[], ovrAudioVector3f* position);
OVRA_FMOD_EXPORT ovrResult OSP_FMOD_GetRaycastHits(ovrAudioVector3f points[], ovrAudioVector3f normals[], int length);

//}


#endif // _OCULUSSPATIALIZERSETTINGS_H_
