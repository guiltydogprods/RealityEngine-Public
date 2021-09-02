#pragma once

#ifdef RE_PLATFORM_WIN64
#include <GL/glew.h>
#include <GL/wglew.h>
#else
#include <jni.h>
#endif

typedef struct rengApi rengApi;
typedef struct rsysApi rsysApi;
typedef struct rresApi rresApi;
typedef struct rgfxApi rgfxApi;
typedef struct raudApi raudApi;
typedef struct rvrsApi rvrsApi;

//typedef struct ovrTracking2_ ovrTracking2;
typedef struct rvrs_trackingInfo rvrs_trackingInfo;

static inline int32_t numDigits(uint32_t value)
{
//	if (value < 0)
//		return -1;
	if (value < 10)
		return 1;
	if (value < 100)
		return 2;
	if (value < 1000)
		return 3;
	if (value < 10000)
		return 4;
	if (value < 100000)
		return 5;
	if (value < 1000000)
		return 6;
	if (value < 10000000)
		return 7;
	if (value < 100000000)
		return 8;
	if (value < 1000000000)
		return 9;
	return 10;
};

#ifdef RE_PLATFORM_WIN64
//__declspec(dllexport) void game_initialize(const int32_t width, const int32_t height, const rsysApi* rsys, const rresApi* rres, const rgfxApi* rgfx, const raudApi* raud);
__declspec(dllexport) void game_initialize(const int32_t width, const int32_t height, const rengApi* reng);
__declspec(dllexport) void game_terminate();
__declspec(dllexport) bool game_shouldQuit(bool systemQuit);
__declspec(dllexport) void game_update(float dt, rsys_input systemInput);
__declspec(dllexport) void game_render();
#else
JNIEXPORT void game_initialize(const int32_t width, const int32_t height, const rengApi* reng);
JNIEXPORT void game_terminate();
JNIEXPORT bool game_shouldQuit(bool systemQuit);
JNIEXPORT void game_update(float dt, rsys_input systemInput);
JNIEXPORT void game_render();
#endif

void game_initialize_vr_platform();
