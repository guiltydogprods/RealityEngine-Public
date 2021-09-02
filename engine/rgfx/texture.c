#include "stdafx.h"
#include "texture.h"

#ifdef RE_PLATFORM_WIN64
#include "GL4.6/dds.h"
#else
#include "GLES3.2/ktx.h"
#endif

#ifndef MAX_PATH
#define MAX_PATH (260)
#endif

rgfx_texture texture_load(const char *textureName, const rgfx_textureLoadDesc* desc)
{
	char filename[MAX_PATH];
#ifdef RE_PLATFORM_WIN64
	strcpy(filename, "assets/Textures/");
	strcat(filename, textureName);
	strncpy(filename + (strlen(filename) - 3), "dds", 3);
	return dds_load(filename, desc);
#else
	strcpy(filename, "Textures/");
	strcat(filename, textureName);
	strncpy(filename + (strlen(filename) - 3), "ktx", 3);
	return ktx_load(filename, desc);
#endif

}