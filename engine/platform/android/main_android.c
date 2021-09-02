/*
* Copyright (C) 2010 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <sys/prctl.h>					// for prctl( PR_SET_NAME )
#include <dlfcn.h>						// for dlopen
#include <android/log.h>
#include <android/window.h>				// for AWINDOW_FLAG_KEEP_SCREEN_ON
#include <android/native_window_jni.h>	// for native window JNI
#include "android_native_app_glue.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include "engine.h"
/*
#include "renderer.h"
#include "resource.h"
#include "vrsystem.h"
#include "audio.h"
#include "system.h"
*/
//#define CHECK_GL_ERRORS

#if !defined( EGL_OPENGL_ES3_BIT_KHR )
#define EGL_OPENGL_ES3_BIT_KHR		0x0040
#endif

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER			0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR		0x1004
#endif

#if !defined( GL_EXT_multisampled_render_to_texture )
typedef void (GL_APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);
#endif

#if !defined( GL_OVR_multiview )
static const int GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR = 0x9630;
static const int GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR = 0x9632;
static const int GL_MAX_VIEWS_OVR = 0x9631;
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews);
#endif

#if !defined( GL_OVR_multiview_multisampled_render_to_texture )
typedef void (GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLsizei samples, GLint baseViewIndex, GLsizei numViews);
#endif

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"

//#include <malloc.h>

#define LOG_TAG "Quest1"

#define ALOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )
#if DEBUG
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__ )
#else
#define ALOGV(...)
#endif

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 2; // 3;
static const int NUM_MULTI_SAMPLES = 4;

#define MAX_PATH 260
#define MULTI_THREADED			0

//VrApi vrapi = {};

/*
================================================================================

System Clock Time

================================================================================
*/

static double GetTimeInSeconds()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
}

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/

typedef struct
{
	bool multi_view;					// GL_OVR_multiview, GL_OVR_multiview2
	bool EXT_texture_border_clamp;		// GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
	bool EXT_texture_filter_anisotropic;
} OpenGLExtensions_t;

OpenGLExtensions_t glExtensions = { 0 };

static void EglInitExtensions()
{
	const char* allExtensions = (const char*)glGetString(GL_EXTENSIONS);
	if (allExtensions != NULL)
	{
		glExtensions.multi_view = strstr(allExtensions, "GL_OVR_multiview2") &&
			strstr(allExtensions, "GL_OVR_multiview_multisampled_render_to_texture");

		glExtensions.EXT_texture_border_clamp = strstr(allExtensions, "GL_EXT_texture_border_clamp") ||
			strstr(allExtensions, "GL_OES_texture_border_clamp");

		glExtensions.EXT_texture_filter_anisotropic = strstr(allExtensions, "GL_EXT_texture_filter_anisotropic");
		const char* vendorString = (const char*)glGetString(GL_VENDOR);
		const char* rendererString = (const char*)glGetString(GL_RENDERER);
		const char* versionString = (const char*)glGetString(GL_VERSION);
		ALOGE("Vendor: %s\n", vendorString);
		ALOGE("Renderer: %s\n", rendererString);
		ALOGE("Version: %s\n", versionString);
#if 1
		ALOGE("Extensions:\n");
		GLint n, i;

		glGetIntegerv(GL_NUM_EXTENSIONS, &n);
		for (i = 0; i < n; i++)
		{
			const char* extensionString = (const char*)glGetStringi(GL_EXTENSIONS, i);
			ALOGE("\t%s\n", extensionString);
		}
#endif
	}
}

static const char* EglErrorString(const EGLint error)
{
	switch (error)
	{
	case EGL_SUCCESS:				return "EGL_SUCCESS";
	case EGL_NOT_INITIALIZED:		return "EGL_NOT_INITIALIZED";
	case EGL_BAD_ACCESS:			return "EGL_BAD_ACCESS";
	case EGL_BAD_ALLOC:				return "EGL_BAD_ALLOC";
	case EGL_BAD_ATTRIBUTE:			return "EGL_BAD_ATTRIBUTE";
	case EGL_BAD_CONTEXT:			return "EGL_BAD_CONTEXT";
	case EGL_BAD_CONFIG:			return "EGL_BAD_CONFIG";
	case EGL_BAD_CURRENT_SURFACE:	return "EGL_BAD_CURRENT_SURFACE";
	case EGL_BAD_DISPLAY:			return "EGL_BAD_DISPLAY";
	case EGL_BAD_SURFACE:			return "EGL_BAD_SURFACE";
	case EGL_BAD_MATCH:				return "EGL_BAD_MATCH";
	case EGL_BAD_PARAMETER:			return "EGL_BAD_PARAMETER";
	case EGL_BAD_NATIVE_PIXMAP:		return "EGL_BAD_NATIVE_PIXMAP";
	case EGL_BAD_NATIVE_WINDOW:		return "EGL_BAD_NATIVE_WINDOW";
	case EGL_CONTEXT_LOST:			return "EGL_CONTEXT_LOST";
	default:						return "unknown";
	}
}

static const char* GlFrameBufferStatusString(GLenum status)
{
	switch (status)
	{
	case GL_FRAMEBUFFER_UNDEFINED:						return "GL_FRAMEBUFFER_UNDEFINED";
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:			return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:	return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
	case GL_FRAMEBUFFER_UNSUPPORTED:					return "GL_FRAMEBUFFER_UNSUPPORTED";
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:			return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
	default:											return "unknown";
	}
}

#ifdef CHECK_GL_ERRORS

static const char* GlErrorString(GLenum error)
{
	switch (error)
	{
	case GL_NO_ERROR:						return "GL_NO_ERROR";
	case GL_INVALID_ENUM:					return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE:					return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION:				return "GL_INVALID_OPERATION";
	case GL_INVALID_FRAMEBUFFER_OPERATION:	return "GL_INVALID_FRAMEBUFFER_OPERATION";
	case GL_OUT_OF_MEMORY:					return "GL_OUT_OF_MEMORY";
	default: return "unknown";
	}
}

static void GLCheckErrors(int line)
{
	for (int i = 0; i < 10; i++)
	{
		const GLenum error = glGetError();
		if (error == GL_NO_ERROR)
		{
			break;
		}
		ALOGE("GL error on line %d: %s", line, GlErrorString(error));
	}
}

#define GL( func )		func; GLCheckErrors( __LINE__ );

#else // CHECK_GL_ERRORS

#define GL( func )		func;

#endif // CHECK_GL_ERRORS


/*
================================================================================

ovrEgl

================================================================================
*/
/*
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
*/
static void ovrEgl_Clear(ovrEgl* egl)
{
	egl->MajorVersion = 0;
	egl->MinorVersion = 0;
	egl->Display = 0;
	egl->Config = 0;
	egl->TinySurface = EGL_NO_SURFACE;
	egl->MainSurface = EGL_NO_SURFACE;
	egl->Context = EGL_NO_CONTEXT;
}

static void ovrEgl_CreateContext(ovrEgl* egl, const ovrEgl* shareEgl)
{
	if (egl->Display != 0)
	{
		return;
	}

	egl->Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	ALOGV("        eglInitialize( Display, &MajorVersion, &MinorVersion )");
	eglInitialize(egl->Display, &egl->MajorVersion, &egl->MinorVersion);
	// Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
	// flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
	// settings, and that is completely wasted for our warp target.
	const int MAX_CONFIGS = 1024;
	EGLConfig configs[MAX_CONFIGS];
	EGLint numConfigs = 0;
	if (eglGetConfigs(egl->Display, configs, MAX_CONFIGS, &numConfigs) == EGL_FALSE)
	{
		ALOGE("        eglGetConfigs() failed: %s", EglErrorString(eglGetError()));
		return;
	}
	const EGLint configAttribs[] =
	{
		EGL_RED_SIZE,		8,
		EGL_GREEN_SIZE,		8,
		EGL_BLUE_SIZE,		8,
		EGL_ALPHA_SIZE,		8, // need alpha for the multi-pass timewarp compositor
		EGL_DEPTH_SIZE,		0,
		EGL_STENCIL_SIZE,	0,
		EGL_SAMPLES,		0,
		EGL_NONE
	};
	egl->Config = 0;
	for (int i = 0; i < numConfigs; i++)
	{
		EGLint value = 0;

		eglGetConfigAttrib(egl->Display, configs[i], EGL_RENDERABLE_TYPE, &value);
		if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR)
		{
			continue;
		}

		// The pbuffer config also needs to be compatible with normal window rendering
		// so it can share textures with the window context.
		eglGetConfigAttrib(egl->Display, configs[i], EGL_SURFACE_TYPE, &value);
		if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT))
		{
			continue;
		}

		int	j = 0;
		for (; configAttribs[j] != EGL_NONE; j += 2)
		{
			eglGetConfigAttrib(egl->Display, configs[i], configAttribs[j], &value);
			if (value != configAttribs[j + 1])
			{
				break;
			}
		}
		if (configAttribs[j] == EGL_NONE)
		{
			egl->Config = configs[i];
			break;
		}
	}
	if (egl->Config == 0)
	{
		ALOGE("        eglChooseConfig() failed: %s", EglErrorString(eglGetError()));
		return;
	}
	EGLint contextAttribs[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};
	ALOGV("        Context = eglCreateContext( Display, Config, EGL_NO_CONTEXT, contextAttribs )");
	egl->Context = eglCreateContext(egl->Display, egl->Config, (shareEgl != NULL) ? shareEgl->Context : EGL_NO_CONTEXT, contextAttribs);
	if (egl->Context == EGL_NO_CONTEXT)
	{
		ALOGE("        eglCreateContext() failed: %s", EglErrorString(eglGetError()));
		return;
	}
	const EGLint surfaceAttribs[] =
	{
		EGL_WIDTH, 16,
		EGL_HEIGHT, 16,
//		EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_SRGB_KHR,
		EGL_NONE
	};
	ALOGV("        TinySurface = eglCreatePbufferSurface( Display, Config, surfaceAttribs )");
	egl->TinySurface = eglCreatePbufferSurface(egl->Display, egl->Config, surfaceAttribs);
	if (egl->TinySurface == EGL_NO_SURFACE)
	{
		ALOGE("        eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
		eglDestroyContext(egl->Display, egl->Context);
		egl->Context = EGL_NO_CONTEXT;
		return;
	}
	ALOGV("        eglMakeCurrent( Display, TinySurface, TinySurface, Context )");
	if (eglMakeCurrent(egl->Display, egl->TinySurface, egl->TinySurface, egl->Context) == EGL_FALSE)
	{
		ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
		eglDestroySurface(egl->Display, egl->TinySurface);
		eglDestroyContext(egl->Display, egl->Context);
		egl->Context = EGL_NO_CONTEXT;
		return;
	}
}

