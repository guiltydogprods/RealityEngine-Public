#include "stdafx.h"
#include "system.h"

//#define ENABLE_CONSOLE_LOG

#ifdef RE_PLATFORM_WIN64
#include <windows.h>
#else
#include <time.h>
#endif

const char* s_storagePath = NULL;

#ifdef RE_PLATFORM_WIN64

//static rsys_perfTimer s_perfTimer;
static LARGE_INTEGER s_timerFreq = { .QuadPart = 0 };
#else
static uint64_t s_timerFreq = 0;
#endif

#ifdef ENABLE_CONSOLE_LOG
#define NUM_ROWS 32
#define MAX_COLS 512
char s_consoleBuffer[NUM_ROWS][MAX_COLS]; // [NUM_ROWS] ;
int32_t s_consoleWrite;
#endif

extern inline void rsys_print(const char* fmt, ...);
extern inline void rsys_assertFunc(const char* func, const char* file, int line, const char* e);
extern inline void rsys_assertMsgFunc(const char* func, const char* file, int line, const char* fmt, ...);

void rsys_initialize(const rsys_initParams* params)
{
#ifdef ENABLE_CONSOLE_LOG
	memset(s_consoleBuffer, 0, sizeof(s_consoleBuffer));
	s_consoleWrite = 0;
#endif
	if (params != NULL)
	{
		s_storagePath = params->storagePath;
	}
}

const char* rsys_getStoragePath(void)
{
	return s_storagePath;
}

void rsys_perfTimerInitialize()
{
#ifdef RE_PLATFORM_WIN64
	QueryPerformanceFrequency(&s_timerFreq);
#else

#endif
}


void rsys_perfTimerStart(rsys_perfTimer* timer)
{
	assert(timer != NULL);
#ifdef RE_PLATFORM_WIN64
	QueryPerformanceCounter(&timer->startTime);
	timer->splitTime = timer->startTime;
#else
	clock_gettime(CLOCK_MONOTONIC, &timer->startTime);
	timer->splitTime = timer->startTime;
#endif
}

uint64_t rsys_perfTimerGetSplit(rsys_perfTimer* timer)
{
#ifdef RE_PLATFORM_WIN64
	assert(s_timerFreq.QuadPart != 0);
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);

	LONGLONG elapsedUsecs = currentTime.QuadPart - timer->splitTime.QuadPart;
	timer->splitTime = currentTime;
	return (elapsedUsecs * 1000000) / s_timerFreq.QuadPart;
#else
	struct timespec currentTime;
	clock_gettime(CLOCK_MONOTONIC, &currentTime);
	uint64_t elapsedUsecs = (currentTime.tv_nsec - timer->splitTime.tv_nsec) / 1000;
	timer->splitTime = currentTime;

	return elapsedUsecs;
#endif
}

uint64_t rsys_perfTimerStop(rsys_perfTimer* timer)
{
	assert(timer != NULL);
#ifdef RE_PLATFORM_WIN64
	assert(s_timerFreq.QuadPart != 0);
	QueryPerformanceCounter(&timer->endTime);

	LONGLONG elapsedUsecs = timer->endTime.QuadPart - timer->splitTime.QuadPart;
	timer->elapsedUsecs = ((timer->endTime.QuadPart - timer->startTime.QuadPart) * 1000000) / s_timerFreq.QuadPart;
	return (elapsedUsecs * 1000000) / s_timerFreq.QuadPart;
#else

	clock_gettime(CLOCK_MONOTONIC, &timer->endTime);
	uint64_t elapsedUsecs = (timer->endTime.tv_nsec - timer->splitTime.tv_nsec) / 1000;
	timer->elapsedUsecs = (timer->endTime.tv_nsec - timer->startTime.tv_nsec) / 1000;

	return elapsedUsecs;
#endif
}

#ifdef ENABLE_CONSOLE_LOG
void rsys_consoleLog(const char* fmt, ...)
{
	char debugString[512];				//CLR - Should really calculate the size of the output string instead.

	va_list args;
	va_start(args, fmt);
#ifdef RE_PLATFORM_WIN64
	vsprintf_s(debugString, sizeof(debugString), fmt, args);
#else
	vsprintf(debugString, fmt, args);
#endif
	va_end(args);
	const char* srcPtr = debugString;
	int32_t byteIndex = 0;
	char ch = '\0';
	while ((ch = *srcPtr) != '\0')
	{
		char ch = (*srcPtr++);
		if (isprint(ch))
		{
			s_consoleBuffer[s_consoleWrite][byteIndex++] = ch;
		}
	}
	s_consoleBuffer[s_consoleWrite++][byteIndex] = '\0';
	s_consoleWrite = s_consoleWrite & (NUM_ROWS - 1);
}

void rsys_consoleClear()
{
	for (int32_t i = 0; i < s_consoleWrite; ++i)
	{
		strcpy(s_consoleBuffer[i], "");
	}
	s_consoleWrite = 0;
}
#endif

void rsys_consoleRender(mat4x4 worldMatrix, rgui_font font)
{
#ifdef ENABLE_CONSOLE_LOG
	float cursorPosX = -0.3f; // (0.028f * 1.0f);
	float cursorPosY = 0.35f; // (0.028f * 1.0f);

	int32_t startIndex = s_consoleWrite;

	for (int32_t i = 0; i < NUM_ROWS; ++i)
	{
		int32_t index = (startIndex + i) & (NUM_ROWS - 1);
		rgui_text3D(&(rgui_textDesc) {
			.rotation = { worldMatrix.xAxis, worldMatrix.yAxis, worldMatrix.zAxis },
			.position = worldMatrix.wAxis + worldMatrix.xAxis * cursorPosX + worldMatrix.yAxis * cursorPosY - worldMatrix.zAxis,
			.color = kDebugColorWhite,
			.scale = 24.0f,
			.text = s_consoleBuffer[index],
			.font = font,
			.layoutFlags = kAlignHLeft | kAlignVCenter, //.layoutFlags = kAlignHRight,
		});
		cursorPosY -= (0.028f * 1.0f);
	}
#endif // ENABLE_CONSOLE_LOG
}

