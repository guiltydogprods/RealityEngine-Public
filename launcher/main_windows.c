
#include "stdafx.h"
#include "engine.h"
#include "math/vec4.h"

#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <direct.h>
#define ION_PI (3.14159265359f)
#define ION_180_OVER_PI (180.0f / ION_PI)
#define ION_PI_OVER_180 (ION_PI / 180.0f)

#define DEGREES(x) ((x) * ION_180_OVER_PI)
#define RADIANS(x) ((x) * ION_PI_OVER_180)
#define TEST_FRAMETIME

const int32_t kWindowWidth = 1280;
const int32_t kWindowHeight = 720;

typedef enum rsys_engineMode
{
	kEMInvalid = 0,
	kEMInit,
	kEMPlay,
	kEMEdit,
	kEMViewer
}rsys_engineMode;

static rsys_engineMode s_currentMode = kEMInit;
static rsys_engineMode s_prevMode = kEMInit;
static bool s_bRoaming = false;
_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;

static void error_callback(int error, const char* description)
{
	(void)error;
	fputs(description, stderr);
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

static rsys_input s_systemInput = { 0 };
//static uint32_t s_keyPresses = 0;
//static float s_mouseMotionValid = 0.0f;
//static float s_mouseMoveX = 0.0f;
//static float s_mouseMoveY = 0.0f;

//void rvrs_initializeOvrPlatform(const char* appId);

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	(void)scancode;
	(void)mods;
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	if (key == GLFW_KEY_E && action == GLFW_PRESS)
	{
		s_prevMode = s_currentMode;
		s_currentMode = kEMEdit;
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
	}

	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
	{
		s_prevMode = s_currentMode;
		s_currentMode = kEMViewer;
		s_systemInput.mouseMotionValid = 0.0f;										// This ensures an immediate camera update.
		if (s_prevMode != kEMViewer) {
			s_bRoaming = true;
		}
		else
		{
			s_bRoaming ^= true;
		}
		glfwSetInputMode(window, GLFW_CURSOR, s_bRoaming ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
	}

	if (key == GLFW_KEY_O && action == GLFW_PRESS)
	{
		s_prevMode = s_currentMode;
		s_currentMode = kEMViewer;
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
	}

	if (key == GLFW_KEY_P && action == GLFW_PRESS)
	{
		s_prevMode = s_currentMode;
		s_currentMode = kEMPlay;
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
	}

	if ((key == GLFW_KEY_MENU || key == GLFW_KEY_GRAVE_ACCENT) && action == GLFW_PRESS)
	{
		s_systemInput.keyPresses |= kKeyMenu;
	}

	if (action == GLFW_PRESS || action == GLFW_REPEAT)
	{
		switch (key)
		{
		case GLFW_KEY_UP:
		case GLFW_KEY_W:
			s_systemInput.keyPresses |= kKeyForward;
			break;
		case GLFW_KEY_DOWN:
		case GLFW_KEY_S:
			s_systemInput.keyPresses |= kKeyBackwards;
			break;
		case GLFW_KEY_LEFT:
		case GLFW_KEY_A:
			s_systemInput.keyPresses |= kKeyLeft;
			break;
		case GLFW_KEY_RIGHT:
		case GLFW_KEY_D:
			s_systemInput.keyPresses |= kKeyRight;
			break;
		case GLFW_KEY_LEFT_BRACKET:
			s_systemInput.keyPresses |= kKeyYawLeft;
			break;
		case GLFW_KEY_RIGHT_BRACKET:
			s_systemInput.keyPresses |= kKeyYawRight;
			break;
		}
	}
	if (action == GLFW_RELEASE)
	{
		switch (key)
		{
		case GLFW_KEY_UP:
		case GLFW_KEY_W:
			s_systemInput.keyPresses &= ~kKeyForward;
			break;
		case GLFW_KEY_DOWN:
		case GLFW_KEY_S:
			s_systemInput.keyPresses &= ~kKeyBackwards;
			break;
		case GLFW_KEY_LEFT:
		case GLFW_KEY_A:
			s_systemInput.keyPresses &= ~kKeyLeft;
			break;
		case GLFW_KEY_RIGHT:
		case GLFW_KEY_D:
			s_systemInput.keyPresses &= ~kKeyRight;
			break;
		case GLFW_KEY_LEFT_BRACKET:
			s_systemInput.keyPresses &= ~kKeyYawLeft;
			break;
		case GLFW_KEY_RIGHT_BRACKET:
			s_systemInput.keyPresses &= ~kKeyYawRight;
			break;
		case GLFW_KEY_MENU:
		case GLFW_KEY_GRAVE_ACCENT:
			s_systemInput.keyPresses &= ~kKeyMenu;
		}
	}
}

static void mouse_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
	s_systemInput.mousePrevPosX = s_systemInput.mousePosX;
	s_systemInput.mousePrevPosY = s_systemInput.mousePosY;
	float x = s_systemInput.mousePosX = (float)xpos;
	float y = s_systemInput.mousePosY = (float)ypos;
	if (s_currentMode == kEMViewer)
	{
		s_systemInput.mouseMoveX = xpos - s_systemInput.mousePrevPosX;
		s_systemInput.mouseMoveY = ypos - s_systemInput.mousePrevPosY;
	}
	s_systemInput.mouseMotionValid = 0.0f;

	s_systemInput.mousePosNdcX = x >= 0.0f && x <= (float)kWindowWidth ? ((x / (float)kWindowWidth) * 2.0f - 1.0f) : -1.0f;
	s_systemInput.mousePosNdcY = y >= 0.0f && y <= (float)kWindowWidth ? ((((float)kWindowHeight - y) / (float)kWindowHeight) * 2.0f - 1.0f) : -1.0f;
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_1)
	{
		if (action == GLFW_PRESS)
			s_systemInput.mousePresses = 1;
		else
			s_systemInput.mousePresses = 0;
	}
}

