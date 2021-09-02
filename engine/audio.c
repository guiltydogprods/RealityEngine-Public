#include "stdafx.h"
#include "stdafx.h"
#include "audio.h"
#include "fmod.h"
#include "stb/stretchy_buffer.h"
#include "math/vec4.h"
#include "hash.h"
#include "resource.h"

//#define DISABLE_DSP

#ifndef MAX_PATH
#define MAX_PATH (260)
#endif

#ifndef RE_PLATFORM_WIN64
typedef struct FMOD_ANDROID_THREADAFFINITY
{
	unsigned int mixer;             /* Software mixer thread. */
	unsigned int stream;            /* Stream thread. */
	unsigned int nonblocking;       /* Asynchronous sound loading thread. */
	unsigned int file;              /* File thread. */
	unsigned int geometry;          /* Geometry processing thread. */
	unsigned int profiler;          /* Profiler threads. */
	unsigned int studioUpdate;      /* Studio update thread. */
	unsigned int studioLoadBank;    /* Studio bank loading thread. */
	unsigned int studioLoadSample;  /* Studio sample loading thread. */
} FMOD_ANDROID_THREADAFFINITY;

//extern "C" FMOD_RESULT F_API FMOD_Android_SetThreadAffinity(FMOD_ANDROID_THREADAFFINITY * affinity);
FMOD_RESULT F_API FMOD_Android_SetThreadAffinity(FMOD_ANDROID_THREADAFFINITY * affinity);
#else
#ifdef VRSYSTEM_OCULUS_WIN
#include "OVR_CAPI_Audio.h"
#endif
#endif
#include "OculusSpatializerFMOD.h"

#define ION_PI (3.14159265359f)

FMOD_SYSTEM *s_fmodSystem = NULL;
FMOD_SOUND **s_fmodSounds = NULL;
//FMOD_DSP* s_fmodDSP = NULL;
uint32_t s_fmodDSPHandle = 0;

rsys_hash s_soundBankLookup = { 0 };
raud_soundBankInfo *s_soundBanks = NULL;

FMOD_CHANNEL** s_activeChannelsArray = NULL;
int32_t s_activeChannelsBufferIndex = 0;