static void ovrEgl_DestroyContext(ovrEgl* egl)
{
	if (egl->Display != 0)
	{
		ALOGE("        eglMakeCurrent( Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )");
		if (eglMakeCurrent(egl->Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE)
		{
			ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
		}
	}
	if (egl->Context != EGL_NO_CONTEXT)
	{
		ALOGE("        eglDestroyContext( Display, Context )");
		if (eglDestroyContext(egl->Display, egl->Context) == EGL_FALSE)
		{
			ALOGE("        eglDestroyContext() failed: %s", EglErrorString(eglGetError()));
		}
		egl->Context = EGL_NO_CONTEXT;
	}
	if (egl->TinySurface != EGL_NO_SURFACE)
	{
		ALOGE("        eglDestroySurface( Display, TinySurface )");
		if (eglDestroySurface(egl->Display, egl->TinySurface) == EGL_FALSE)
		{
			ALOGE("        eglDestroySurface() failed: %s", EglErrorString(eglGetError()));
		}
		egl->TinySurface = EGL_NO_SURFACE;
	}
	if (egl->Display != 0)
	{
		ALOGE("        eglTerminate( Display )");
		if (eglTerminate(egl->Display) == EGL_FALSE)
		{
			ALOGE("        eglTerminate() failed: %s", EglErrorString(eglGetError()));
		}
		egl->Display = 0;
	}
}

/*
================================================================================

ovrGeometry

================================================================================
*/

typedef struct
{
	GLuint			Index;
	GLint			Size;
	GLenum			Type;
	GLboolean		Normalized;
	GLsizei			Stride;
	const GLvoid* Pointer;
} ovrVertexAttribPointer;

#define MAX_VERTEX_ATTRIB_POINTERS		3

typedef struct
{
	GLuint					VertexBuffer;
	GLuint					IndexBuffer;
	GLuint					VertexArrayObject;
	int						VertexCount;
	int 					IndexCount;
	ovrVertexAttribPointer	VertexAttribs[MAX_VERTEX_ATTRIB_POINTERS];
} ovrGeometry;

enum VertexAttributeLocation
{
	VERTEX_ATTRIBUTE_LOCATION_POSITION,
	VERTEX_ATTRIBUTE_LOCATION_COLOR,
	VERTEX_ATTRIBUTE_LOCATION_UV,
	VERTEX_ATTRIBUTE_LOCATION_TRANSFORM
};

typedef struct
{
	enum VertexAttributeLocation location;
	const char* name;
} ovrVertexAttribute;

static ovrVertexAttribute ProgramVertexAttributes[] =
{
	{ VERTEX_ATTRIBUTE_LOCATION_POSITION,	"vertexPosition" },
	{ VERTEX_ATTRIBUTE_LOCATION_COLOR,		"vertexColor" },
	{ VERTEX_ATTRIBUTE_LOCATION_UV,			"vertexUv" },
	{ VERTEX_ATTRIBUTE_LOCATION_TRANSFORM,	"vertexTransform" }
};

static void ovrGeometry_Clear(ovrGeometry* geometry)
{
	geometry->VertexBuffer = 0;
	geometry->IndexBuffer = 0;
	geometry->VertexArrayObject = 0;
	geometry->VertexCount = 0;
	geometry->IndexCount = 0;
	for (int i = 0; i < MAX_VERTEX_ATTRIB_POINTERS; i++)
	{
		memset(&geometry->VertexAttribs[i], 0, sizeof(geometry->VertexAttribs[i]));
		geometry->VertexAttribs[i].Index = -1;
	}
}

static void ovrGeometry_CreateCube(ovrGeometry* geometry)
{
	typedef struct
	{
		signed char positions[8][4];
		unsigned char colors[8][4];
	} ovrCubeVertices;

	static const ovrCubeVertices cubeVertices =
	{
		// positions
		{
			{ -127, +127, -127, +127 }, { +127, +127, -127, +127 }, { +127, +127, +127, +127 }, { -127, +127, +127, +127 },	// top
			{ -127, -127, -127, +127 }, { -127, -127, +127, +127 }, { +127, -127, +127, +127 }, { +127, -127, -127, +127 }	// bottom
		},
		// colors
		{
			{ 255,   0, 255, 255 }, {   0, 255,   0, 255 }, {   0,   0, 255, 255 }, { 255,   0,   0, 255 },
			{   0,   0, 255, 255 }, {   0, 255,   0, 255 }, { 255,   0, 255, 255 }, { 255,   0,   0, 255 }
		},
	};

	static const unsigned short cubeIndices[36] =
	{
		0, 2, 1, 2, 0, 3,	// top
		4, 6, 5, 6, 4, 7,	// bottom
		2, 6, 7, 7, 1, 2,	// right
		0, 4, 5, 5, 3, 0,	// left
		3, 5, 6, 6, 2, 3,	// front
		0, 1, 7, 7, 4, 0	// back
	};

	geometry->VertexCount = 8;
	geometry->IndexCount = 36;

	geometry->VertexAttribs[0].Index = VERTEX_ATTRIBUTE_LOCATION_POSITION;
	geometry->VertexAttribs[0].Size = 4;
	geometry->VertexAttribs[0].Type = GL_BYTE;
	geometry->VertexAttribs[0].Normalized = true;
	geometry->VertexAttribs[0].Stride = sizeof(cubeVertices.positions[0]);
	geometry->VertexAttribs[0].Pointer = (const GLvoid*)offsetof(ovrCubeVertices, positions);

	geometry->VertexAttribs[1].Index = VERTEX_ATTRIBUTE_LOCATION_COLOR;
	geometry->VertexAttribs[1].Size = 4;
	geometry->VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
	geometry->VertexAttribs[1].Normalized = true;
	geometry->VertexAttribs[1].Stride = sizeof(cubeVertices.colors[0]);
	geometry->VertexAttribs[1].Pointer = (const GLvoid*)offsetof(ovrCubeVertices, colors);

	GL(glGenBuffers(1, &geometry->VertexBuffer));
	GL(glBindBuffer(GL_ARRAY_BUFFER, geometry->VertexBuffer));
	GL(glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW));
	GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

	GL(glGenBuffers(1, &geometry->IndexBuffer));
	GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->IndexBuffer));
	GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW));
	GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

static void ovrGeometry_Destroy(ovrGeometry* geometry)
{
	GL(glDeleteBuffers(1, &geometry->IndexBuffer));
	GL(glDeleteBuffers(1, &geometry->VertexBuffer));

	ovrGeometry_Clear(geometry);
}

static void ovrGeometry_CreateVAO(ovrGeometry* geometry)
{
	GL(glGenVertexArrays(1, &geometry->VertexArrayObject));
	GL(glBindVertexArray(geometry->VertexArrayObject));

	GL(glBindBuffer(GL_ARRAY_BUFFER, geometry->VertexBuffer));

	for (int i = 0; i < MAX_VERTEX_ATTRIB_POINTERS; i++)
	{
		if (geometry->VertexAttribs[i].Index != -1)
		{
			GL(glEnableVertexAttribArray(geometry->VertexAttribs[i].Index));
			GL(glVertexAttribPointer(geometry->VertexAttribs[i].Index, geometry->VertexAttribs[i].Size,
				geometry->VertexAttribs[i].Type, geometry->VertexAttribs[i].Normalized,
				geometry->VertexAttribs[i].Stride, geometry->VertexAttribs[i].Pointer));
		}
	}

	GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geometry->IndexBuffer));

	GL(glBindVertexArray(0));
}

static void ovrGeometry_DestroyVAO(ovrGeometry* geometry)
{
	GL(glDeleteVertexArrays(1, &geometry->VertexArrayObject));
}

/*
================================================================================

ovrProgram

================================================================================
*/

#define MAX_PROGRAM_UNIFORMS	8
#define MAX_PROGRAM_TEXTURES	8

typedef struct
{
	GLuint	Program;
	GLuint	VertexShader;
	GLuint	FragmentShader;
	// These will be -1 if not used by the program.
	GLint	UniformLocation[MAX_PROGRAM_UNIFORMS];	// ProgramUniforms[].name
	GLint	UniformBinding[MAX_PROGRAM_UNIFORMS];	// ProgramUniforms[].name
	GLint	Textures[MAX_PROGRAM_TEXTURES];			// Texture%i
} ovrProgram;

typedef struct
{
	enum
	{
		UNIFORM_MODEL_MATRIX,
		UNIFORM_VIEW_ID,
		UNIFORM_SCENE_MATRICES,
	}				index;
	enum
	{
		UNIFORM_TYPE_VECTOR4,
		UNIFORM_TYPE_MATRIX4X4,
		UNIFORM_TYPE_INT,
		UNIFORM_TYPE_BUFFER,
	}				type;
	const char* name;
} ovrUniform;

static ovrUniform ProgramUniforms[] =
{
	{ UNIFORM_MODEL_MATRIX,			UNIFORM_TYPE_MATRIX4X4,	"ModelMatrix"	},
	{ UNIFORM_VIEW_ID,				UNIFORM_TYPE_INT,       "ViewID"		},
	{ UNIFORM_SCENE_MATRICES,		UNIFORM_TYPE_BUFFER,	"SceneMatrices" },
};

static void ovrProgram_Clear(ovrProgram* program)
{
	program->Program = 0;
	program->VertexShader = 0;
	program->FragmentShader = 0;
	memset(program->UniformLocation, 0, sizeof(program->UniformLocation));
	memset(program->UniformBinding, 0, sizeof(program->UniformBinding));
	memset(program->Textures, 0, sizeof(program->Textures));
}

static const char* programVersion = "#version 300 es\n";