vec4 to_euler(vec4 right, vec4 up, vec4 forward)
{
	/** this conversion uses conventions as described on page:
	*   https://www.euclideanspace.com/maths/geometry/rotations/euler/index.htm
	*   Coordinate System: right hand
	*   Positive angle: right hand
	*   Order of euler angles: heading first, then attitude, then bank
	*   matrix row column ordering:
	*   [m00 m01 m02]
	*   [m10 m11 m12]
	*   [m20 m21 m22]*/
	float m00 = right.x;
	float m10 = right.y;
	float m20 = right.z;
//	float m01 = up.x;
	float m11 = up.y;
//	float m21 = up.z;
	float m02 = forward.x;
	float m12 = forward.y;
	float m22 = forward.z;

	// Assuming the angles are in radians.
	float heading, pitch, roll;
	if (m10 > 0.998f) { // singularity at north pole
		heading = atan2f(m02, m22);
		pitch = ION_PI / 2.0f;
		roll = 0.0f;
		goto exit;
	}
	if (up.x < -0.998f) { // singularity at south pole
		heading = atan2f(m02, m22);
		pitch = -ION_PI / 2.0f;
		roll = 0.0f;
		goto exit;
	}
	heading = atan2f(-m20, m00);
	pitch = atan2f(-m12, m11);
	roll = asinf(m10);
exit:
	return (vec4) { roll, heading, -pitch, 0.0f };
}

vec4 from_euler(vec4 euler)
{
	float rx = euler.x;
	float ry = euler.y;
	float rz = euler.z;

	float tx = rx * (float)0.5f;
	float ty = ry * (float)0.5f;
	float tz = rz * (float)0.5f;

	float cx = (float)cosf(tx);
	float cy = (float)cosf(ty);
	float cz = (float)cosf(tz);
	float sx = (float)sinf(tx);
	float sy = (float)sinf(ty);
	float sz = (float)sinf(tz);

	float cc = cx * cz;
	float cs = cx * sz;
	float sc = sx * cz;
	float ss = sx * sz;

	float x = (cy * sc) - (sy * cs);
	float y = (cy * ss) + (sy * cc);
	float z = (cy * cs) - (sy * sc);
	float w = (cy * cc) + (sy * ss);

	return (vec4) { x, y, z, w };
	// INSURE THE QUATERNION IS NORMALIZED
	// PROBABLY NOT NECESSARY IN MOST CASES
//		Normalise();
}