void raud_initialize()
{
	FMOD_RESULT result = FMOD_System_Create(&s_fmodSystem);
	assert(result == FMOD_OK);

#ifndef RE_PLATFORM_WIN64
	FMOD_ANDROID_THREADAFFINITY ta = {};
	result = FMOD_Android_SetThreadAffinity(&ta);
	assert(result == FMOD_OK);
#else
#ifdef VRSYSTEM_OCULUS_WIN
	GUID guid;
	ovr_GetAudioDeviceOutGuid(&guid);

	int driverCount = 0;
	FMOD_System_GetNumDrivers(s_fmodSystem, &driverCount);

	int driver = 0;
	while (driver < driverCount)
	{
		char name[256] = { 0 };
		FMOD_GUID fmodGuid = { 0 };
		FMOD_System_GetDriverInfo(s_fmodSystem, driver, name, 256, &fmodGuid, NULL, NULL, NULL);

		if (guid.Data1 == fmodGuid.Data1 &
			guid.Data2 == fmodGuid.Data2 &
			guid.Data3 == fmodGuid.Data3 &
			memcmp(guid.Data4, fmodGuid.Data4, 8) == 0)
		{
			break;
		}
		++driver;
	}

	if (driver < driverCount)
	{
		FMOD_System_SetDriver(s_fmodSystem, driver);
	}
	else
	{
		// error rift not connected
	}
#endif
#endif
	result = FMOD_System_Init(s_fmodSystem, 64, FMOD_INIT_NORMAL /*| FMOD_INIT_3D_RIGHTHANDED*/, 0);
	assert(result == FMOD_OK);
#ifndef DISABLE_DSP
#ifdef VRSYSTEM_OCULUS_WIN
	result = FMOD_System_LoadPlugin(s_fmodSystem, "OculusSpatializerFMOD.dll", &s_fmodDSPHandle, 0);
	assert(result == FMOD_OK);

	FMOD_PLUGINTYPE type;
	result = FMOD_System_GetPluginInfo(s_fmodSystem, s_fmodDSPHandle, &type, NULL, 0, NULL);
	assert(result == FMOD_OK);

//	result = FMOD_System_CreateDSPByPlugin(s_fmodSystem, handle, &s_fmodDSP);
//	assert(result == FMOD_OK);

//	result = FMOD_Channel_AddDSP()
//	ERRCHECK(channel->addDSP(FMOD_CHANNELCONTROL_DSP_HEAD, hrtfDSP))
//	result = FMOD_DSP_SetParameterBool(s_fmodDSP, OSP_PARAM_INDEX_ATTENUATION_ENABLED, true);
//	assert(result == FMOD_OK);
//	result = FMOD_DSP_SetParameterFloat(s_fmodDSP, OSP_PARAM_INDEX_SOURCE_RANGE_MIN, 0.1f);
//	assert(result == FMOD_OK);
//	result = FMOD_DSP_SetParameterFloat(s_fmodDSP, OSP_PARAM_INDEX_SOURCE_RANGE_MAX, 100.0f);
//	assert(result == FMOD_OK);
//	ERRCHECK(hrtfDSP->setParameterBool(OSP_PARAM_INDEX_ATTENUATION_ENABLED, true));
//  ERRCHECK(hrtfDSP->setParameterFloat(OSP_PARAM_INDEX_SOURCE_RANGE_MIN, 0.1f));
//	ERRCHECK(hrtfDSP->setParameterFloat(OSP_PARAM_INDEX_SOURCE_RANGE_MAX, 100.0f));
#else
	result = FMOD_System_LoadPlugin(s_fmodSystem, "libOculusSpatializerFMOD.so", &s_fmodDSPHandle, 0);
	assert(result == FMOD_OK);

	FMOD_PLUGINTYPE type;
	result = FMOD_System_GetPluginInfo(s_fmodSystem, s_fmodDSPHandle, &type, NULL, 0, NULL);
	assert(result == FMOD_OK);
#endif
#endif
	result = FMOD_System_Set3DSettings(s_fmodSystem, 1.0f, 1.0f, 1.0f);
	assert(result == FMOD_OK);

	result = FMOD_System_SetStreamBufferSize(s_fmodSystem, 65536, FMOD_TIMEUNIT_RAWBYTES);
	assert(result == FMOD_OK);
}

void raud_terminate()
{
	int32_t numSounds = sb_count(s_fmodSounds);
	for (int32_t i = 0; i < numSounds; ++i)
	{
		FMOD_Sound_Release(s_fmodSounds[i]);
		s_fmodSounds[i] = NULL;
	}
	FMOD_System_Close(s_fmodSystem);
	FMOD_System_Release(s_fmodSystem);
	s_fmodSystem = 0;
}

raud_sound raud_loadSound(const char *soundName)
{
	char filename[MAX_PATH];
#ifdef RE_PLATFORM_WIN64
	strcpy(filename, "assets/sounds/");
	strcat(filename, soundName);
#else
	strcpy(filename, "sounds/");
	strcat(filename, soundName);
#endif

	rsys_file file = file_open(filename, "rb");
	if (!file_is_open(file))
	{
		printf("Error: Unable to open file %s\n", filename);
		exit(1);
	}

	size_t fileLen = file_length(file);
	char *buffer = malloc(fileLen);
	memset(buffer, 0, fileLen);
	file_read(buffer, fileLen, file);
	file_close(file);

	FMOD_SOUND *pSound = NULL;

	FMOD_RESULT result = FMOD_System_CreateSound(s_fmodSystem, buffer, FMOD_3D_WORLDRELATIVE | FMOD_3D | FMOD_LOOP_OFF | FMOD_CREATECOMPRESSEDSAMPLE | FMOD_OPENMEMORY, &(FMOD_CREATESOUNDEXINFO) {
		.cbsize = sizeof(FMOD_CREATESOUNDEXINFO),
		.length = fileLen,
	}, &pSound);
	(void)result;
	assert(result == FMOD_OK);
	free(buffer);

	result = FMOD_Sound_Set3DMinMaxDistance(pSound, 0.5f, 50.0f);
	assert(result == FMOD_OK);

	sb_push(s_fmodSounds, pSound);

	raud_sound retVal = {};

	retVal.id = sb_count(s_fmodSounds)-1;

	return retVal;
}