static bool ovrProgram_Create(ovrProgram* program, const char* vertexSource, const char* fragmentSource, const bool useMultiview)
{
	GLint r;

	GL(program->VertexShader = glCreateShader(GL_VERTEX_SHADER));

	const char* vertexSources[3] = { programVersion,
								(useMultiview) ? "#define DISABLE_MULTIVIEW 0\n" : "#define DISABLE_MULTIVIEW 1\n",
								vertexSource
	};
	GL(glShaderSource(program->VertexShader, 3, vertexSources, 0));
	GL(glCompileShader(program->VertexShader));
	GL(glGetShaderiv(program->VertexShader, GL_COMPILE_STATUS, &r));
	if (r == GL_FALSE)
	{
		GLchar msg[4096];
		GL(glGetShaderInfoLog(program->VertexShader, sizeof(msg), 0, msg));
		ALOGE("%s\n%s\n", vertexSource, msg);
		return false;
	}

	const char* fragmentSources[2] = { programVersion, fragmentSource };
	GL(program->FragmentShader = glCreateShader(GL_FRAGMENT_SHADER));
	GL(glShaderSource(program->FragmentShader, 2, fragmentSources, 0));
	GL(glCompileShader(program->FragmentShader));
	GL(glGetShaderiv(program->FragmentShader, GL_COMPILE_STATUS, &r));
	if (r == GL_FALSE)
	{
		GLchar msg[4096];
		GL(glGetShaderInfoLog(program->FragmentShader, sizeof(msg), 0, msg));
		ALOGE("%s\n%s\n", fragmentSource, msg);
		return false;
	}

	GL(program->Program = glCreateProgram());
	GL(glAttachShader(program->Program, program->VertexShader));
	GL(glAttachShader(program->Program, program->FragmentShader));

	// Bind the vertex attribute locations.
	for (int i = 0; i < sizeof(ProgramVertexAttributes) / sizeof(ProgramVertexAttributes[0]); i++)
	{
		GL(glBindAttribLocation(program->Program, ProgramVertexAttributes[i].location, ProgramVertexAttributes[i].name));
	}

	GL(glLinkProgram(program->Program));
	GL(glGetProgramiv(program->Program, GL_LINK_STATUS, &r));
	if (r == GL_FALSE)
	{
		GLchar msg[4096];
		GL(glGetProgramInfoLog(program->Program, sizeof(msg), 0, msg));
		ALOGE("Linking program failed: %s\n", msg);
		return false;
	}

	int numBufferBindings = 0;

	// Get the uniform locations.
	memset(program->UniformLocation, -1, sizeof(program->UniformLocation));
	for (int i = 0; i < sizeof(ProgramUniforms) / sizeof(ProgramUniforms[0]); i++)
	{
		const int uniformIndex = ProgramUniforms[i].index;
		if (ProgramUniforms[i].type == UNIFORM_TYPE_BUFFER)
		{
			GL(program->UniformLocation[uniformIndex] = glGetUniformBlockIndex(program->Program, ProgramUniforms[i].name));
			program->UniformBinding[uniformIndex] = numBufferBindings++;
			GL(glUniformBlockBinding(program->Program, program->UniformLocation[uniformIndex], program->UniformBinding[uniformIndex]));
		}
		else
		{
			GL(program->UniformLocation[uniformIndex] = glGetUniformLocation(program->Program, ProgramUniforms[i].name));
			program->UniformBinding[uniformIndex] = program->UniformLocation[uniformIndex];
		}
	}

	GL(glUseProgram(program->Program));

	// Get the texture locations.
	for (int i = 0; i < MAX_PROGRAM_TEXTURES; i++)
	{
		char name[32];
		sprintf(name, "Texture%i", i);
		program->Textures[i] = glGetUniformLocation(program->Program, name);
		if (program->Textures[i] != -1)
		{
			GL(glUniform1i(program->Textures[i], i));
		}
	}

	GL(glUseProgram(0));

	return true;
}

static void ovrProgram_Destroy(ovrProgram* program)
{
	if (program->Program != 0)
	{
		GL(glDeleteProgram(program->Program));
		program->Program = 0;
	}
	if (program->VertexShader != 0)
	{
		GL(glDeleteShader(program->VertexShader));
		program->VertexShader = 0;
	}
	if (program->FragmentShader != 0)
	{
		GL(glDeleteShader(program->FragmentShader));
		program->FragmentShader = 0;
	}
}

static const char VERTEX_SHADER[] =
"#ifndef DISABLE_MULTIVIEW\n"
"	#define DISABLE_MULTIVIEW 0\n"
"#endif\n"
"#define NUM_VIEWS 2\n"
"#if defined( GL_OVR_multiview2 ) && ! DISABLE_MULTIVIEW\n"
"	#extension GL_OVR_multiview2 : enable\n"
"	layout(num_views=NUM_VIEWS) in;\n"
"	#define VIEW_ID gl_ViewID_OVR\n"
"#else\n"
"	uniform lowp int ViewID;\n"
"	#define VIEW_ID ViewID\n"
"#endif\n"
"in vec3 vertexPosition;\n"
"in vec4 vertexColor;\n"
"in mat4 vertexTransform;\n"
"uniform SceneMatrices\n"
"{\n"
"	uniform mat4 ViewMatrix[NUM_VIEWS];\n"
"	uniform mat4 ProjectionMatrix[NUM_VIEWS];\n"
"} sm;\n"
"out vec4 fragmentColor;\n"
"void main()\n"
"{\n"
"	gl_Position = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( vertexTransform * vec4( vertexPosition * 0.1, 1.0 ) ) );\n"
"	fragmentColor = vertexColor;\n"
"}\n";

static const char FRAGMENT_SHADER[] =
"in lowp vec4 fragmentColor;\n"
"out lowp vec4 outColor;\n"
"void main()\n"
"{\n"
"	outColor = fragmentColor;\n"
"}\n";

/*
================================================================================

ovrFramebuffer

================================================================================
*/

typedef struct
{
	int						Width;
	int						Height;
	int						Multisamples;
	int						TextureSwapChainLength;
	int						TextureSwapChainIndex;
	bool					UseMultiview;
	ovrTextureSwapChain* ColorTextureSwapChain;
	GLuint* DepthBuffers;
	GLuint* FrameBuffers;
} ovrFramebuffer;

static void ovrFramebuffer_Clear(ovrFramebuffer* frameBuffer)
{
	frameBuffer->Width = 0;
	frameBuffer->Height = 0;
	frameBuffer->Multisamples = 0;
	frameBuffer->TextureSwapChainLength = 0;
	frameBuffer->TextureSwapChainIndex = 0;
	frameBuffer->UseMultiview = false;
	frameBuffer->ColorTextureSwapChain = NULL;
	frameBuffer->DepthBuffers = NULL;
	frameBuffer->FrameBuffers = NULL;
}

static bool ovrFramebuffer_Create(ovrFramebuffer* frameBuffer, const bool useMultiview, const GLenum colorFormat, const int width, const int height, const int multisamples)
{
	PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT =
		(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress("glRenderbufferStorageMultisampleEXT");
	PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT =
		(PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT");

	PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR =
		(PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)eglGetProcAddress("glFramebufferTextureMultiviewOVR");
	PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR =
		(PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)eglGetProcAddress("glFramebufferTextureMultisampleMultiviewOVR");

	frameBuffer->Width = width;
	frameBuffer->Height = height;
	frameBuffer->Multisamples = multisamples;
	frameBuffer->UseMultiview = (useMultiview && (glFramebufferTextureMultiviewOVR != NULL)) ? true : false;
	frameBuffer->TextureSwapChainIndex = 0;

	frameBuffer->ColorTextureSwapChain = vrapi_CreateTextureSwapChain3(frameBuffer->UseMultiview ? VRAPI_TEXTURE_TYPE_2D_ARRAY : VRAPI_TEXTURE_TYPE_2D, colorFormat, width, height, 1, 3);
	frameBuffer->TextureSwapChainLength = vrapi_GetTextureSwapChainLength(frameBuffer->ColorTextureSwapChain);
	frameBuffer->DepthBuffers = (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));
	frameBuffer->FrameBuffers = (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));

	ALOGV("        frameBuffer->UseMultiview = %d", frameBuffer->UseMultiview);

	for (int i = 0; i < frameBuffer->TextureSwapChainLength; i++)
	{
		// Create the color buffer texture.
		const GLuint colorTexture = vrapi_GetTextureSwapChainHandle(frameBuffer->ColorTextureSwapChain, i);
		GLenum colorTextureTarget = frameBuffer->UseMultiview ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
		GL(glBindTexture(colorTextureTarget, colorTexture));
		if (glExtensions.EXT_texture_border_clamp)
		{
			GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
			GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
			GLfloat borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			GL(glTexParameterfv(colorTextureTarget, GL_TEXTURE_BORDER_COLOR, borderColor));
		}
		else
		{
			// Just clamp to edge. However, this requires manually clearing the border
			// around the layer to clear the edge texels.
			GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
			GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		}
		GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GL(glBindTexture(colorTextureTarget, 0));

		if (frameBuffer->UseMultiview)
		{
			// Create the depth buffer texture.
			GL(glGenTextures(1, &frameBuffer->DepthBuffers[i]));
			GL(glBindTexture(GL_TEXTURE_2D_ARRAY, frameBuffer->DepthBuffers[i]));
			GL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT24, width, height, 2));
			GL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));

			// Create the frame buffer.
			GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
			GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
			if (multisamples > 1 && (glFramebufferTextureMultisampleMultiviewOVR != NULL))
			{
				GL(glFramebufferTextureMultisampleMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, frameBuffer->DepthBuffers[i], 0 /* level */, multisamples /* samples */, 0 /* baseViewIndex */, 2 /* numViews */));
				GL(glFramebufferTextureMultisampleMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture, 0 /* level */, multisamples /* samples */, 0 /* baseViewIndex */, 2 /* numViews */));
			}
			else
			{
				GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, frameBuffer->DepthBuffers[i], 0 /* level */, 0 /* baseViewIndex */, 2 /* numViews */));
				GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture, 0 /* level */, 0 /* baseViewIndex */, 2 /* numViews */));
			}

			GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
			GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
			if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE)
			{
				ALOGE("Incomplete frame buffer object: %s", GlFrameBufferStatusString(renderFramebufferStatus));
				return false;
			}
		}
		else
		{
			if (multisamples > 1 && glRenderbufferStorageMultisampleEXT != NULL && glFramebufferTexture2DMultisampleEXT != NULL)
			{
				// Create multisampled depth buffer.
				GL(glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]));
				GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
				GL(glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, multisamples, GL_DEPTH_COMPONENT24, width, height));
				GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

				// Create the frame buffer.
				// NOTE: glFramebufferTexture2DMultisampleEXT only works with GL_FRAMEBUFFER.
				GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
				GL(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
				GL(glFramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0, multisamples));
				GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
				GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER));
				GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
				if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE)
				{
					ALOGE("Incomplete frame buffer object: %s", GlFrameBufferStatusString(renderFramebufferStatus));
					return false;
				}
			}
			else
			{
				// Create depth buffer.
				GL(glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]));
				GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
				GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height));
				GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

				// Create the frame buffer.
				GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
				GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
				GL(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
				GL(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0));
				GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
				GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
				if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE)
				{
					ALOGE("Incomplete frame buffer object: %s", GlFrameBufferStatusString(renderFramebufferStatus));
					return false;
				}
			}
		}
	}

	return true;
}

static void ovrFramebuffer_Destroy(ovrFramebuffer* frameBuffer)
{
	GL(glDeleteFramebuffers(frameBuffer->TextureSwapChainLength, frameBuffer->FrameBuffers));
	if (frameBuffer->UseMultiview)
	{
		GL(glDeleteTextures(frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers));
	}
	else
	{
		GL(glDeleteRenderbuffers(frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers));
	}
	vrapi_DestroyTextureSwapChain(frameBuffer->ColorTextureSwapChain);

	free(frameBuffer->DepthBuffers);
	free(frameBuffer->FrameBuffers);

	ovrFramebuffer_Clear(frameBuffer);
}

static void ovrFramebuffer_SetCurrent(ovrFramebuffer* frameBuffer)
{
	GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[frameBuffer->TextureSwapChainIndex]));
}

static void ovrFramebuffer_SetNone()
{
	GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
}

static void ovrFramebuffer_Resolve(ovrFramebuffer* frameBuffer)
{
	// Discard the depth buffer, so the tiler won't need to write it back out to memory.
	const GLenum depthAttachment[1] = { GL_DEPTH_ATTACHMENT };
	glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, depthAttachment);

	// Flush this frame worth of commands.
	glFlush();
}