void updateViewerModeCamera(float dt)
{
	static bool bInitialized;
	static mat4x4 s_editCamWorldMatrix;
	static mat4x4 s_editCamProjMatrix;
	static float s_editCamHeading = 0.0f;
	static float s_editCamPitch = 0.0f;

	if (!bInitialized)
	{
		mat4x4 viewMatrix = rgfx_getCameraViewMatrix(0);
		s_editCamProjMatrix = rgfx_getCameraProjectionMatrix(0);
		s_editCamWorldMatrix = mat4x4_orthoInverse(viewMatrix);
		// Obtain heading and pitch from existing camera.
		vec4 euler = to_euler(s_editCamWorldMatrix.xAxis, s_editCamWorldMatrix.yAxis, s_editCamWorldMatrix.zAxis);
		s_editCamHeading = DEGREES(euler.y);
		s_editCamPitch = DEGREES(euler.z);
		bInitialized = true;
	}

	const float kMoveSpeed = 2.5f;
	const float kMouseSpeed = 5.0f;

	if (s_bRoaming && (s_systemInput.keyPresses != 0 || s_systemInput.mouseMotionValid < (2.0f * dt)))
	{
		s_editCamHeading -= s_systemInput.mouseMoveX * kMouseSpeed * dt;
		s_editCamPitch -= s_systemInput.mouseMoveY * kMouseSpeed * dt;

		if (s_editCamHeading >= 360.0f)
			s_editCamHeading -= 360.0f;
		if (s_editCamHeading < 0.0f)
			s_editCamHeading += 360.0f;

		if (s_editCamPitch >= 360.0f)
			s_editCamPitch -= 360.0f;
		if (s_editCamPitch < 0.0f)
			s_editCamPitch += 360.0f;
	}
	{
		//Calculate rotation matrix
		float cosa, sina;
		cosa = cosf(RADIANS(s_editCamHeading));
		sina = sinf(RADIANS(s_editCamHeading));

		mat4x4 yrot = mat4x4_rotate(RADIANS(s_editCamHeading), (vec4) { 0.0f, 1.0f, 0.0f, 0.0f });
		mat4x4 xrot = mat4x4_rotate(RADIANS(s_editCamPitch), (vec4) { -cosa, 0.0f, sina, 0.0f });
		mat4x4 rot = mat4x4_mul(xrot, yrot);

/* // - Code to check calculated euler angles match cam heading and pitch.
		vec4 euler = to_euler(rot.xAxis, rot.yAxis, rot.zAxis);
		float calcHeading = DEGREES(euler.y);
		if (calcHeading >= 360.0f)
			calcHeading -= 360.0f;
		if (calcHeading < 0.0f)
			calcHeading += 360.0f;
		float calcPitch = DEGREES(euler.z);
		if (calcPitch >= 360.0f)
			calcPitch -= 360.0f;
		if (calcPitch < 0.0f)
			calcPitch += 360.0f;
		rsys_print("Launcher: Heading (%.3f, %.3f), Pitch (%.3f, %.3f)\n", calcHeading, s_editCamHeading, calcPitch, s_editCamPitch);
*/
		vec4 right = rot.xAxis;
//		vec4 up = rot.yAxis;
		vec4 forward = rot.zAxis;
		vec4 pos = s_editCamWorldMatrix.wAxis;

		if (s_systemInput.keyPresses & kKeyForward)
			pos -= forward * kMoveSpeed * dt;
		if (s_systemInput.keyPresses & kKeyBackwards)
			pos += forward * kMoveSpeed * dt;
		if (s_systemInput.keyPresses & kKeyLeft)
			pos -= right * kMoveSpeed * dt;
		if (s_systemInput.keyPresses & kKeyRight)
			pos += right * kMoveSpeed * dt;

		mat4x4 trans = mat4x4_translate(pos);
		s_editCamWorldMatrix = mat4x4_mul(trans, rot);

		mat4x4 viewMatrix = mat4x4_orthoInverse(s_editCamWorldMatrix);

		rgfx_updateTransforms(&(rgfx_cameraDesc) {
			.camera[0].position = pos,
			.camera[0].viewMatrix = viewMatrix,
			.camera[0].projectionMatrix = s_editCamProjMatrix,
		});
	}
	if (s_systemInput.mouseMotionValid >= (2.0f * dt))
	{
		s_systemInput.mouseMoveX = 0.0f;
		s_systemInput.mouseMoveY = 0.0f;
	}
}

void* (*_Block_copy)(const void* aBlock) = NULL;
void (*_Block_release)(const void* aBlock) = NULL;

#define GAME_INITIALIZE(name) void name(int32_t width, int height, const rengApi *reng)
typedef GAME_INITIALIZE(GameInitializeFunc);
GAME_INITIALIZE(gameInitializeStub)
{
	(void)width;
	(void)height;
	(void)reng;
}

#define GAME_TERMINATE(name) void name(void/*ion::GfxDevice& gfxDevice*/)
typedef GAME_TERMINATE(GameTerminateFunc);
GAME_TERMINATE(gameTerminateStub)
{
}

#define GAME_SHOULDQUIT(name) bool name(bool systemQuit)
typedef GAME_SHOULDQUIT(GameShouldQuitFunc);
GAME_SHOULDQUIT(gameShouldQuitStub)
{
	(void)systemQuit;
}