void raud_update(void)
{
	FMOD_RESULT res = FMOD_System_Update(s_fmodSystem);
	assert(res == FMOD_OK);

#ifndef DISABLE_DSP
	int32_t numChannels = sb_count(s_activeChannelsArray);
	FMOD_CHANNEL** stillActive = (FMOD_CHANNEL * *)alloca(sizeof(FMOD_CHANNEL*) * numChannels);
	int32_t stillActiveCount = 0;
	if (numChannels > 0)
	{
		for (int32_t chan = 0; chan < numChannels; ++chan)
		{
			FMOD_CHANNEL* channel = s_activeChannelsArray[chan];
			FMOD_BOOL isPlaying = 0;
			FMOD_Channel_IsPlaying(channel, &isPlaying);
			if (isPlaying)
			{
				FMOD_DSP* dsp = NULL;
				res = FMOD_Channel_GetDSP(channel, 0, &dsp);
				assert(res == FMOD_OK);
				if (dsp != NULL) {
					res = FMOD_Channel_RemoveDSP(channel, dsp);
					assert(res == FMOD_OK);
				}
				stillActive[stillActiveCount++] = channel;
			}
		}
//		sb_free(s_activeChannelsArray);					// Free up old array
		sb_reset(s_activeChannelsArray);
		if (stillActiveCount > 0)
		{
			for (int32_t i = 0; i < stillActiveCount; ++i)
			{
				sb_push(s_activeChannelsArray, stillActive[i]);
			}
		}
	}
#endif
}

raud_soundBank raud_registerSoundBank(const char *bankName, const char **bankSoundNames, int32_t numSounds)
{
	uint32_t bankNameHash = hashString(bankName);
	ptrdiff_t index = rsys_hm_find(&s_soundBankLookup, bankNameHash);
	if (index < 0)
	{
		raud_soundBankInfo newBank;
		newBank.numSounds = numSounds;
		newBank.sounds = malloc(sizeof(raud_sound) * numSounds);
		sb_push(s_soundBanks, newBank);
		index = sb_count(s_soundBanks) - 1;
		rsys_hm_insert(&s_soundBankLookup, bankNameHash, index);
	}

	raud_soundBankInfo *bank = &s_soundBanks[index];
	for (int32_t i = 0; i < numSounds; ++i)
	{
		const char *soundName = bankSoundNames[i];
		int32_t hashName = hashString(soundName);
		bank->sounds[i] = rres_findSound(hashName);
	}

	return (raud_soundBank) { .id = index };
}

raud_soundBank raud_findSoundBank(uint32_t hash)
{
	raud_soundBank soundBank = { .id = -1 };
	int32_t index = rsys_hm_find(&s_soundBankLookup, hash);
	if (index >= 0)
	{
		soundBank.id = &s_soundBanks[index] - s_soundBanks;
	}
	return soundBank;
}