static void ovrFramebuffer_Advance(ovrFramebuffer* frameBuffer)
{
	// Advance to the next texture from the set.
	frameBuffer->TextureSwapChainIndex = (frameBuffer->TextureSwapChainIndex + 1) % frameBuffer->TextureSwapChainLength;
}

/*
================================================================================

ovrScene

================================================================================
*/

#define NUM_INSTANCES		1500
#define NUM_ROTATIONS		16

typedef struct
{
	bool				CreatedScene;
	bool				CreatedVAOs;
	unsigned int		Random;
	ovrProgram			Program;
	ovrGeometry			Cube;
	GLuint				SceneMatrices;
	GLuint				InstanceTransformBuffer;
	ovrVector3f			Rotations[NUM_ROTATIONS];
	ovrVector3f			CubePositions[NUM_INSTANCES];
	int					CubeRotations[NUM_INSTANCES];
} ovrScene;

static void ovrScene_Clear(ovrScene* scene)
{
	scene->CreatedScene = false;
	scene->CreatedVAOs = false;
	scene->Random = 2;
	scene->SceneMatrices = 0;
	scene->InstanceTransformBuffer = 0;

	ovrProgram_Clear(&scene->Program);
	ovrGeometry_Clear(&scene->Cube);
}

static bool ovrScene_IsCreated(ovrScene* scene)
{
	return scene->CreatedScene;
}

static void ovrScene_CreateVAOs(ovrScene* scene)
{
	if (!scene->CreatedVAOs)
	{
		ovrGeometry_CreateVAO(&scene->Cube);

		// Modify the VAO to use the instance transform attributes.
		GL(glBindVertexArray(scene->Cube.VertexArrayObject));
		GL(glBindBuffer(GL_ARRAY_BUFFER, scene->InstanceTransformBuffer));
		for (int i = 0; i < 4; i++)
		{
			GL(glEnableVertexAttribArray(VERTEX_ATTRIBUTE_LOCATION_TRANSFORM + i));
			GL(glVertexAttribPointer(VERTEX_ATTRIBUTE_LOCATION_TRANSFORM + i, 4, GL_FLOAT,
				false, 4 * 4 * sizeof(float), (void*)(i * 4 * sizeof(float))));
			GL(glVertexAttribDivisor(VERTEX_ATTRIBUTE_LOCATION_TRANSFORM + i, 1));
		}
		GL(glBindVertexArray(0));

		scene->CreatedVAOs = true;
	}
}

static void ovrScene_DestroyVAOs(ovrScene* scene)
{
	if (scene->CreatedVAOs)
	{
		ovrGeometry_DestroyVAO(&scene->Cube);

		scene->CreatedVAOs = false;
	}
}

// Returns a random float in the range [0, 1].
static float ovrScene_RandomFloat(ovrScene* scene)
{
	scene->Random = 1664525L * scene->Random + 1013904223L;
	unsigned int rf = 0x3F800000 | (scene->Random & 0x007FFFFF);
	return (*(float*)&rf) - 1.0f;
}

static void ovrScene_Create(ovrScene* scene, bool useMultiview)
{
	ovrProgram_Create(&scene->Program, VERTEX_SHADER, FRAGMENT_SHADER, useMultiview);
	ovrGeometry_CreateCube(&scene->Cube);

	// Create the instance transform attribute buffer.
	GL(glGenBuffers(1, &scene->InstanceTransformBuffer));
	GL(glBindBuffer(GL_ARRAY_BUFFER, scene->InstanceTransformBuffer));
	GL(glBufferData(GL_ARRAY_BUFFER, NUM_INSTANCES * 4 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW));
	GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

	// Setup the scene matrices.
	GL(glGenBuffers(1, &scene->SceneMatrices));
	GL(glBindBuffer(GL_UNIFORM_BUFFER, scene->SceneMatrices));
	GL(glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(ovrMatrix4f) /* 2 view matrices */ + 2 * sizeof(ovrMatrix4f) /* 2 projection matrices */,
		NULL, GL_STATIC_DRAW));
	GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));

	// Setup random rotations.
	for (int i = 0; i < NUM_ROTATIONS; i++)
	{
		scene->Rotations[i].x = ovrScene_RandomFloat(scene);
		scene->Rotations[i].y = ovrScene_RandomFloat(scene);
		scene->Rotations[i].z = ovrScene_RandomFloat(scene);
	}

	// Setup random cube positions and rotations.
	for (int i = 0; i < NUM_INSTANCES; i++)
	{
		// Using volatile keeps the compiler from optimizing away multiple calls to ovrScene_RandomFloat().
		volatile float rx, ry, rz;
		for (; ; )
		{
			rx = (ovrScene_RandomFloat(scene) - 0.5f) * (50.0f + sqrt(NUM_INSTANCES));
			ry = (ovrScene_RandomFloat(scene) - 0.5f) * (50.0f + sqrt(NUM_INSTANCES));
			rz = (ovrScene_RandomFloat(scene) - 0.5f) * (50.0f + sqrt(NUM_INSTANCES));
			// If too close to 0,0,0
			if (fabsf(rx) < 4.0f && fabsf(ry) < 4.0f && fabsf(rz) < 4.0f)
			{
				continue;
			}
			// Test for overlap with any of the existing cubes.
			bool overlap = false;
			for (int j = 0; j < i; j++)
			{
				if (fabsf(rx - scene->CubePositions[j].x) < 4.0f &&
					fabsf(ry - scene->CubePositions[j].y) < 4.0f &&
					fabsf(rz - scene->CubePositions[j].z) < 4.0f)
				{
					overlap = true;
					break;
				}
			}
			if (!overlap)
			{
				break;
			}
		}

		rx *= 0.1f;
		ry *= 0.1f;
		rz *= 0.1f;

		// Insert into list sorted based on distance.
		int insert = 0;
		const float distSqr = rx * rx + ry * ry + rz * rz;
		for (int j = i; j > 0; j--)
		{
			const ovrVector3f* otherPos = &scene->CubePositions[j - 1];
			const float otherDistSqr = otherPos->x * otherPos->x + otherPos->y * otherPos->y + otherPos->z * otherPos->z;
			if (distSqr > otherDistSqr)
			{
				insert = j;
				break;
			}
			scene->CubePositions[j] = scene->CubePositions[j - 1];
			scene->CubeRotations[j] = scene->CubeRotations[j - 1];
		}

		scene->CubePositions[insert].x = rx;
		scene->CubePositions[insert].y = ry;
		scene->CubePositions[insert].z = rz;

		scene->CubeRotations[insert] = (int)(ovrScene_RandomFloat(scene) * (NUM_ROTATIONS - 0.1f));
	}

	scene->CreatedScene = true;

#if !MULTI_THREADED
	ovrScene_CreateVAOs(scene);
#endif
}

static void ovrScene_Destroy(ovrScene* scene)
{
#if !MULTI_THREADED
	ovrScene_DestroyVAOs(scene);
#endif

	ovrProgram_Destroy(&scene->Program);
	ovrGeometry_Destroy(&scene->Cube);
	GL(glDeleteBuffers(1, &scene->InstanceTransformBuffer));
	GL(glDeleteBuffers(1, &scene->SceneMatrices));
	scene->CreatedScene = false;
}

/*
================================================================================

ovrSimulation

================================================================================
*/

typedef struct
{
	ovrVector3f			CurrentRotation;
} ovrSimulation;

static void ovrSimulation_Clear(ovrSimulation* simulation)
{
	simulation->CurrentRotation.x = 0.0f;
	simulation->CurrentRotation.y = 0.0f;
	simulation->CurrentRotation.z = 0.0f;
}

static void ovrSimulation_Advance(ovrSimulation* simulation, double elapsedDisplayTime)
{
	// Update rotation.
	simulation->CurrentRotation.x = (float)(elapsedDisplayTime);
	simulation->CurrentRotation.y = (float)(elapsedDisplayTime);
	simulation->CurrentRotation.z = (float)(elapsedDisplayTime);
}

/*
================================================================================

ovrRenderer

================================================================================
*/

typedef struct
{
	ovrFramebuffer	FrameBuffer[VRAPI_FRAME_LAYER_EYE_MAX];
	int				NumBuffers;
} ovrRenderer;

static void ovrRenderer_Clear(ovrRenderer* renderer)
{
	for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
	{
		ovrFramebuffer_Clear(&renderer->FrameBuffer[eye]);
	}
	renderer->NumBuffers = VRAPI_FRAME_LAYER_EYE_MAX;
}

static void ovrRenderer_Create(ovrRenderer* renderer, const ovrJava* java, const bool useMultiview)
{
	renderer->NumBuffers = useMultiview ? 1 : VRAPI_FRAME_LAYER_EYE_MAX;

	// Create the frame buffers.
	for (int eye = 0; eye < renderer->NumBuffers; eye++)
	{
		ovrFramebuffer_Create(&renderer->FrameBuffer[eye], useMultiview,
			GL_RGBA8,
			vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH),
			vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT),
			NUM_MULTI_SAMPLES);

	}
}

static void ovrRenderer_Destroy(ovrRenderer* renderer)
{
	for (int eye = 0; eye < renderer->NumBuffers; eye++)
	{
		ovrFramebuffer_Destroy(&renderer->FrameBuffer[eye]);
	}
}