#define GAME_UPDATE(name) void name(float dt, rsys_input systemInput)
typedef GAME_UPDATE(GameUpdateFunc);
GAME_UPDATE(gameUpdateStub)
{
}

#define GAME_RENDER(name) void name(void/*ion::GfxDevice& gfxDevice*/)
typedef GAME_RENDER(GameRenderFunc);
GAME_RENDER(gameRenderStub)
{
}

typedef struct win32_game_code
{
	HMODULE GameCodeDLL;
	GameInitializeFunc *initialize;
	GameTerminateFunc *terminate;
	GameShouldQuitFunc* shouldQuit;
	GameUpdateFunc *update;
	GameRenderFunc *render;
	FILETIME DLLTimeStamp;
	bool IsValid;
}win32_game_code;

FILETIME GetLastWriteTime(const char* filename)
{
	FILETIME lastWriteTime = {};

	WIN32_FIND_DATAA FindData;
	HANDLE FindHandle = FindFirstFileA(filename, &FindData);
	if (FindHandle != INVALID_HANDLE_VALUE)
	{
		FindClose(FindHandle);
		lastWriteTime = FindData.ftLastWriteTime;
	}
	return lastWriteTime;
}

void GetDLLPaths(const char* dllName, char* sourcePath, char* workingPath)
{
	char exePath[MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	// Get path of running exe. (To determine location of DLL.);
	HMODULE ExeModule = GetModuleHandleA(NULL);
	GetModuleFileNameA(ExeModule, exePath, MAX_PATH);
//	_splitpath(exePath, drive, dir, fname, ext);

	// Copy Game DLL to 'dllName'_working.dll so visual studio locking doesn't stop us rebuilding it.
	_splitpath(dllName, drive, dir, fname, ext);
	_makepath(sourcePath, drive, dir, fname, ext);
	strcat(fname, "_working");
	_makepath(workingPath, drive, dir, fname, ext);
}

void CleanupPDBs(const char* dllName)
{
	char searchNameBuffer[MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	_splitpath(dllName, drive, dir, fname, ext);

	size_t dllNameLen = strlen(dllName);
	strncpy(searchNameBuffer, dllName, dllNameLen-4);
	strcat(searchNameBuffer, "-*.pdb");

	WIN32_FIND_DATAA findData = {};
	HANDLE hFind = FindFirstFileA(searchNameBuffer, &findData);

	char foundFiles[16][MAX_PATH];
	int32_t foundCount = 0;
	if (hFind != INVALID_HANDLE_VALUE)
	{
		FILETIME mostRecent = findData.ftLastWriteTime;
		int32_t mostRecentIndex = 0;
		strcpy(foundFiles[foundCount++], findData.cFileName);
		bool found;
		do 
		{
			found = FindNextFileA(hFind, &findData);
			if (found)
			{
				FILETIME moreRecent = findData.ftLastWriteTime;
				if (CompareFileTime(&moreRecent, &mostRecent) > 0)
				{
					mostRecent = moreRecent;
					mostRecentIndex = foundCount;
				}
				strcpy(foundFiles[foundCount++], findData.cFileName);
			}
		} while (found == true);
		FindClose(hFind);

		for (int32_t i = 0; i < foundCount; ++i)
		{
			if (i != mostRecentIndex)
			{
				_makepath(searchNameBuffer, drive, dir, foundFiles[i], NULL);
//				strncpy(searchNameBuffer, dllName, dllNameLen - 4);
//				strcat(searchNameBuffer, foundFiles[i]);
				char debugString[4096];				//CLR - Should really calculate the size of the output string instead.
				sprintf(debugString, "Deleting %s\n", searchNameBuffer);
				OutputDebugStringA(debugString);
				DeleteFileA(searchNameBuffer);
			}
		}
	}

/*
	char exePath[MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	// Get path of running exe. (To determine location of DLL.);
	HMODULE ExeModule = GetModuleHandleA(NULL);
	GetModuleFileNameA(ExeModule, exePath, MAX_PATH);
	_splitpath(exePath, drive, dir, fname, ext);

	// Copy Game DLL to 'dllName'_working.dll so visual studio locking doesn't stop us rebuilding it.
	_splitpath(dllName, NULL, NULL, fname, ext);
	_makepath(sourcePath, drive, dir, fname, ext);
	strcat(fname, "_working");
	_makepath(workingPath, drive, dir, fname, ext);
*/
}

#include <strsafe.h>

win32_game_code LoadGameCode(const char* sourceDllName, const char* workingDllName)
{
	win32_game_code result = {};

	if (CopyFileA(sourceDllName, workingDllName, false))
	{
		result.DLLTimeStamp = GetLastWriteTime(workingDllName); // sourceDllName);
		// Load Game DLL using modified filename.
		result.GameCodeDLL = LoadLibraryA(workingDllName);
		if (result.GameCodeDLL)
		{
			result.initialize = (GameInitializeFunc*)GetProcAddress(result.GameCodeDLL, "game_initialize");
			result.shouldQuit = (GameShouldQuitFunc*)GetProcAddress(result.GameCodeDLL, "game_shouldQuit");
			result.terminate = (GameTerminateFunc *)GetProcAddress(result.GameCodeDLL, "game_terminate");
			result.update = (GameUpdateFunc *)GetProcAddress(result.GameCodeDLL, "game_update");
			result.render = (GameRenderFunc *)GetProcAddress(result.GameCodeDLL, "game_render");
			result.IsValid = result.initialize && result.update && result.render;
		}
		else
		{
			LPTSTR lpszFunction = "Hello: ";
			LPVOID lpMsgBuf;
			LPVOID lpDisplayBuf;
			DWORD dw = GetLastError();
			FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				dw,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPTSTR)&lpMsgBuf,
				0, NULL);

			// Display the error message and exit the process

			lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
				(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
			StringCchPrintf((LPTSTR)lpDisplayBuf,
				LocalSize(lpDisplayBuf) / sizeof(TCHAR),
				TEXT("%s failed with error %d: %s"),
				lpszFunction, dw, lpMsgBuf);
			MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

			LocalFree(lpMsgBuf);
			LocalFree(lpDisplayBuf);
			ExitProcess(dw);
		}
	}

	CleanupPDBs(sourceDllName);

	if (!result.IsValid)
	{
		result.initialize = gameInitializeStub;
		result.shouldQuit = gameShouldQuitStub;
		result.terminate = gameTerminateStub;
		result.update = gameUpdateStub;
		result.render = gameRenderStub;
		result.IsValid = true;
	}

	return result;
}

void UnloadGameCode(win32_game_code game)
{
	game.terminate();
	FreeLibrary(game.GameCodeDLL);
	game.GameCodeDLL = NULL;
	game.initialize = gameInitializeStub;
	game.shouldQuit = gameShouldQuitStub;
	game.terminate = gameTerminateStub;
	game.update = gameUpdateStub;
	game.render = gameRenderStub;
	game.IsValid = true;
}

/*
rsysApi rsys = {
	.file_open = file_open,
	.file_close = file_close,
	.file_is_open = file_is_open,
	.file_length = file_length,
	.file_read = file_read,
	.print = rsys_print,
	.perfTimerStart = rsys_perfTimerStart,
	.perfTimerStop = rsys_perfTimerStop,
};

rresApi rres = {
	.registerResource = rres_registerResource,
	.findTexture = rres_findTexture,
};

rgfxApi rgfx = {
	.addMeshInstance = rgfx_addMeshInstance,
	.updateMeshInstance = rgfx_updateMeshInstance,
	.updateMeshMaterial = rgfx_updateMeshMaterial,
	.removeAllMeshInstances = rgfx_removeAllMeshInstances,
	.updateTransforms = rgfx_updateTransforms,
	.mapModelMatricesBuffer = rgfx_mapModelMatricesBuffer,
	.unmapModelMatricesBuffer = rgfx_unmapModelMatricesBuffer,
	.createBuffer = rgfx_createBuffer,
	.mapBuffer = rgfx_mapBuffer,
	.unmapBuffer = rgfx_unmapBuffer,
	.getBufferInfo = rgfx_getBufferInfo,
	.createPass = rgfx_createPass,
	.createRenderTarget = rgfx_createRenderTarget,
	.beginDefaultPass = rgfx_beginDefaultPass,
	.beginPass = rgfx_beginPass,
	.renderWorld = rgfx_renderWorld,
	.endPass = rgfx_endPass,
	.createPipeline = rgfx_createPipeline,
	.bindPipeline = rgfx_bindPipeline,
	.loadShader = rgfx_loadShader,
};

raudApi raud = {
	.registerSoundBank = raud_registerSoundBank,
	.findSoundBank = raud_findSoundBank,
	.playSound = raud_playSound,
	.playSoundBank = raud_playSoundBank,
	.updateListenerPosition = raud_updateListenerPosition,
	.updateSoundPosition = raud_updateSoundPosition,
};
*/

static rengApi reng;

#define match(X, ...) _Generic ((X), ##__VA_ARGS__)(X)

bool rvrs_initializeAvatarOvr(const char* appId);
bool rvrs_requestAvatarSpecOvr(uint64_t userId, int32_t slot);
bool rvrs_destroyAvatarOvr(uint64_t userId, int32_t slot);

int main(int argc, char *argv[])
{
	(void)argc;

	s_currentMode = kEMInit;
#
	if (!glfwInit())
		exit(EXIT_FAILURE);

	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
	glfwSetErrorCallback(error_callback);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	const int32_t kDefaultOpenGLMajor = 4;
	const int32_t kDefaultOpenGLMinor = 6;

	GLFWwindow *window = NULL;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, kDefaultOpenGLMajor);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, kDefaultOpenGLMinor);
	window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Air Hockey Arcade", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwShowWindow(window);
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
	}
	
	fprintf(stdout, "Vendor: %s\n", glGetString(GL_VENDOR));
	fprintf(stdout, "Renderer: %s\n", glGetString(GL_RENDERER));
	fprintf(stdout, "Version: %s\n", glGetString(GL_VERSION));

	fprintf(stdout, "Extensions:\n");
	GLint n, i;
	glGetIntegerv(GL_NUM_EXTENSIONS, &n);
	for (i = 0; i < n; i++)
	{
		fprintf(stdout, "%s\n", glGetStringi(GL_EXTENSIONS, i));
	}

	fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_pos_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	//	const uint64_t kKilobytes = 1024;
