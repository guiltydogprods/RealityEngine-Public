#pragma once
#include "system.h"
#include "resource.h"
#include "rgfx/renderer.h"
#include "rgfx/debugRender.h"
#include "gui.h"
#include "audio.h"
#include "vrsystem.h"

#define STATIC_ARRAY_SIZE(_a) (sizeof(_a) / sizeof(_a[0]))

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef clamp
#define clamp(a, b, c)		(min(max((a), (b)), (c)))
#endif

typedef struct rengApi
{
	rsysApi rsys;
	rresApi rres;
	rgfxApi rgfx;
	rguiApi rgui;
	raudApi raud;
	rvrsApi rvrs;
}rengApi;