void raud_playSound(raud_sound sound, float *position, float *velocity, float volume, int priority)
{
	FMOD_CHANNEL *pChannel = NULL;
	assert(sound.id >= 0);
	FMOD_SOUND *pSound = s_fmodSounds[sound.id];
	{
		FMOD_RESULT res = FMOD_System_PlaySound(s_fmodSystem, pSound, NULL, true, &pChannel);
		assert(res == FMOD_OK);
		if (pChannel)
		{
			FMOD_MODE currMode;
			res = FMOD_Sound_GetMode(pSound, &currMode);
			assert(res == FMOD_OK);
			if (currMode & FMOD_3D)
			{
				FMOD_VECTOR fmodPosition;
				fmodPosition.x = position[0];
				fmodPosition.y = position[1];
				fmodPosition.z = position[2];
#ifndef DISABLE_DSP
				FMOD_DSP* dsp = NULL;
				res = FMOD_System_CreateDSPByPlugin(s_fmodSystem, s_fmodDSPHandle, &dsp);
				assert(res == FMOD_OK);
				if (dsp != NULL)
				{
					res = FMOD_Channel_AddDSP(pChannel, FMOD_CHANNELCONTROL_DSP_HEAD, dsp);
					assert(res == FMOD_OK);
					res = FMOD_DSP_SetParameterBool(dsp, OSP_PARAM_INDEX_ATTENUATION_ENABLED, true);
					assert(res == FMOD_OK);
					res = FMOD_DSP_SetParameterFloat(dsp, OSP_PARAM_INDEX_SOURCE_RANGE_MIN, 0.1f);
					assert(res == FMOD_OK);
					res = FMOD_DSP_SetParameterFloat(dsp, OSP_PARAM_INDEX_SOURCE_RANGE_MAX, 100.0f);
					assert(res == FMOD_OK);

					FMOD_VECTOR fmodVelocity = { 0.0f, 0.0f, 0.0f };
					if (velocity != NULL)
					{
						fmodVelocity.x = velocity[0];
						fmodVelocity.y = velocity[1];
						fmodVelocity.z = velocity[2];
					}
					FMOD_VECTOR fmodDirection = { 0.0f, 0.0f, 1.0f };
					FMOD_DSP_PARAMETER_3DATTRIBUTES attributes = { 
						.absolute.position = fmodPosition,
						.absolute.velocity = fmodVelocity,
						.absolute.forward = fmodDirection,
						.absolute.up = {0.0f, 1.0f, 0.0f},
					};
					res = FMOD_DSP_SetParameterData(dsp, OSP_PARAM_INDEX_SOUND_POSITION, &attributes, sizeof(attributes));
					assert(res == FMOD_OK);
				}
				else
				{
					(void)velocity;
					res = FMOD_Channel_Set3DAttributes(pChannel, &fmodPosition, NULL);
					assert(res == FMOD_OK);
				}
#else
				(void)velocity;
				res = FMOD_Channel_Set3DAttributes(pChannel, &fmodPosition, NULL);
				assert(res == FMOD_OK);
#endif

			}
			res = FMOD_Channel_SetVolume(pChannel, volume);
			assert(res == FMOD_OK);
			res = FMOD_Channel_SetPriority(pChannel, priority);
			assert(res == FMOD_OK);
			
			res = FMOD_Channel_SetPaused(pChannel, false);
			assert(res == FMOD_OK);
#ifndef DISABLE_DSP
			sb_push(s_activeChannelsArray, pChannel);
#endif
		}
	}
}

void raud_playSoundBank(raud_soundBank soundBank, float* position, float* velocity, float volume, int priority)
{
	raud_soundBankInfo *bank = &s_soundBanks[soundBank.id];
	assert(bank != NULL);
	int32_t index = (int32_t)roundf((float)(bank->numSounds - 1) * (float)rand() / (float)RAND_MAX);
	raud_sound sound = bank->sounds[index];
	raud_playSound(sound, position, velocity, volume, priority);
}

void raud_updateListenerPosition(float* position, float* velocity, float* forward, float* up)
{
	FMOD_VECTOR fmodPosition;
	fmodPosition.x = position[0];
	fmodPosition.y = position[1];
	fmodPosition.z = position[2];

	FMOD_VECTOR fmodForward;
	fmodForward.x = forward[0];
	fmodForward.y = forward[1];
	fmodForward.z = forward[2];

	FMOD_VECTOR fmodUp;
	fmodUp.x = up[0];
	fmodUp.y = up[1];
	fmodUp.z = up[2];

	FMOD_RESULT res = FMOD_System_Set3DListenerAttributes(s_fmodSystem, 0, &fmodPosition, NULL, &fmodForward, &fmodUp);
	assert(res == FMOD_OK);

	res = FMOD_System_Update(s_fmodSystem);
	assert(res == FMOD_OK);
}

void raud_updateSoundVolume(const void* soundChannel, float volume)
{
	FMOD_CHANNEL* channel = soundChannel;
	FMOD_RESULT res = FMOD_Channel_SetVolume(channel, volume);
	assert(res == FMOD_OK);
}