//	const uint64_t kMegaBytes = 1024 * kKilobytes;
//	const uint64_t kGigaBytes = 1024 * kMegaBytes;
//	const uint64_t kTerraBytes = 1024 * kGigaBytes;
//	uint64_t kSystemBaseAddress = 1 * 1024 * kGigaBytes;
//	uint64_t kGameBaseAddress = 2 * 1024 * kGigaBytes;
//	uint64_t kSystemMemorySize = 1 * kGigaBytes;
//	uint64_t kGameMemorySize = 1 * kGigaBytes;

//	LPVOID systemMemory = VirtualAlloc((void *)kSystemBaseAddress, kSystemMemorySize, MEM_RESERVE, PAGE_READWRITE);
//	LPVOID gameMemory = VirtualAlloc((void *)kGameBaseAddress, kGameMemorySize, MEM_RESERVE, PAGE_READWRITE);

//	size_t memorySize = 32 * 1024 * 1024;
//	uint8_t *memoryBlock = (uint8_t *)_aligned_malloc(memorySize, 32);
//	pGNSI = (PGNSI)GetProcAddress(
//		GetModuleHandle(TEXT("kernel32.dll")),
//		"GetNativeSystemInfo");

// Obtain entry points for Blocks Runtime. 
	HMODULE handle = GetModuleHandleA("BlocksRuntime.dll"); // NULL);
	_Block_copy = GetProcAddress(handle, "_Block_copy");
	_Block_release = GetProcAddress(handle, "_Block_release");
	void(^ myBlock)(void) = ^ (void) {
		rsys_print("Blocks Runtime initialized!\n");
	};
	myBlock();

	int32_t cwdLen = GetCurrentDirectoryA(0, NULL);

	const char* storagePath = (const char*)malloc(cwdLen + 1);
	GetCurrentDirectoryA(cwdLen, storagePath);

	rsys_initialize(&(rsys_initParams) {
		.storagePath = storagePath,
	});

	rsys_perfTimerInitialize();

	rvrs_initialize(&(rvrs_initParams) {
#ifdef VRSYSTEM_OCULUS_WIN
		.ovrInitParams = {
			.Flags = ovrInit_RequestVersion | ovrInit_FocusAware,
			.RequestedMinorVersion = OVR_MINOR_VERSION,
		},
		.ovrMirrorTextureDesc = {
			.Width = kWindowWidth > 1280 ? 1280 : kWindowWidth,
			.Height = kWindowHeight > 720 ? 720 : kWindowHeight,
			.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB,
			.MirrorOptions = ovrMirrorOption_RightEyeOnly
		},
#endif
	});

	rgfx_initialize(&(rgfx_initParams) {
		.eyeWidth = 0,
		.eyeHeight = 0,
		.extensions = {
			.multi_view = false,
			.EXT_texture_border_clamp = false,
		}
	});

	reng.rsys = (rsysApi) {
		.file_open = file_open,
		.file_close = file_close,
		.file_is_open = file_is_open,
		.file_length = file_length,
		.file_read = file_read,
		.print = rsys_print,
		.getStoragePath = rsys_getStoragePath,
		.consoleRender = rsys_consoleRender,
		.perfTimerStart = rsys_perfTimerStart,
		.perfTimerStop = rsys_perfTimerStop,
		.perfTimerGetSplit = rsys_perfTimerGetSplit,
	};

	reng.rres = (rresApi) {
		.registerResource = rres_registerResource,
		.findTexture = rres_findTexture,
		.findFont = rres_findFont,
		.findSound = rres_findSound,
	};

	reng.rgfx = (rgfxApi){
		.findMesh = rgfx_findMesh,
		.findMeshInstance = rgfx_findMeshInstance,
		.addMeshInstance = rgfx_addMeshInstance,
		.updateLight = rgfx_updateLight,
		.updateMeshInstance = rgfx_updateMeshInstance,
		.updateMeshMaterial = rgfx_updateMeshMaterial,
		.removeAllMeshInstances = rgfx_removeAllMeshInstances,
		.getCameraViewMatrix = rgfx_getCameraViewMatrix,
		.updateTransforms = rgfx_updateTransforms,
		.setGameCamera = rgfx_setGameCamera,
		.mapModelMatricesBuffer = rgfx_mapModelMatricesBuffer,
		.unmapModelMatricesBuffer = rgfx_unmapModelMatricesBuffer,
		.createBuffer = rgfx_createBuffer,
		.mapBuffer = rgfx_mapBuffer,
		.unmapBuffer = rgfx_unmapBuffer,
		.getBufferInfo = rgfx_getBufferInfo,
		.createPass = rgfx_createPass,
		.createRenderTarget = rgfx_createRenderTarget,
		.createTextureArray = rgfx_createTextureArray,
		.scaleEyeFrameBuffers = rgfx_scaleEyeFrameBuffers,
		.beginDefaultPass = rgfx_beginDefaultPass,
		.beginPass = rgfx_beginPass,
		.renderWorld = rgfx_renderWorld,
		.getRenderStats = rgfx_getRenderStats,
		.getLightsBuffer = rgfx_getLightsBuffer,
		.endPass = rgfx_endPass,
		.createPipeline = rgfx_createPipeline,
		.bindPipeline = rgfx_bindPipeline,
		.loadShader = rgfx_loadShader,
		.initPostEffects = rgfx_initPostEffects,
		.renderFullscreenQuad = rgfx_renderFullscreenQuad,
		.debugRenderInitialize = rgfx_debugRenderInitialize,
		.drawDebugLine = rgfx_drawDebugLine,
		.drawDebugAABB = rgfx_drawDebugAABB,
	};

	reng.raud = (raudApi) {
		.update = raud_update,
		.registerSoundBank = raud_registerSoundBank,
		.findSoundBank = raud_findSoundBank,
		.playSound = raud_playSound,
		.playSoundBank = raud_playSoundBank,
		.updateListenerPosition = raud_updateListenerPosition,
		.updateSoundVolume = raud_updateSoundVolume,
		.updateSoundPosition = raud_updateSoundPosition,
		.getSoundUserData = raud_getSoundUserData,
		.createStream = raud_createStream,
		.stopStream = raud_stopStream,
	};

	reng.rvrs = (rvrsApi) {
		.isInitialized = rvrs_isInitialized,
		.isHmdMounted = rvrs_isHmdMounted,
		.isTrackingControllers = rvrs_isTrackingControllers,
		.shouldPause = rvrs_shouldPause,
		.shouldQuit = rvrs_shouldQuit,
		.getDeviceType = rvrs_getDeviceType,
		.getRefreshRate = rvrs_getRefreshRate,
		.setRefreshRate = rvrs_setRefreshRate,
		.setFoveationLevel = rvrs_setFoveationLevel,
		.getScaledEyeSize = rvrs_getScaledEyeSize,
		.getSupportedRefreshRates = rvrs_getSupportedRefreshRates,
		.getHeadMatrix = rvrs_getHeadMatrix,
		.getHeadPose = rvrs_getHeadPose,
		.getTrackedControllerMatrix = rvrs_getTrackedControllerMatrix,
		.getTrackedControllerPose = rvrs_getTrackedControllerPose,
		.getControllerState = rvrs_getControllerState,
		.setControllerHapticVibration = rvrs_setControllerHapticVibration,
		.updateAvatarFrameParams = rvrs_updateAvatarFrameParams,
		.initializePlatformOvr = rvrs_initializePlatformOvr,
		.createPrivateRoomOvr = rvrs_createPrivateRoomOvr,
		.inviteUserOvr = rvrs_inviteUserOvr,
		.kickUserOvr = rvrs_kickUserOvr,
		.joinRoomOvr = rvrs_joinRoomOvr,
		.leaveRoomOvr = rvrs_leaveRoomOvr,
		.netConnect = rvrs_netConnect,
		.netClose = rvrs_netClose,
		.netSendPacket = rvrs_netSendPacket,
		.netReadPackets = rvrs_netReadPackets,
		.netPing = rvrs_netPing,
		.voipStart = rvrs_voipStart,
		.voipStop = rvrs_voipStop,
		.voipMute = rvrs_voipMute,
		.voipGetPCM = rvrs_voipGetPCM,
		.initializeAvatarOvr = rvrs_initializeAvatarOvr,
		.requestAvatarSpecOvr = rvrs_requestAvatarSpecOvr,
		.destroyAvatarOvr = rvrs_destroyAvatarOvr,
	};

	raud_initialize();

	reng.rgui = rgui_initialize();

