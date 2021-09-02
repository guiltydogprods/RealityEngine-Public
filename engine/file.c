#include "stdafx.h"
#include "file.h"

#ifndef RE_PLATFORM_ANDROID
rsys_file file_open(const char *filename, const char *mode)
{
	rsys_file ret = {
		.fptr = fopen(filename, mode)
	};
	return ret;
}

void file_close(rsys_file file)
{
	fclose(file.fptr);
}

bool file_is_open(rsys_file file)
{
	return file.fptr != NULL;
}

size_t file_length(rsys_file file)
{
	fseek(file.fptr, 0, SEEK_END);
	size_t length = ftell(file.fptr);
	fseek(file.fptr, 0, SEEK_SET);
	return length;
}

size_t file_read(void *buffer, size_t bytes, rsys_file file)
{
	return fread(buffer, 1, bytes, file.fptr);
}

#else

extern ANativeActivity *g_androidActivity;

rsys_file file_open(const char *filename, const char *mode)
{
	rsys_file ret = 
	{
		.asset = AAssetManager_open(g_androidActivity->assetManager, filename, AASSET_MODE_STREAMING)
	};
	return ret;
}

void file_close(rsys_file file)
{
//	fclose(fptr);
	AAsset_close(file.asset);
}

bool file_is_open(rsys_file file)
{
	return file.asset != NULL;
}

size_t file_length(rsys_file file)
{
//	fseek(fptr, 0, SEEK_END);
//	size_t length = ftell(fptr);
//	fseek(fptr, 0, SEEK_SET);
//	return length;
	return AAsset_getLength(file.asset);
}

size_t file_read(void *buffer, size_t bytes, rsys_file file)
{
//	return fread(buffer, 1, bytes, fptr);

	return AAsset_read(file.asset, buffer, bytes);
}

#if 0
AAsset* asset = AAssetManager_open(g_androidActivity->assetManager, filename, AASSET_MODE_STREAMING);
assert(asset != NULL);
off_t count = AAsset_getLength(asset);
if (count > 0)
{
	//					content = new char[count+1];
	//					content = static_cast<char*>(ion::MemMgr::AllocMemory(count + 1));
	ScopeStack localScope(MemMgr::ThreadLocalAllocator());
	content = static_cast<char*>(localScope.alloc(count + 1));
	//				count = fread(content, sizeof(char), count, fp);
	//				content[count] = '\0';
	AAsset_read(asset, content, count);
	AAsset_close(asset);
}
#endif

#endif