static ovrLayerProjection2 ovrRenderer_RenderFrame(ovrRenderer* renderer, const ovrJava* java,
	/*const ovrScene* scene, const ovrSimulation* simulation,*/
	const ovrTracking2* tracking, ovrMobile* ovr)
{
#if 0
	ovrMatrix4f rotationMatrices[NUM_ROTATIONS];
	for (int i = 0; i < NUM_ROTATIONS; i++)
	{
		rotationMatrices[i] = ovrMatrix4f_CreateRotation(
			scene->Rotations[i].x * simulation->CurrentRotation.x,
			scene->Rotations[i].y * simulation->CurrentRotation.y,
			scene->Rotations[i].z * simulation->CurrentRotation.z);
	}

	// Update the instance transform attributes.
	GL(glBindBuffer(GL_ARRAY_BUFFER, scene->InstanceTransformBuffer));
	GL(ovrMatrix4f * cubeTransforms = (ovrMatrix4f*)glMapBufferRange(GL_ARRAY_BUFFER, 0,
		NUM_INSTANCES * sizeof(ovrMatrix4f), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
	for (int i = 0; i < NUM_INSTANCES; i++)
	{
		const int index = scene->CubeRotations[i];

		// Write in order in case the mapped buffer lives on write-combined memory.
		cubeTransforms[i].M[0][0] = rotationMatrices[index].M[0][0];
		cubeTransforms[i].M[0][1] = rotationMatrices[index].M[0][1];
		cubeTransforms[i].M[0][2] = rotationMatrices[index].M[0][2];
		cubeTransforms[i].M[0][3] = rotationMatrices[index].M[0][3];

		cubeTransforms[i].M[1][0] = rotationMatrices[index].M[1][0];
		cubeTransforms[i].M[1][1] = rotationMatrices[index].M[1][1];
		cubeTransforms[i].M[1][2] = rotationMatrices[index].M[1][2];
		cubeTransforms[i].M[1][3] = rotationMatrices[index].M[1][3];

		cubeTransforms[i].M[2][0] = rotationMatrices[index].M[2][0];
		cubeTransforms[i].M[2][1] = rotationMatrices[index].M[2][1];
		cubeTransforms[i].M[2][2] = rotationMatrices[index].M[2][2];
		cubeTransforms[i].M[2][3] = rotationMatrices[index].M[2][3];

		cubeTransforms[i].M[3][0] = scene->CubePositions[i].x;
		cubeTransforms[i].M[3][1] = scene->CubePositions[i].y;
		cubeTransforms[i].M[3][2] = scene->CubePositions[i].z;
		cubeTransforms[i].M[3][3] = 1.0f;
	}
	GL(glUnmapBuffer(GL_ARRAY_BUFFER));
	GL(glBindBuffer(GL_ARRAY_BUFFER, 0));
#endif
	ovrTracking2 updatedTracking = *tracking;
#if 0
	ovrMatrix4f eyeViewMatrixTransposed[2];
	eyeViewMatrixTransposed[0] = ovrMatrix4f_Transpose(&updatedTracking.Eye[0].ViewMatrix);
	eyeViewMatrixTransposed[1] = ovrMatrix4f_Transpose(&updatedTracking.Eye[1].ViewMatrix);

	ovrMatrix4f projectionMatrixTransposed[2];
	projectionMatrixTransposed[0] = ovrMatrix4f_Transpose(&updatedTracking.Eye[0].ProjectionMatrix);
	projectionMatrixTransposed[1] = ovrMatrix4f_Transpose(&updatedTracking.Eye[1].ProjectionMatrix);

	// Update the scene matrices.
	GL(glBindBuffer(GL_UNIFORM_BUFFER, scene->SceneMatrices));
	GL(ovrMatrix4f * sceneMatrices = (ovrMatrix4f*)glMapBufferRange(GL_UNIFORM_BUFFER, 0,
		2 * sizeof(ovrMatrix4f) /* 2 view matrices */ + 2 * sizeof(ovrMatrix4f) /* 2 projection matrices */,
		GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

	if (sceneMatrices != NULL)
	{
		memcpy((char*)sceneMatrices, &eyeViewMatrixTransposed, 2 * sizeof(ovrMatrix4f));
		memcpy((char*)sceneMatrices + 2 * sizeof(ovrMatrix4f), &projectionMatrixTransposed, 2 * sizeof(ovrMatrix4f));
	}

	GL(glUnmapBuffer(GL_UNIFORM_BUFFER));
	GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));
#endif
	ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
	layer.HeadPose = updatedTracking.HeadPose;
	for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++)
	{
		ovrFramebuffer* frameBuffer = &renderer->FrameBuffer[renderer->NumBuffers == 1 ? 0 : eye];
		layer.Textures[eye].ColorSwapChain = frameBuffer->ColorTextureSwapChain;
		layer.Textures[eye].SwapChainIndex = frameBuffer->TextureSwapChainIndex;
		layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&updatedTracking.Eye[eye].ProjectionMatrix);
	}
	layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	// Render the eye images.
	for (int eye = 0; eye < renderer->NumBuffers; eye++)
	{
		// NOTE: In the non-mv case, latency can be further reduced by updating the sensor prediction
		// for each eye (updates orientation, not position)
		ovrFramebuffer* frameBuffer = &renderer->FrameBuffer[eye];
		ovrFramebuffer_SetCurrent(frameBuffer);
#if 0
		GL(glUseProgram(scene->Program.Program));
		GL(glBindBufferBase(GL_UNIFORM_BUFFER, scene->Program.UniformBinding[UNIFORM_SCENE_MATRICES], scene->SceneMatrices));
		if (scene->Program.UniformLocation[UNIFORM_VIEW_ID] >= 0)  // NOTE: will not be present when multiview path is enabled.
		{
			GL(glUniform1i(scene->Program.UniformLocation[UNIFORM_VIEW_ID], eye));
		}
#endif
		GL(glEnable(GL_SCISSOR_TEST));
		GL(glDepthMask(GL_TRUE));
		GL(glEnable(GL_DEPTH_TEST));
		GL(glDepthFunc(GL_LEQUAL));
		GL(glEnable(GL_CULL_FACE));
		GL(glCullFace(GL_BACK));
		GL(glViewport(0, 0, frameBuffer->Width, frameBuffer->Height));
		GL(glScissor(0, 0, frameBuffer->Width, frameBuffer->Height));
		GL(glClearColor(0.125f, 0.0f, 0.125f, 1.0f));
		GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
#if 0
		GL(glBindVertexArray(scene->Cube.VertexArrayObject));
		GL(glDrawElementsInstanced(GL_TRIANGLES, scene->Cube.IndexCount, GL_UNSIGNED_SHORT, NULL, NUM_INSTANCES));
		GL(glBindVertexArray(0));
		GL(glUseProgram(0));
#endif
		// Explicitly clear the border texels to black when GL_CLAMP_TO_BORDER is not available.
		if (glExtensions.EXT_texture_border_clamp == false)
		{
			// Clear to fully opaque black.
			GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
			// bottom
			GL(glScissor(0, 0, frameBuffer->Width, 1));
			GL(glClear(GL_COLOR_BUFFER_BIT));
			// top
			GL(glScissor(0, frameBuffer->Height - 1, frameBuffer->Width, 1));
			GL(glClear(GL_COLOR_BUFFER_BIT));
			// left
			GL(glScissor(0, 0, 1, frameBuffer->Height));
			GL(glClear(GL_COLOR_BUFFER_BIT));
			// right
			GL(glScissor(frameBuffer->Width - 1, 0, 1, frameBuffer->Height));
			GL(glClear(GL_COLOR_BUFFER_BIT));
		}

		ovrFramebuffer_Resolve(frameBuffer);
		ovrFramebuffer_Advance(frameBuffer);
	}

	ovrFramebuffer_SetNone();

	return layer;
}

/*
================================================================================

ovrApp

================================================================================
*/

static void ovrApp_Clear(ovrApp* app)
{
	app->Java.Vm = NULL;
	app->Java.Env = NULL;
	app->Java.ActivityObject = NULL;
	app->NativeWindow = NULL;
	app->Resumed = false;
	app->Ovr = NULL;
	app->FrameIndex = 1;
	app->DisplayTime = 0;
	app->SwapInterval = 1;
	app->CpuLevel = 2;
	app->GpuLevel = 2;
	app->MainThreadTid = 0;
	app->RenderThreadTid = 0;
	app->RefreshRate = 0;
	app->BackButtonDownLastFrame = false;
	app->UseMultiview = true;

	ovrEgl_Clear(&app->Egl);
//	ovrScene_Clear(&app->Scene);
//	ovrSimulation_Clear(&app->Simulation);
#if 0
#if MULTI_THREADED
	ovrRenderThread_Clear(&app->RenderThread);
#else
	ovrRenderer_Clear(&app->Renderer);
#endif
#endif
}

static void ovrApp_PushBlackFinal(ovrApp* app)
{
#if MULTI_THREADED
	ovrRenderThread_Submit(&app->RenderThread, app->Ovr,
		RENDER_BLACK_FINAL, app->FrameIndex, app->DisplayTime, app->SwapInterval,
		NULL, NULL, NULL);
#else
	int frameFlags = 0;
	frameFlags |= VRAPI_FRAME_FLAG_FLUSH | VRAPI_FRAME_FLAG_FINAL;

	ovrLayerProjection2 layer = vrapi_DefaultLayerBlackProjection2();
	layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

	const ovrLayerHeader2* layers[] =
	{
		&layer.Header
	};

	ovrSubmitFrameDescription2 frameDesc = { 0 };
	frameDesc.Flags = frameFlags;
	frameDesc.SwapInterval = 1;
	frameDesc.FrameIndex = app->FrameIndex;
	frameDesc.DisplayTime = app->DisplayTime;
	frameDesc.LayerCount = 1;
	frameDesc.Layers = layers;

	vrapi_SubmitFrame2(app->Ovr, &frameDesc);
#endif
}

static void ovrApp_HandleVrModeChanges(ovrApp* app)
{
	if (app->Resumed != false && app->NativeWindow != NULL)
	{
		if (app->Ovr == NULL)
		{
			ovrModeParms parms = vrapi_DefaultModeParms(&app->Java);
			// No need to reset the FLAG_FULLSCREEN window flag when using a View
			parms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;

			parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
			parms.Flags |= VRAPI_MODE_FLAG_FRONT_BUFFER_SRGB;
//			parms.Flags |= VRAPI_MODE_FLAG_PHASE_SYNC;
			parms.Display = (size_t)app->Egl.Display;
			parms.WindowSurface = (size_t)app->NativeWindow;
			parms.ShareContext = (size_t)app->Egl.Context;

			ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

			ALOGV("        vrapi_EnterVrMode()");

			app->Ovr = vrapi_EnterVrMode(&parms);

			ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

			// If entering VR mode failed then the ANativeWindow was not valid.
			if (app->Ovr == NULL)
			{
				ALOGE("Invalid ANativeWindow!");
				app->NativeWindow = NULL;
			}

			// Set performance parameters once we have entered VR mode and have a valid ovrMobile.
			if (app->Ovr != NULL)
			{
				vrapi_SetClockLevels(app->Ovr, app->CpuLevel, app->GpuLevel);

				ALOGV("		vrapi_SetClockLevels( %d, %d )", app->CpuLevel, app->GpuLevel);

				vrapi_SetPerfThread(app->Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, app->MainThreadTid);

				ALOGV("		vrapi_SetPerfThread( MAIN, %d )", app->MainThreadTid);

				vrapi_SetPerfThread(app->Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, app->RenderThreadTid);

				ALOGV("		vrapi_SetPerfThread( RENDERER, %d )", app->RenderThreadTid);
			}
		}
	}
	else
	{
		if (app->Ovr != NULL)
		{
#if MULTI_THREADED
			// Make sure the renderer thread is no longer using the ovrMobile.
			ovrRenderThread_Wait(&app->RenderThread);
#endif
			ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

			ALOGV("        vrapi_LeaveVrMode()");

			vrapi_LeaveVrMode(app->Ovr);
			app->Ovr = NULL;

			ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));
		}
	}
}