// Parse command line (CLR - Need to do this properly).
	const char* cmd = NULL;
	char gameDllName[MAX_PATH];
	while ((cmd = *(argv++)) != NULL)
	{
		if (cmd[0] == '-')
		{
			if (!strncmp(&cmd[1], "game", 4))
			{
				if (cmd[5] == '=')
				{
					size_t nameLen = strlen(&cmd[6]);
					if (nameLen < 512)
					{
						strncpy_s(gameDllName, sizeof(gameDllName), &cmd[6], nameLen);
						gameDllName[nameLen] = '\0';
					}
				}
			}
		}
	}
	const char* sourceDllName = (const char *)gameDllName;
	char sourceDllPath[MAX_PATH];
	char workingDllPath[MAX_PATH];
	GetDLLPaths(sourceDllName, sourceDllPath, workingDllPath);
	win32_game_code game = LoadGameCode(sourceDllPath, workingDllPath);
	game.initialize(kWindowWidth, kWindowHeight, &reng);

	s_currentMode = kEMPlay;
#ifdef TEST_FRAMETIME
	rsys_perfTimer timer;
	rsys_perfTimerStart(&timer);
#endif
	while (!game.shouldQuit(glfwWindowShouldClose(window) != 0))
	{
#ifndef TEST_FRAMETIME
		rsys_perfTimer timer;
		rsys_perfTimerStart(&timer);
#endif

		static int32_t reloadCount = 0;
		FILETIME NewDLLWriteTime = GetLastWriteTime(sourceDllPath);
		if (CompareFileTime(&NewDLLWriteTime, &game.DLLTimeStamp) > 0)
		{
			UnloadGameCode(game);
			game = LoadGameCode(sourceDllPath, workingDllPath);
			game.initialize(kWindowWidth, kWindowHeight, &reng);
			reloadCount = 0;
		}

		rvrs_updateTracking(&(rvrs_appInfo) {
		});

		if (rvrs_isHmdMounted())
			glfwSwapInterval(0);
		else
			glfwSwapInterval(1);

#ifndef TEST_FRAMETIME
		float dt = rvrs_isInitialized() && rvrs_isHmdMounted() ? rvrs_getRefreshRate() : 1.0f / 60.0f;
#else
		uint64_t now = rsys_perfTimerGetSplit(&timer);
		float dt = (float)now / 1000000.0f;
#endif

		switch (s_currentMode)
		{
		case kEMPlay:
			game.update(dt, s_systemInput);
			break;
		case kEMEdit:
//			updateEditMode(dt);
			break;
		case kEMViewer:
			game.update(dt, s_systemInput);
			updateViewerModeCamera(dt);
			break;
		default:
			assert(0);
		}
		game.render();

#ifndef TEST_FRAMETIME
		uint64_t uSecs = rsys_perfTimerStop(&timer);

		static uint64_t totalUSecs = 0;
		static uint64_t frames = 0;
		totalUSecs += uSecs;
		if (++frames > 60)
		{
//			rsys_print("Avg uSecs = %ld\n", totalUSecs / frames);
			totalUSecs = 0;
			frames = 0;
		}
#endif

		glfwSwapBuffers(window);
		glfwPollEvents();
		s_systemInput.mouseMotionValid += 1.0f / 60.0f;
	}

	game.terminate();

	rgfx_terminate();

//	rvrs_terminate();

	raud_terminate();

	glfwDestroyWindow(window);
	glfwTerminate();
}