void raud_updateSoundPosition(const void* soundChannel, float* position, float* velocity, float* direction)
{
	FMOD_CHANNEL* channel = soundChannel;
#ifndef DISABLE_DSP
	FMOD_DSP* dsp = NULL;
	FMOD_RESULT res = FMOD_Channel_GetDSP(channel, 0, &dsp);
	assert(res == FMOD_OK);
	if (dsp != NULL)
	{
		FMOD_VECTOR fmodPosition;
		fmodPosition.x = position[0];
		fmodPosition.y = position[1];
		fmodPosition.z = position[2];

		FMOD_VECTOR fmodDirection = { 0.0f, 0.0f, 1.0f };
		if (direction != NULL)
		{
			fmodDirection.x = direction[0];
			fmodDirection.y = direction[1];
			fmodDirection.z = direction[2];
		}

		FMOD_DSP_PARAMETER_3DATTRIBUTES attributes = { 0 };

		attributes.absolute.position = fmodPosition;
		attributes.absolute.velocity.x = 0.0f;
		attributes.absolute.velocity.y = 0.0f;
		attributes.absolute.velocity.z = 0.0f;
		attributes.absolute.forward = fmodDirection;
		attributes.absolute.up.x = 0.0f;
		attributes.absolute.up.y = 0.0f;
		attributes.absolute.up.z = 0.0f;
		res = FMOD_DSP_SetParameterData(dsp, 0, &attributes, sizeof(attributes));
		assert(res == FMOD_OK);
	}
	else
	{
		FMOD_VECTOR fmodPosition;
		fmodPosition.x = position[0];
		fmodPosition.y = position[1];
		fmodPosition.z = position[2];
		(void)velocity;
		res = FMOD_Channel_Set3DAttributes(channel, &fmodPosition, NULL);
		assert(res == FMOD_OK);
		if (direction != NULL)
		{
			FMOD_VECTOR fmodDirection;
			fmodDirection.x = direction[0];
			fmodDirection.y = direction[1];
			fmodDirection.z = direction[2];
			res = FMOD_Channel_Set3DConeOrientation(channel, &fmodDirection);
			assert(res == FMOD_OK);
		}
	}
#else
	FMOD_VECTOR fmodPosition;
	fmodPosition.x = position[0];
	fmodPosition.y = position[1];
	fmodPosition.z = position[2];
	(void)velocity;
	FMOD_RESULT res = FMOD_Channel_Set3DAttributes(channel, &fmodPosition, NULL);
	assert(res == FMOD_OK);
	if (direction != NULL)
	{
		FMOD_VECTOR fmodDirection;
		fmodDirection.x = direction[0];
		fmodDirection.y = direction[1];
		fmodDirection.z = direction[2];
		res = FMOD_Channel_Set3DConeOrientation(channel, &fmodDirection);
		assert(res == FMOD_OK);
	}
#endif
}

void raud_getSoundUserData(void* sound, void** userData)
{
	FMOD_RESULT res = FMOD_Sound_GetUserData(sound, userData);
	assert(res == FMOD_OK);
}

// Sine wave
static float Sine(float samplePos)
{
	return sinf(samplePos * ION_PI * 2.0f);
}

static float Sawtooth(float samplePos)
{
	if (samplePos == 0)
		return 0.0f;

	return (2.0f / samplePos) - 1.0f;
}

// Square wave
static float Square(float samplePos)
{
	return (samplePos < 0.5f ? 1.f : -1.f);
}