static void ovrApp_HandleInput(ovrApp* app)
{
//	rsys_consoleClear();
	app->ControllerIDs[0] = 0;
	app->ControllerIDs[1] = 0;
	app->HandIDs[0] = 0;
	app->HandIDs[1] = 0;
	int32_t deviceCount = 0;
	bool backButtonDownThisFrame = false;
	for (int i = 0; ; i++)
	{
		ovrInputCapabilityHeader cap;
		ovrResult result = vrapi_EnumerateInputDevices(app->Ovr, i, &cap);
		if (result < 0)
		{
			break;
		}
		deviceCount++;
		if (cap.Type == ovrControllerType_TrackedRemote)
		{
			ovrInputStateTrackedRemote trackedRemoteState;
			trackedRemoteState.Header.ControllerType = ovrControllerType_TrackedRemote;
			result = vrapi_GetCurrentInputState(app->Ovr, cap.DeviceID, &trackedRemoteState.Header);
			if (result == ovrSuccess)
			{
				backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_Back;

				ovrInputTrackedRemoteCapabilities remoteCaps;
				remoteCaps.Header = cap;
				vrapi_GetInputDeviceCapabilities(app->Ovr, &remoteCaps.Header);
				int32_t deviceIndex = remoteCaps.ControllerCapabilities & ovrControllerCaps_LeftHand ? 0 : 1;
				app->ControllerIDs[deviceIndex] = cap.DeviceID;
				memcpy(&app->ControllerState[deviceIndex], &trackedRemoteState, sizeof(ovrInputStateTrackedRemote));
			}
		}

		if (cap.Type == ovrControllerType_Hand)
		{
			ovrInputStateHand handState;
			handState.Header.ControllerType = ovrControllerType_Hand;
			result = vrapi_GetCurrentInputState(app->Ovr, cap.DeviceID, &handState.Header);
			if (result == ovrSuccess)
			{
//				backButtonDownThisFrame |= trackedRemoteState.Buttons & ovrButton_Back;

				ovrInputHandCapabilities remoteCaps;
				remoteCaps.Header = cap;
				result = vrapi_GetInputDeviceCapabilities(app->Ovr, &remoteCaps.Header);
				ovrHandedness controllerHand = remoteCaps.HandCapabilities & ovrHandCaps_LeftHand ? VRAPI_HAND_LEFT : VRAPI_HAND_RIGHT;
				assert((int32_t)controllerHand != VRAPI_HAND_UNKNOWN);
				int32_t deviceIndex = (int32_t)controllerHand - 1;
				app->HandIDs[deviceIndex] = cap.DeviceID;
				memcpy(&app->HandState[deviceIndex], &handState, sizeof(ovrInputStateHand));
//				rsys_consoleLog("Hand: = %d (flags = 0x%04x)", cap.DeviceID, handState.InputStateStatus);
			}
		}
	}
//	rsys_consoleLog("#Devices = %d", deviceCount);

	bool backButtonDownLastFrame = app->BackButtonDownLastFrame;
	app->BackButtonDownLastFrame = backButtonDownThisFrame;
/*
	if (backButtonDownLastFrame && !backButtonDownThisFrame)
	{
		ALOGV("back button short press");
		ALOGV("        ovrApp_PushBlackFinal()");
		ovrApp_PushBlackFinal(app);
		ALOGV("        vrapi_ShowSystemUI( confirmQuit )");
		vrapi_ShowSystemUI(&app->Java, VRAPI_SYS_UI_CONFIRM_QUIT_MENU);
	}
*/
}

static void ovrApp_HandleVrApiEvents(ovrApp* app) {
	ovrEventDataBuffer eventDataBuffer = {};

	// Poll for VrApi events
	for (;;) {
		ovrEventHeader* eventHeader = (ovrEventHeader*)(&eventDataBuffer);
		ovrResult res = vrapi_PollEvent(eventHeader);
		if (res != ovrSuccess) {
			break;
		}

		switch (eventHeader->EventType) {
		case VRAPI_EVENT_DATA_LOST:
			rsys_print("vrapi_PollEvent: Received VRAPI_EVENT_DATA_LOST");
			break;
		case VRAPI_EVENT_VISIBILITY_GAINED:
			rsys_print("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_GAINED");
			break;
		case VRAPI_EVENT_VISIBILITY_LOST:
			rsys_print("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_LOST\n");
			break;
		case VRAPI_EVENT_FOCUS_GAINED:
			// FOCUS_GAINED is sent when the application is in the foreground and has
			// input focus. This may be due to a system overlay relinquishing focus
			// back to the application.
			app->ShouldPause = false;
			rsys_print("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_GAINED");
			break;
		case VRAPI_EVENT_FOCUS_LOST:
			// FOCUS_LOST is sent when the application is no longer in the foreground and
			// therefore does not have input focus. This may be due to a system overlay taking
			// focus from the application. The application should take appropriate action when
			// this occurs.
			app->ShouldPause = true;
			rsys_print("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_LOST");
			break;
		case VRAPI_EVENT_DISPLAY_REFRESH_RATE_CHANGE:
		{
			ovrEventDisplayRefreshRateChange* refreshRateChangeEvent = (ovrEventDisplayRefreshRateChange*)eventHeader;
			app->RefreshRate = refreshRateChangeEvent->toDisplayRefreshRate;
			break;
		}
		default:
			ALOGV("vrapi_PollEvent: Unknown event");
			break;
		}
	}
}

/*
================================================================================

Native Activity

================================================================================
*/

/**
 * Process the next main command.
 */
static void app_handle_cmd(struct android_app* app, int32_t cmd)
{
	ovrApp* appState = (ovrApp*)app->userData;

	switch (cmd)
	{
		// There is no APP_CMD_CREATE. The ANativeActivity creates the
		// application thread from onCreate(). The application thread
		// then calls android_main().
	case APP_CMD_START:
	{
		ALOGV("onStart()");
		ALOGV("    APP_CMD_START");
		break;
	}
	case APP_CMD_RESUME:
	{
		ALOGV("onResume()");
		ALOGV("    APP_CMD_RESUME");
		appState->Resumed = true;
		break;
	}
	case APP_CMD_PAUSE:
	{
		ALOGV("onPause()");
		ALOGV("    APP_CMD_PAUSE");
		appState->Resumed = false;
		break;
	}
	case APP_CMD_STOP:
	{
		ALOGV("onStop()");
		ALOGV("    APP_CMD_STOP");
		break;
	}
	case APP_CMD_DESTROY:
	{
		ALOGV("onDestroy()");
		ALOGV("    APP_CMD_DESTROY");
		appState->NativeWindow = NULL;
		break;
	}
	case APP_CMD_INIT_WINDOW:
	{
		ALOGV("surfaceCreated()");
		ALOGV("    APP_CMD_INIT_WINDOW");
		appState->NativeWindow = app->window;
		break;
	}
	case APP_CMD_TERM_WINDOW:
	{
		ALOGV("surfaceDestroyed()");
		ALOGV("    APP_CMD_TERM_WINDOW");
		appState->NativeWindow = NULL;
		break;
	}
	}
}

bool LoadVrApi(const char* filename)
{
	return true;
}

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

#define GAME_UPDATE(name) void name(float dt, rsys_input systemInput)
typedef GAME_UPDATE(GameUpdateFunc);
GAME_UPDATE(gameUpdateStub)
{
}

#define GAME_RENDER(name) void name()
typedef GAME_RENDER(GameRenderFunc);
GAME_RENDER(gameRenderStub)
{
}

typedef struct android_game_code
{
	void *gameCodeSO;
	GameInitializeFunc* initialize;
	GameTerminateFunc* terminate;
	GameUpdateFunc* update;
	GameRenderFunc* render;
//	FILETIME DLLTimeStamp;
	bool IsValid;
}android_game_code;

ANativeActivity *g_androidActivity = NULL;

/**
* This is the main entry point of a native application that is using
* android_native_app_glue.  It runs in its own thread, with its own
* event loop for receiving input events and doing other things.
*/
//JNI_OnLoadJavaVM* vm, void* reserved

typedef jint (*func_moody_t)(JavaVM *vm, void* reserved);


/**
 * \brief Gets the internal name for an android permission.
 * \param[in] lJNIEnv a pointer to the JNI environment
 * \param[in] perm_name the name of the permission, e.g.,
 *   "READ_EXTERNAL_STORAGE", "WRITE_EXTERNAL_STORAGE".
 * \return a jstring with the internal name of the permission,
 *   to be used with android Java functions
 *   Context.checkSelfPermission() or Activity.requestPermissions()
 */
jstring android_permission_name(JNIEnv* lJNIEnv, const char* perm_name) {
	// nested class permission in class android.Manifest,
	// hence android 'slash' Manifest 'dollar' permission
	jclass ClassManifestpermission = (*lJNIEnv)->FindClass(lJNIEnv, "android/Manifest$permission");
	jfieldID lid_PERM = (*lJNIEnv)->GetStaticFieldID(lJNIEnv, ClassManifestpermission, perm_name, "Ljava/lang/String;");
	jstring ls_PERM = (jstring)((*lJNIEnv)->GetStaticObjectField(lJNIEnv, ClassManifestpermission, lid_PERM));
	return ls_PERM;
}

/**
 * \brief Tests whether a permission is granted.
 * \param[in] app a pointer to the android app.
 * \param[in] perm_name the name of the permission, e.g.,
 *   "READ_EXTERNAL_STORAGE", "WRITE_EXTERNAL_STORAGE".
 * \retval true if the permission is granted.
 * \retval false otherwise.
 * \note Requires Android API level 23 (Marshmallow, May 2015)
 */
bool android_has_permission(struct android_app* app, const char* perm_name) {
	JavaVM* lJavaVM = app->activity->vm;
	JNIEnv* lJNIEnv = NULL;
	bool lThreadAttached = false;

	// Get JNIEnv from lJavaVM using GetEnv to test whether
	// thread is attached or not to the VM. If not, attach it
	// (and note that it will need to be detached at the end
	//  of the function).
	switch ((*lJavaVM)->GetEnv(lJavaVM, (void**)&lJNIEnv, JNI_VERSION_1_6))
	{
		case JNI_OK:
			break;
		case JNI_EDETACHED: 
		{
			jint lResult = (*lJavaVM)->AttachCurrentThread(lJavaVM, &lJNIEnv, NULL);
			if (lResult == JNI_ERR) 
			{
//			throw std::runtime_error("Could not attach current thread");
			}
			lThreadAttached = true;
			break;
		}
		case JNI_EVERSION:
			//		throw std::runtime_error("Invalid java version");
			break;
	}

	bool result = false;

	jstring ls_PERM = android_permission_name(lJNIEnv, perm_name);

	jint PERMISSION_GRANTED = (jint)-1;
	{
		jclass ClassPackageManager = (*lJNIEnv)->FindClass(lJNIEnv, "android/content/pm/PackageManager");
		jfieldID lid_PERMISSION_GRANTED = (*lJNIEnv)->GetStaticFieldID(lJNIEnv, ClassPackageManager, "PERMISSION_GRANTED", "I");
		PERMISSION_GRANTED = (*lJNIEnv)->GetStaticIntField(lJNIEnv, ClassPackageManager, lid_PERMISSION_GRANTED);
	}
	{
		jobject activity = app->activity->clazz;
		jclass ClassContext = (*lJNIEnv)->FindClass(lJNIEnv, "android/content/Context");
		jmethodID MethodcheckSelfPermission = (*lJNIEnv)->GetMethodID(lJNIEnv, ClassContext, "checkSelfPermission", "(Ljava/lang/String;)I");
		jint int_result = (*lJNIEnv)->CallIntMethod(lJNIEnv, activity, MethodcheckSelfPermission, ls_PERM);
		result = (int_result == PERMISSION_GRANTED);
	}

	if (lThreadAttached) {
		(*lJavaVM)->DetachCurrentThread(lJavaVM);
	}

	return result;
}

