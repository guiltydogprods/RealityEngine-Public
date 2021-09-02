#pragma once

#include <stdio.h>

#ifdef RE_PLATFORM_ANDROID
typedef struct AAsset AAsset;
#endif

typedef struct rsys_file
{
#ifndef RE_PLATFORM_ANDROID
	FILE *fptr;
#else
	AAsset *asset;
#endif
}rsys_file;

rsys_file file_open(const char *filename, const char *mode);
void file_close(rsys_file fptr);
bool file_is_open(rsys_file);
size_t file_length(rsys_file fptr);
size_t file_read(void *buffer, size_t bytes, rsys_file fptr);
