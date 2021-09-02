#include "stdafx.h"
#include "resource.h"
#include "rgfx/mesh.h"
#include "rgfx/texture.h"
#include "rgfx/renderer.h"
#include "audio.h"
#include "font.h"
#include "hash.h"
#include "stb/stretchy_buffer.h"
//#define STB_DS_IMPLEMENTATION
//#include "stb/stb_ds.h"

rsys_hash s_meshLookup = {};
Mesh* s_meshes = NULL;

rsys_hash s_textureLookup = {};
rgfx_texture *s_textures = NULL;

rsys_hash s_soundLookup = {};
raud_sound *s_sounds = NULL;

rsys_hash s_fontLookup = {};
rgui_font* s_fonts = NULL;

typedef struct array_of_strings 
{
	char *block;
	uint32_t block_used;
	int32_t *stringIndexes;
}array_of_strings;

typedef struct rres_resource
{
	int32_t filenameIndex;
	rres_type type;
}rres_resource;

typedef struct rres_resourceTable
{
	char* block;
	uint32_t block_used;
	rres_resource *resources;
}rres_resourceTable;

//char* s_resourceFilenames;
array_of_strings s_resourceFilenames = {};

//rres_resource* s_resources;

rsys_hash s_resourceLookup = {};
rres_resourceTable s_resourceTable = {};

int32_t filenameIndex;

void rstr_array_push(struct array_of_strings *a, const char *s)
{
	int32_t n = strlen(s) + 1;
	sb_add(a->block, n);
	sb_push(a->stringIndexes, (a->block + a->block_used) - a->block);
	memcpy(a->block + a->block_used, s, n);
	a->block_used += n;
}

void rres_initialise()
{
	memset(&s_resourceLookup, 0, sizeof(rsys_hash));
	memset(&s_meshLookup, 0, sizeof(rsys_hash));
	memset(&s_textureLookup, 0, sizeof(rsys_hash));
	memset(&s_soundLookup, 0, sizeof(rsys_hash));
}

void rres_registerFilename(const char *filename)
{
	rstr_array_push(&s_resourceFilenames, filename);
}

void rres_addResource(const rres_resourceDesc resource)
{
	int32_t filenameLen = strlen(resource.filename) + 1;
	sb_add(s_resourceTable.block, filenameLen);
	rres_resource res = {
		.filenameIndex = (s_resourceTable.block + s_resourceTable.block_used) - s_resourceTable.block,
		.type = resource.type
	};
	sb_push(s_resourceTable.resources, res);
	memcpy(s_resourceTable.block + s_resourceTable.block_used, resource.filename, filenameLen);
	s_resourceTable.block_used += filenameLen;

	uint32_t filenameHash = hashString(resource.filename);
	int32_t index = sb_count(s_resourceTable.resources) - 1;
	rsys_hm_insert(&s_resourceLookup, filenameHash, index);
}

bool rres_registerResource(const rres_resourceDesc resource)
{
	int32_t retVal = -1;

	if (resource.filename)
	{
		uint32_t hashedFilename = hashString(resource.filename);
		if ((retVal = rsys_hm_find(&s_resourceLookup, hashedFilename)) < 0)
		{
			rres_addResource(resource);

			switch (resource.type)
			{
			case kRTMesh:
				retVal = rres_registerMesh(resource.filename);
				break;
			case kRTTexture:
				retVal = rres_registerTexture(resource.filename);
				break;
			case kRTShader:
				break;
			case kRTSound:
				retVal = rres_registerSound(resource.filename);
				break;
			case kRTFont:
				retVal = rres_registerFont(resource.filename);
				break;
			default:
				break;
			}
		}
	}
	return retVal >= 0;
}

int32_t rres_registerMesh(const char* meshName)
{
	uint32_t filenameHash = hashString(meshName);
	ptrdiff_t index = rsys_hm_find(&s_meshLookup, filenameHash);
	if (index < 0)
	{
		memset(sb_add(s_meshes, 1), 0, sizeof(Mesh));
		index = sb_count(s_meshes) - 1;
		rsys_hm_insert(&s_meshLookup, filenameHash, index);
	}
	return index;
}

Mesh *rres_findMesh(uint32_t filenameHash)
{
	Mesh *mesh = NULL;
	int32_t index = rsys_hm_find(&s_meshLookup, filenameHash);
	if (index >= 0)
	{
		mesh = &s_meshes[index];
		if (!mesh->renderables)
		{
			int32_t resIndex = rsys_hm_find(&s_resourceLookup, filenameHash);
			const char *meshName = s_resourceTable.block + s_resourceTable.resources[resIndex].filenameIndex;
			mesh_load(meshName, mesh);
		}
	}
	return mesh;
}

int32_t rres_registerTexture(const char *filename)
{
	rgfx_texture ret = { 0 };

	uint32_t filenameHash = hashString(filename);
	ptrdiff_t index = rsys_hm_find(&s_textureLookup, filenameHash);
	if (index < 0)
	{
		sb_push(s_textures, (rgfx_texture) { 0 });
		index = sb_count(s_textures) - 1;
		rsys_hm_insert(&s_textureLookup, filenameHash, index);
	}
	ret.id = index;

	return index;
}

rgfx_texture rres_findTexture(uint32_t filenameHash)
{
	rgfx_texture tex = { 0 };
	int32_t index = rsys_hm_find(&s_textureLookup, filenameHash);
	if (index >= 0)
	{
		tex = s_textures[index];
		if (tex.id == 0)
		{
			int32_t resIndex = rsys_hm_find(&s_resourceLookup, filenameHash);
			const char* texName = s_resourceTable.block + s_resourceTable.resources[resIndex].filenameIndex;
			tex = texture_load(texName, NULL);
			s_textures[index] = tex;
		}
	}
	return tex;
}

int32_t rres_registerSound(const char* filename)
{
	raud_sound ret = { .id = -1 };

	uint32_t filenameHash = hashString(filename);
	ptrdiff_t index = rsys_hm_find(&s_soundLookup, filenameHash);
	if (index < 0)
	{
		raud_sound sound = raud_loadSound(filename);
		sb_push(s_sounds, sound);
		index = sb_count(s_sounds) - 1;
		rsys_hm_insert(&s_soundLookup, filenameHash, index);
	}
	ret.id = index;

	return index;
}

raud_sound rres_findSound(uint32_t filenameHash)
{
	raud_sound sound = { .id = -1 };
	int32_t index = rsys_hm_find(&s_soundLookup, filenameHash);
	if (index >= 0)
	{
		sound = s_sounds[index];
	}
	return sound;
}

int32_t rres_registerFont(const char* filename)
{
	rgui_font ret = { 0 };

	uint32_t filenameHash = hashString(filename);
	ptrdiff_t index = rsys_hm_find(&s_textureLookup, filenameHash);
	if (index < 0)
	{
		rgui_font font = rgui_loadFont(filename);
		sb_push(s_fonts, font);
		index = sb_count(s_fonts) - 1;
		rsys_hm_insert(&s_fontLookup, filenameHash, index);
	}
	ret.id = index;

	return index;
}

rgui_font rres_findFont(uint32_t filenameHash)
{
	rgui_font font = { 0 };
	int32_t index = rsys_hm_find(&s_fontLookup, filenameHash);
	if (index >= 0)
	{
		font = s_fonts[index];
	}
	return font;
}