/**
 * \brief Query file permissions.
 * \details This opens the system dialog that lets the user
 *  grant (or deny) the permission.
 * \param[in] app a pointer to the android app.
 * \note Requires Android API level 23 (Marshmallow, May 2015)
 */
/*
void android_request_file_permissions(struct android_app* app) 
{
	JavaVM* lJavaVM = app->activity->vm;
	JNIEnv* lJNIEnv = NULL;
	bool lThreadAttached = false;

	// Get JNIEnv from lJavaVM using GetEnv to test whether
	// thread is attached or not to the VM. If not, attach it
	// (and note that it will need to be detached at the end
	//  of the function).
	switch ((*lJavaVM)->GetEnv(lJavaVM, (void**)&lJNIEnv, JNI_VERSION_1_6))
	{
		case JNI_OK:
			break;
		case JNI_EDETACHED: 
		{
			jint lResult = (*lJavaVM)->AttachCurrentThread(lJavaVM, &lJNIEnv, NULL);
			if (lResult == JNI_ERR) {
//				throw std::runtime_error("Could not attach current thread");
			}
			lThreadAttached = true;
			break;
		};
		case JNI_EVERSION:
//			throw std::runtime_error("Invalid java version");
			break;
	}

	jobjectArray perm_array = (*lJNIEnv)->NewObjectArray(lJNIEnv, 2, (*lJNIEnv)->FindClass(lJNIEnv, "java/lang/String"), (*lJNIEnv)->NewStringUTF(lJNIEnv, ""));

	(*lJNIEnv)->SetObjectArrayElement(lJNIEnv, perm_array, 0, android_permission_name(lJNIEnv, "READ_EXTERNAL_STORAGE"));

	(*lJNIEnv)->SetObjectArrayElement(lJNIEnv, perm_array, 1, android_permission_name(lJNIEnv, "WRITE_EXTERNAL_STORAGE"));

	jobject activity = app->activity->clazz;

	jclass ClassActivity = (*lJNIEnv)->FindClass(lJNIEnv, "android/app/Activity");

	jmethodID MethodrequestPermissions = (*lJNIEnv)->GetMethodID(lJNIEnv, ClassActivity, "requestPermissions", "([Ljava/lang/String;I)V");

	// Last arg (0) is just for the callback (that I do not use)
	(*lJNIEnv)->CallVoidMethod(lJNIEnv, activity, MethodrequestPermissions, perm_array, 0);

	if (lThreadAttached) 
	{
		(*lJavaVM)->DetachCurrentThread(lJavaVM);
	}
}
*/

void android_request_microphone_permission(struct android_app* app)
{
	JavaVM* lJavaVM = app->activity->vm;
	JNIEnv* lJNIEnv = NULL;
	bool lThreadAttached = false;

	// Get JNIEnv from lJavaVM using GetEnv to test whether
	// thread is attached or not to the VM. If not, attach it
	// (and note that it will need to be detached at the end
	//  of the function).
	switch ((*lJavaVM)->GetEnv(lJavaVM, (void**)&lJNIEnv, JNI_VERSION_1_6))
	{
	case JNI_OK:
		break;
	case JNI_EDETACHED:
	{
		jint lResult = (*lJavaVM)->AttachCurrentThread(lJavaVM, &lJNIEnv, NULL);
		if (lResult == JNI_ERR) {
			//				throw std::runtime_error("Could not attach current thread");
		}
		lThreadAttached = true;
		break;
	};
	case JNI_EVERSION:
		//			throw std::runtime_error("Invalid java version");
		break;
	}

	jobjectArray perm_array = (*lJNIEnv)->NewObjectArray(lJNIEnv, 1, (*lJNIEnv)->FindClass(lJNIEnv, "java/lang/String"), (*lJNIEnv)->NewStringUTF(lJNIEnv, ""));

	(*lJNIEnv)->SetObjectArrayElement(lJNIEnv, perm_array, 0, android_permission_name(lJNIEnv, "RECORD_AUDIO"));

	jobject activity = app->activity->clazz;

	jclass ClassActivity = (*lJNIEnv)->FindClass(lJNIEnv, "android/app/Activity");

	jmethodID MethodrequestPermissions = (*lJNIEnv)->GetMethodID(lJNIEnv, ClassActivity, "requestPermissions", "([Ljava/lang/String;I)V");

	// Last arg (0) is just for the callback (that I do not use)
	(*lJNIEnv)->CallVoidMethod(lJNIEnv, activity, MethodrequestPermissions, perm_array, 0);

	if (lThreadAttached)
	{
		(*lJavaVM)->DetachCurrentThread(lJavaVM);
	}
}