// Generate new samples
// We must fill "length" bytes in the buffer provided by "data"
FMOD_RESULT F_CALLBACK PCMRead(FMOD_SOUND* sound, void* data, unsigned int length)
{
	// Sample rate
	const int32_t sampleRate = 48000;

	// Frequency to generate (Hz)
	const int32_t frequency = 800;

	// Volume level (0.0 - 1.0)
	const float volume = 0.3f;

	// How many samples we have generated so far
	static int32_t samplesElapsed = 0;

	// Get buffer in 16-bit format
	int16_t* buffer = (int16_t*)data;

	// A 2-channel 16-bit stereo stream uses 4 bytes per sample, therefore a 1-channel 16-bit stereo stream uses 2 bytes per sample
	for (unsigned int sample = 0; sample < length / 2; sample++)
	{
		// Get the position in the sample
		float pos = frequency * (float)samplesElapsed / sampleRate;

		// The generator function returns a value from -1 to 1 so we multiply this by the
		// maximum possible volume of a 16-bit PCM sample (32767) to get the true volume to store

		// Generate a sample for the left channel
		*buffer++ = (int16_t)(Sine(pos) * 32767.0f * volume);

		// Increment number of samples generated
		samplesElapsed++;
	}

	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK PCMSetPosition(FMOD_SOUND* sound, int subsound, unsigned int position, FMOD_TIMEUNIT postype)
{
	// If you need to process the user changing the playback position (seeking), do it here

	return FMOD_OK;
}

void* raud_createStream(const raud_streamDesc* desc)
{
	assert(desc != NULL);
	FMOD_CREATESOUNDEXINFO soundInfo = { 0 };

	// The sample rate, number of channels and total time in seconds before the gnerated sample repeats
	int32_t sampleRate = 48000;
	int32_t channels = 1;
	int32_t lengthInSeconds = 1;

	// Set up the sound
	soundInfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);

	// The number of samples to fill per call to the PCM read callback (here we go for 1 second's worth)
	soundInfo.decodebuffersize = desc->decodeBufferSize; // 9600;

	// The length of the entire sample in bytes, calculated as:
	// Sample rate * number of channels * bits per sample per channel * number of seconds
	soundInfo.length = desc->length; // 9600; // sampleRate* channels * sizeof(signed short)* lengthInSeconds;

	// Number of channels and sample rate
	soundInfo.numchannels = desc->numChannels; // channels;
	soundInfo.defaultfrequency = desc->sampleRate;

	// The sound format (here we use 16-bit signed PCM)
	soundInfo.format = FMOD_SOUND_FORMAT_PCM16;

	// Callback for generating new samples
	soundInfo.pcmreadcallback = desc->pcmreadcallback != NULL ? desc->pcmreadcallback : PCMRead;

	// Callback for when the user seeks in the playback
	soundInfo.pcmsetposcallback = PCMSetPosition;

	soundInfo.userdata = desc->userData;

	FMOD_SOUND* sound = NULL;
	FMOD_RESULT res = FMOD_System_CreateStream(s_fmodSystem, NULL, FMOD_3D_WORLDRELATIVE | FMOD_3D | FMOD_OPENUSER, &soundInfo, &sound);
	assert(res == FMOD_OK);

	FMOD_CHANNEL* channel = NULL;
	res = FMOD_System_PlaySound(s_fmodSystem, sound, NULL, true, &channel);
	assert(res == FMOD_OK);
#ifndef DISABLE_DSP
	FMOD_DSP* dsp = NULL;
	res = FMOD_System_CreateDSPByPlugin(s_fmodSystem, s_fmodDSPHandle, &dsp);
	assert(res == FMOD_OK);
	res = FMOD_Channel_AddDSP(channel, 0, dsp);
	assert(res == FMOD_OK);
	res = FMOD_DSP_SetParameterBool(dsp, OSP_PARAM_INDEX_ATTENUATION_ENABLED, true);
	assert(res == FMOD_OK);
	res = FMOD_DSP_SetParameterFloat(dsp, OSP_PARAM_INDEX_SOURCE_RANGE_MIN, 0.1f);
	assert(res == FMOD_OK);
	res = FMOD_DSP_SetParameterFloat(dsp, OSP_PARAM_INDEX_SOURCE_RANGE_MAX, 100.0f);
	assert(res == FMOD_OK);
#endif
	res = FMOD_Channel_SetVolume(channel, desc->volume);
	assert(res == FMOD_OK);
	res = FMOD_Channel_SetLoopCount(channel, -1);
	assert(res == FMOD_OK);
	res = FMOD_Channel_SetMode(channel, FMOD_LOOP_NORMAL);
	assert(res == FMOD_OK);
	res = FMOD_Channel_SetPosition(channel, 0, FMOD_TIMEUNIT_MS);	// this flushes the buffer to ensure the loop mode takes effect
	assert(res == FMOD_OK);
	res = FMOD_Channel_SetPaused(channel, false);
	assert(res == FMOD_OK);

	return channel;
}

void raud_stopStream(const void* stream)
{
	if (stream == NULL)
		return;

	FMOD_CHANNEL* channel = stream;
#ifndef DISABLE_DSP
	FMOD_DSP* dsp = NULL;
	FMOD_RESULT res = FMOD_Channel_GetDSP(channel, 0, &dsp);
	assert(res == FMOD_OK);
	if (dsp != NULL) {
		res = FMOD_Channel_RemoveDSP(channel, dsp);
		assert(res == FMOD_OK);
	}
#endif
	FMOD_Channel_Stop(channel);
}
