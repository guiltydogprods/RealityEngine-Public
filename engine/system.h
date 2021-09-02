#pragma once
#include "file.h"
#include "math/mat4x4.h"
#include "font.h"

//#define ENABLE_CONSOLE_LOG

typedef struct rsys_initParams
{
	const char* storagePath;
}rsys_initParams;

#ifdef RE_PLATFORM_WIN64
#include <windows.h>

typedef struct rsys_perfTimer {
	LARGE_INTEGER startTime;
	LARGE_INTEGER endTime;
	LARGE_INTEGER splitTime;
	uint64_t elapsedUsecs;
}rsys_perfTimer;

#else
#include <time.h>
typedef struct rsys_perfTimer {
	struct timespec startTime;
	struct timespec endTime;
	struct timespec splitTime;
	uint64_t elapsedUsecs;
}rsys_perfTimer;

#endif

typedef struct rsys_input {
	uint32_t keyPresses;
	uint32_t mousePresses;
	float mousePrevPosX;
	float mousePrevPosY;
	float mousePosX;
	float mousePosY;
	float mouseMoveX;
	float mouseMoveY;
	float mousePosNdcX;
	float mousePosNdcY;
	float mouseMotionValid;
}rsys_input;

void rsys_initialize(const rsys_initParams* params);
const char* rsys_getStoragePath(void);
void rsys_consoleLog(const char* fmt, ...);
void rsys_consoleRender(mat4x4 worldMatrix, rgui_font font);

void rsys_perfTimerInitialize();
void rsys_perfTimerStart(rsys_perfTimer* timer);
uint64_t rsys_perfTimerStop(rsys_perfTimer* timer);
uint64_t rsys_perfTimerGetSplit(rsys_perfTimer* timer);

inline void rsys_print(const char* fmt, ...)
{
	char debugString[4096];				//CLR - Should really calculate the size of the output string instead.

	va_list args;
	va_start(args, fmt);
#ifdef RE_PLATFORM_WIN64
	vsprintf_s(debugString, sizeof(debugString), fmt, args);
#else
	vsprintf(debugString, fmt, args);
#endif
	va_end(args);
#ifdef RE_PLATFORM_WIN64
	if (IsDebuggerPresent())
	{
		OutputDebugStringA(debugString);
		fputs(debugString, stdout);
	}
	else
		fputs(debugString, stdout);
#else // RE_PLATFORM_ANDROID
	__android_log_print(ANDROID_LOG_DEBUG, "RealityEngine", "%s", debugString);
#endif
#ifdef ENABLE_CONSOLE_LOG
	rsys_consoleLog(debugString);
#endif
}

inline void rsys_assertFunc(const char* func, const char* file, int line, const char* e)
{
	rsys_print("Assertion Failed: (%s),  function %s, file %s, line %d.\n", e, func, file, line);
#ifdef RE_PLATFORM_WIN64
	//	__asm("int3");
	DebugBreak();
#else
//	__asm volatile("bkpt 0");
	assert(0);
#endif
}

inline void rsys_assertMsgFunc(const char* func, const char* file, int line, const char* fmt, ...)
{
	char debugString[255];

	va_list args;
	va_start(args, fmt);
	vsprintf(debugString, fmt, args);
	va_end(args);

	rsys_print("Assertion Failed: %s\n  function %s, file %s, line %d.\n", debugString, func, file, line);
#ifdef RE_PLATFORM_WIN64
	DebugBreak();
#else
//	__asm volatile("bkpt 0");
	assert(0);
#endif
}

#ifndef RE_PLATFORM_WIN64
#define AssertMsg(e, fmt, ...)	(__builtin_expect(!(e), 0) ? rsys_assertMsgFunc(__func__, __FILE__, __LINE__, fmt, ##__VA_ARGS__) : (void)0)
#define	Assert(e)				(__builtin_expect(!(e), 0) ? rsys_assertFunc(__func__, __FILE__, __LINE__, #e) : (void)0)
#else
#define AssertMsg(e, fmt, ...)	((!(e), 0) ? rsys_assertMsgFunc(__func__, __FILE__, __LINE__, fmt, ##__VA_ARGS__) : (void)0)
#define	Assert(e)				((!(e), 0) ? rsys_assertFunc(__func__, __FILE__, __LINE__, #e) : (void)0)
#endif	

rsys_file file_open(const char* filename, const char* mode);
void file_close(rsys_file fptr);
bool file_is_open(rsys_file);
size_t file_length(rsys_file fptr);
size_t file_read(void* buffer, size_t bytes, rsys_file fptr);

typedef struct rsysApi
{
	const char*(*getStoragePath)(void);
	rsys_file(*file_open)(const char* filename, const char* mode);
	void(*file_close)(rsys_file file);
	bool(*file_is_open)(rsys_file file);
	size_t(*file_length)(rsys_file file);
	size_t(*file_read)(void *buffer, size_t bytes, rsys_file file);
	void(*print)(const char* fmt, ...);
	void(*consoleRender)(mat4x4 worldMatrix, rgui_font font);
	void(*perfTimerStart)(rsys_perfTimer* timer);
	uint64_t(*perfTimerStop)(rsys_perfTimer* timer);
	uint64_t(*perfTimerGetSplit)(rsys_perfTimer* timer);
}rsysApi;