const char* android_get_external_storage_path(struct android_app* app)
{
	JavaVM* lJavaVM = app->activity->vm;
	JNIEnv* lJNIEnv = NULL;
	bool lThreadAttached = false;

	// Get JNIEnv from lJavaVM using GetEnv to test whether
	// thread is attached or not to the VM. If not, attach it
	// (and note that it will need to be detached at the end
	//  of the function).
	switch ((*lJavaVM)->GetEnv(lJavaVM, (void**)&lJNIEnv, JNI_VERSION_1_6))
	{
	case JNI_OK:
		break;
	case JNI_EDETACHED:
	{
		jint lResult = (*lJavaVM)->AttachCurrentThread(lJavaVM, &lJNIEnv, NULL);
		if (lResult == JNI_ERR) {
			//				throw std::runtime_error("Could not attach current thread");
		}
		lThreadAttached = true;
		break;
	};
	case JNI_EVERSION:
		//			throw std::runtime_error("Invalid java version");
		break;
	}

	jobjectArray perm_array = (*lJNIEnv)->NewObjectArray(lJNIEnv, 1, (*lJNIEnv)->FindClass(lJNIEnv, "java/lang/String"), (*lJNIEnv)->NewStringUTF(lJNIEnv, ""));

	(*lJNIEnv)->SetObjectArrayElement(lJNIEnv, perm_array, 0, android_permission_name(lJNIEnv, "RECORD_AUDIO"));

	jobject activity = app->activity->clazz;
/*
	jclass ClassActivity = (*lJNIEnv)->FindClass(lJNIEnv, "android/app/Activity");

	jmethodID MethodrequestPermissions = (*lJNIEnv)->GetMethodID(lJNIEnv, ClassActivity, "requestPermissions", "([Ljava/lang/String;I)V");

	// Last arg (0) is just for the callback (that I do not use)
	(*lJNIEnv)->CallVoidMethod(lJNIEnv, activity, MethodrequestPermissions, perm_array, 0);
*/
// getExternalFilesDir() - java
	jclass ClassActivity = (*lJNIEnv)->FindClass(lJNIEnv, "android/app/NativeActivity");
	jmethodID MethodgetExternalFilesDir = (*lJNIEnv)->GetMethodID(lJNIEnv, ClassActivity, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
	jobject ObjFile = (*lJNIEnv)->CallObjectMethod(lJNIEnv, activity, MethodgetExternalFilesDir, NULL);
	jclass ClassFile = (*lJNIEnv)->FindClass(lJNIEnv, "java/io/File");
	jmethodID MethodgetPath = (*lJNIEnv)->GetMethodID(lJNIEnv, ClassFile, "getPath", "()Ljava/lang/String;");
	jstring Path = (*lJNIEnv)->CallObjectMethod(lJNIEnv, ObjFile, MethodgetPath);
	const char* path = (*lJNIEnv)->GetStringUTFChars(lJNIEnv, Path, NULL);

//	jclass cls_Env = env->FindClass("android/app/NativeActivity");
//	jmethodID mid = env->GetMethodID(cls_Env, "getExternalFilesDir",
//		"(Ljava/lang/String;)Ljava/io/File;");
//	jobject obj_File = env->CallObjectMethod(_activity->clazz, mid, NULL);
//	jclass cls_File = env->FindClass("java/io/File");
//	jmethodID mid_getPath = env->GetMethodID(cls_File, "getPath",
//		"()Ljava/lang/String;");
//	jstring obj_Path = (jstring)env->CallObjectMethod(obj_File, mid_getPath);
//	return obj_Path;

	if (lThreadAttached)
	{
		(*lJavaVM)->DetachCurrentThread(lJavaVM);
	}
	return path;
}

void check_android_permissions(struct android_app* app)
{
/*
	bool read = android_has_permission(app, "READ_EXTERNAL_STORAGE");
	bool write = android_has_permission(app, "WRITE_EXTERNAL_STORAGE");
	bool OK = read && write;
	if (!OK)
	{
		android_request_file_permissions(app);
	}
*/
	bool mic = android_has_permission(app, "RECORD_AUDIO");
	if (!mic)
	{
		android_request_microphone_permission(app);
	}
}

void rvrs_initializePlatformOvr(const rvrs_platformDescOvr* desc);
void rvrs_initializeAvatarOvr(const char* appId);
bool rvrs_requestAvatarSpecOvr(uint64_t userId, int32_t slot);
bool rvrs_destroyAvatarOvr(uint64_t userId, int32_t slot);

void rvrs_updateAvatarFrameParams(int32_t avatarIdx, float dt, const rvrs_avatarFrameParams* params);
#include "Block.h"
void android_main(struct android_app* app)
{
		int multiplier = 7;
		int (^myBlock)(int) = ^(int num) {
			return num * multiplier;
		};

		printf("%d", myBlock(3));

		int (^newBlock)(int) = _Block_copy(myBlock);
		printf("%d", newBlock(3));
		_Block_release(newBlock);

		//	LoadVrApi("libvrapi.so");
//	void* vrApiLibrary = dlopen("libvrapi.so", RTLD_LAZY);
//	if (vrApiLibrary == NULL)
//	{
//		ALOGE("Failed to load VrApi.  Application exiting!\n");
//		exit(1);
//	}

//	void* vrApiLibrary2 = dlopen("libfmodL.so", RTLD_LAZY | RTLD_GLOBAL);

	ALOGV("----------------------------------------------------------------");
	ALOGV("android_app_entry()");
	ALOGV("    android_main()");

	ANativeActivity_setWindowFlags(app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
	g_androidActivity = app->activity;

	ovrJava java;
	java.Vm = app->activity->vm;
	(*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
	java.ActivityObject = app->activity->clazz;

	// Note that AttachCurrentThread will reset the thread name.
	prctl(PR_SET_NAME, (long)"OVR::Main", 0, 0, 0);

	check_android_permissions(app);			// CLR - Request microphone permission if not already granted.
	const char* storagePath = android_get_external_storage_path(app);
	ovrApp appState;
	ovrApp_Clear(&appState);
	appState.Java = java;
	rsys_initialize(&(rsys_initParams) {
		.storagePath = storagePath,
	});

	rvrs_initialize(&(rvrs_initParams) {
		.ovrInitParams = {
			.Type = VRAPI_STRUCTURE_TYPE_INIT_PARMS,
			.ProductVersion = VRAPI_PRODUCT_VERSION,
			.MajorVersion = VRAPI_MAJOR_VERSION,
			.MinorVersion = VRAPI_MINOR_VERSION,
			.PatchVersion = VRAPI_PATCH_VERSION,
			.GraphicsAPI = VRAPI_GRAPHICS_API_OPENGL_ES_3,
			.Java = java,
		},
		.appState = &appState,
		.ovrLibrary = NULL, // vrApiLibrary,
		.oversamplePercentage = 100,
	});

	ovrEgl_CreateContext(&appState.Egl, NULL);
	EglInitExtensions();

	appState.UseMultiview &= glExtensions.multi_view;

	ALOGV("AppState UseMultiview : %d", appState.UseMultiview);

	appState.CpuLevel = CPU_LEVEL;
	appState.GpuLevel = GPU_LEVEL;
	appState.MainThreadTid = gettid();

	android_game_code game = {};

	char fileBuffer[MAX_PATH];
	const char* filename = NULL;
	rsys_file file = file_open("startup.ini", "rt");
	size_t fileLen = min(file_length(file), MAX_PATH-1);
	memset(fileBuffer, 0, fileLen);
	file_read(fileBuffer, fileLen, file);
	file_close(file);
	const char* cmd = fileBuffer;
	if (cmd[0] == '-')
	{
		if (!strncmp(&cmd[1], "game", 4))
		{
			if (cmd[5] == '=')
			{
				filename = &cmd[6];
			}
		}
	}
	size_t nameLen = strlen(filename);
	char* ext = strstr(filename, ".so");
	if (ext)
	{
		ext[3] = '\0';
	}
	filename = (nameLen > 0 && ext != NULL) ? filename : "libgame.so";

// Game Library is already open but we open it again to obtain entrypoints.
	void* gameLibrary = dlopen(filename, RTLD_LAZY); // , RTLD_LAZY);
	if (gameLibrary)
	{
		GameInitializeFunc *gameInitialize = (GameInitializeFunc*)dlsym(gameLibrary, "game_initialize");
		if (gameInitialize == NULL)
			exit(1);

		game.initialize = (GameInitializeFunc*)dlsym(gameLibrary, "game_initialize");
		game.terminate = (GameTerminateFunc*)dlsym(gameLibrary, "game_terminate");
		game.update = (GameUpdateFunc*)dlsym(gameLibrary, "game_update");
		game.render = (GameRenderFunc*)dlsym(gameLibrary, "game_render");
		game.IsValid = game.initialize && game.terminate && game.terminate && game.render;
		if (game.IsValid == false)
			exit(1);
	}

	int32_t eyeWidth = 0; 
	int32_t eyeHeight = 0;
	rvrs_getEyeWidthHeight(&eyeWidth, &eyeHeight);
	rgfx_initialize(&(rgfx_initParams) {
		.eyeWidth = eyeWidth,
		.eyeHeight = eyeHeight,
		.extensions = {
			.multi_view = glExtensions.multi_view,
			.EXT_texture_border_clamp = glExtensions.EXT_texture_border_clamp,
		}
	});

	rres_initialise();

/*
	rresApi rres = {
		.registerResource = rres_registerResource,
	};
*/
	rengApi reng;

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

	reng.rgfx = (rgfxApi) {
		.findMesh = rgfx_findMesh,
		.findMeshInstance = rgfx_findMeshInstance,
		.addMeshInstance = rgfx_addMeshInstance,
		.updateLight = rgfx_updateLight,
		.updateMeshInstance = rgfx_updateMeshInstance,
		.updateMeshMaterial = rgfx_updateMeshMaterial,
		.removeAllMeshInstances = rgfx_removeAllMeshInstances,
		.updateTransforms = rgfx_updateTransforms,
		.setGameCamera = rgfx_setGameCamera,
		.mapModelMatricesBuffer = rgfx_mapModelMatricesBuffer,
		.unmapModelMatricesBuffer = rgfx_unmapModelMatricesBuffer,
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
		.shouldPause = rvrs_shouldPause,
		.isTrackingControllers = rvrs_isTrackingControllers,
		.getHeadMatrix = rvrs_getHeadMatrix,
		.getHeadPose = rvrs_getHeadPose,
		.getDeviceType = rvrs_getDeviceType,
		.getRefreshRate = rvrs_getRefreshRate,
		.setRefreshRate = rvrs_setRefreshRate,
		.setFoveationLevel = rvrs_setFoveationLevel,
		.getScaledEyeSize = rvrs_getScaledEyeSize,
		.getSupportedRefreshRates = rvrs_getSupportedRefreshRates,
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

	game.initialize(eyeWidth, eyeHeight, &reng);

	app->userData = &appState;
	app->onAppCmd = app_handle_cmd;

	appState.RefreshRate = vrapi_GetSystemPropertyFloat(&java, VRAPI_SYS_PROP_DISPLAY_REFRESH_RATE);

	const double startTime = GetTimeInSeconds();
	uint32_t leftControllerButtonState = 0;
	uint32_t prevLeftControllerButtonState = 0;
	ovrHmdColorDesc hmdColorDesc = { VRAPI_COLORSPACE_RIFT_S };
	uint32_t foveationLevel = 2;
	bool bUseDynamicFoveation = false;
	bool bFoveationLevelChanged = true; //Force set initial foveation level.
	bool bRefreshRateChangeAtStart = false;
	const rsys_input systemInput = { 0 };
	while (app->destroyRequested == 0)
	{
		// Read all pending events.
		for (;;)
		{
			int events;
			struct android_poll_source* source;
			const int timeoutMilliseconds = (appState.Ovr == NULL && app->destroyRequested == 0) ? -1 : 0;
			if (ALooper_pollAll(timeoutMilliseconds, NULL, &events, (void**)&source) < 0)
			{
				break;
			}

			// Process this event.
			if (source != NULL)
			{
				source->process(app, source);
			}

			ovrApp_HandleVrModeChanges(&appState);
		}

		// We must read from the event queue with regular frequency.
		ovrApp_HandleVrApiEvents(&appState);

		ovrApp_HandleInput(&appState);

		if (appState.Ovr == NULL)
		{
			continue;
		}
/*
		static bool firstTime = true;
		if (firstTime)
		{
			ovrHmdColorDesc colorDesc = { VRAPI_COLORSPACE_REC_709 };
			vrapi_SetClientColorDesc(appState.Ovr, &colorDesc);
			firstTime = false;
		}
*/
		vrapi_SetTrackingSpace(appState.Ovr, VRAPI_TRACKING_SPACE_LOCAL_FLOOR);

		// This is the only place the frame index is incremented, right before
		// calling vrapi_GetPredictedDisplayTime().
		appState.FrameIndex++;
/*
		// Get the HMD pose, predicted for the middle of the time period during which
		// the new eye images will be displayed. The number of frames predicted ahead
		// depends on the pipeline depth of the engine and the synthesis rate.
		// The better the prediction, the less black will be pulled in at the edges.
		const double predictedDisplayTime = vrapi->GetPredictedDisplayTime(appState.Ovr, appState.FrameIndex);
		rvrs_trackingInfo tracking = {};
		tracking.hmdTracking = vrapi->GetPredictedTracking2(appState.Ovr, predictedDisplayTime);
		ovrTracking contorllerTracking[2];
		for (int32_t i = 0; i < 2; ++i)
		{
			vrapi->GetInputTrackingState(appState.Ovr, appState.ControllerIDs[i], predictedDisplayTime, &tracking.controllerTracking[i]);
			memcpy(&tracking.controllerState[i], &appState.ControllerState[i], sizeof(ovrInputStateTrackedRemote));
		}
*/
		if (rvrs_isHmdMounted())
		{
			rvrs_beginFrame();
		}

		rvrs_updateTracking(&(rvrs_appInfo) {
			.appState = &appState,
		});

//		appState.DisplayTime = predictedDisplayTime;
/*
		prevLeftControllerButtonState = leftControllerButtonState;
		leftControllerButtonState = tracking.controllerState[0].Buttons;

		if ((leftControllerButtonState & ovrButton_Y) && !(prevLeftControllerButtonState & ovrButton_Y))
		{
			rsys.print("Y Button Pressed.");
			foveationLevel += foveationLevel < 4 ? 1 : 0;
			bFoveationLevelChanged = true;
		}

		if ((leftControllerButtonState & ovrButton_X) && !(prevLeftControllerButtonState & ovrButton_X))
		{
			rsys.print("X Button Pressed.");
			foveationLevel -= foveationLevel > 0 ? 1 : 0;
			bFoveationLevelChanged = true;
		}
*/
/*
		rvrs_controllerState leftControllerButtonState;
		if (rvrs_getControllerState(0, &leftControllerButtonState) >= 0)
		{
			if (leftControllerButtonState.buttonsPressed & ovrButton_Y)
			{
				hmdColorDesc.ColorSpace += hmdColorDesc.ColorSpace <= VRAPI_COLORSPACE_ADOBE_RGB ? 1 : 0;
				vrapi_SetClientColorDesc(appState.Ovr, &hmdColorDesc);
			}

			if (leftControllerButtonState.buttonsPressed & ovrButton_X)
			{
				hmdColorDesc.ColorSpace -= hmdColorDesc.ColorSpace > VRAPI_COLORSPACE_UNMANAGED ? 1 : 0;
				vrapi_SetClientColorDesc(appState.Ovr, &hmdColorDesc);
			}
		}
*/
		if (bFoveationLevelChanged)
		{
			vrapi_SetPropertyInt(&appState.Java, VRAPI_FOVEATION_LEVEL, foveationLevel);
			vrapi_SetPropertyInt(&appState.Java, VRAPI_DYNAMIC_FOVEATION_ENABLED, bUseDynamicFoveation);
			bFoveationLevelChanged = false;
		}

		if (bRefreshRateChangeAtStart)
		{
			ovrResult err = vrapi_SetDisplayRefreshRate(appState.Ovr, 90.0f);
			vrapi_SetExtraLatencyMode(appState.Ovr, VRAPI_EXTRA_LATENCY_MODE_ON);
			appState.RefreshRate = vrapi_GetSystemPropertyFloat(&appState.Java, VRAPI_SYS_PROP_DISPLAY_REFRESH_RATE);
			bRefreshRateChangeAtStart = false;
		}

		const float dt = 1.0f / appState.RefreshRate;


		game.update(dt, systemInput);
		game.render();
	}

	game.terminate();

	rgfx_terminate();

	ovrEgl_DestroyContext(&appState.Egl);

	rvrs_terminate();

	raud_terminate();

	(*java.Vm)->DetachCurrentThread(java.Vm);
}
