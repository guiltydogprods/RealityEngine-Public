//
// pch.h
// Header for standard system include files.
//
// Used by the build system to generate the precompiled header. Note that no
// pch.cpp is needed and the pch.h is automatically included in all cpp files
// that are part of the project
//
#ifdef RE_PLATFORM_ANDROID
#include <jni.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>


#include <EGL/egl.h>
#include <GLES3/gl32.h>

#include <android/log.h>
#include "platform/android/android_native_app_glue.h"

#endif
