#pragma once
#include "math/vec4.h"

typedef struct raud_sound { int32_t id; } raud_sound;
typedef struct raud_soundBank { int32_t id; } raud_soundBank;

//typedef FMOD_RESULT(F_CALL* FMOD_SOUND_PCMREAD_CALLBACK)   (FMOD_SOUND* sound, void* data, unsigned int datalen);
//typedef int32_t(* FMOD_SOUND_PCMREAD_CALLBACK)   (void* sound, void* data, unsigned int datalen);

typedef struct raud_soundBankInfo
{
	int32_t numSounds;
	raud_sound *sounds;
}raud_soundBankInfo;

typedef struct raud_streamDesc
{
	int32_t sampleRate;
	int32_t numChannels;
	uint32_t decodeBufferSize;
	uint32_t length;
	float volume;
	void* userData;
	int32_t (*pcmreadcallback)(void* sound, void* data, unsigned int datalen);
}raud_streamDesc;

void raud_initialize();
void raud_terminate();
void raud_update(void);

raud_sound raud_loadSound(const char *filename);
raud_soundBank raud_registerSoundBank(const char *bankName, const char **bankSoundNames, int32_t numSounds);
raud_soundBank raud_findSoundBank(uint32_t hash);

void raud_playSound(raud_sound sound, float *position, float *velocity, float volume, int priority);
void raud_playSoundBank(raud_soundBank soundBank, float* position, float* velocity, float volume, int priority);

void raud_updateListenerPosition(float* position, float* velocity, float* forward, float* up);
void raud_updateSoundVolume(const void* soundChannel, float volume);
void raud_updateSoundPosition(const void* soundChannel, float* position, float* velocity, float* direction);
void raud_getSoundUserData(void* sound, void** userData);

void* raud_createStream(const raud_streamDesc* desc);
void raud_stopStream(const void* stream);

typedef struct raudApi
{
	void (*update)(void);
	raud_soundBank(*registerSoundBank)(const char* bankName, const char** bankSoundNames, int32_t numSounds);
	raud_soundBank(*findSoundBank)(uint32_t hash);
	void (*playSound)(raud_sound sound, float* position, float* velocity, float volume, int priority);
	void (*playSoundBank)(raud_soundBank sound, float* position, float* velocity, float volume, int priority);
	void (*updateListenerPosition)(float* position, float* velocity, float* forward, float* up);
	void (*updateSoundVolume)(const void* fmodChannel, float volume);
	void (*updateSoundPosition)(const void* fmodChannel, float* position, float* velocity, float* direction);
	void (*getSoundUserData)(void* sound, void** userData);
	void* (*createStream)(const raud_streamDesc* desc);
	void (*stopStream)(const void* stream);
}raudApi;

extern raudApi raud;
