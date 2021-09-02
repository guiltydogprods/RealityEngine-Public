#pragma once
#include "rgfx/renderer.h"
#include "audio.h"
#include "font.h"

struct Mesh;

typedef enum rres_type
{
	kRTInvalid,
	kRTMesh,
	kRTTexture,
	kRTShader,
	kRTSound,
	kRTFont,
}rres_type;

typedef struct rres_resourceDesc
{
	const char *filename;
	rres_type	type; 
}rres_resourceDesc;

/*
typedef struct rres_resource
{
	uint32_t filenameHash;
	rres_type type;
}rres_resource;
*/
void rres_initialise();
bool rres_registerResource(const rres_resourceDesc res);
int32_t rres_registerMesh(const char *meshName);
struct Mesh *rres_getMesh(int32_t meshId);
struct Mesh *rres_findMesh(uint32_t filenameHash);

int32_t rres_registerTexture(const char *filename);
rgfx_texture rres_findTexture(uint32_t filename);

int32_t rres_registerSound(const char* filename);
raud_sound rres_findSound(uint32_t filenameHash);

int32_t rres_registerFont(const char* filename);
rgui_font rres_findFont(uint32_t filenameHash);

unsigned int rres_loadKTX(const char *filename, unsigned int tex);

typedef struct rresApi
{
	bool (*registerResource)(const rres_resourceDesc resource);
	rgfx_texture(*findTexture)(uint32_t nameHash);
	rgui_font(*findFont)(uint32_t nameHash);
	raud_sound(*findSound)(uint32_t nameHash);
	//	unsigned int (*loadKTX)(const char *filename, unsigned int tex);
	//int(*registerMesh)(const char* meshName);
	//	struct Mesh *(*getMesh)(int32_t meshId);
}rresApi;

extern rresApi rres;

