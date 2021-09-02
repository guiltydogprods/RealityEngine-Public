#include "stdafx.h"
#include "font.h"
#include "resource.h"
#include "stb/stretchy_buffer.h"

static int32_t kFontVersionMajor = 0;
static int32_t kFontVersionMinor = 1;
static const uint32_t kFourCC_FONT = ION_MAKEFOURCC('F', 'O', 'N', 'T');	// Font Chunk FourCC
static const uint32_t kFourCC_FDAT = ION_MAKEFOURCC('F', 'D', 'A', 'T');	// Font Data Chunk FourCC

#ifndef MAX_PATH
#define MAX_PATH (260)
#endif

static rgui_fontInfo* s_fontInfoArray;

rgui_font rgui_loadFont(const char* fontName)
{
	char filename[MAX_PATH];

#ifdef RE_PLATFORM_WIN64
	strcpy(filename, "assets/Fonts/");
#else
	strcpy(filename, "Fonts/");
#endif
	strcat(filename, fontName);
	strncpy(filename + (strlen(filename) - 3), "fnt", 3);

	rsys_file file = file_open(filename, "rb");
	if (!file_is_open(file))
	{
		printf("Error: Unable to open file %s\n", filename);
		exit(1);
	}

	size_t fileLen = file_length(file);
	uint8_t* buffer = malloc(fileLen);
	memset(buffer, 0, fileLen);
	file_read(buffer, fileLen, file);
	file_close(file);

	rgui_fontHeader* header = (rgui_fontHeader*)buffer;
	assert(header->chunkId.fourCC == kFourCC_FONT);
	int16_t len = header->textureNameLength;
	int16_t firstChar = header->firstChar;
	int16_t numChars = header->numChars;
	float fontSize = (float)header->fontSize;
	float ascent = (float)header->ascent;
	float descent = (float)header->descent;
	float lineGap = (float)header->lineGap;
	uint8_t* ptr = (uint8_t*)buffer + sizeof(rgui_fontHeader);

	char strBuffer[1024];	
	strncpy(strBuffer, (char*)ptr, len);
	strBuffer[len] = '\0';
//	const char* filename = strBuffer;
	rres_registerResource((rres_resourceDesc) {
		.filename = strBuffer,
		.type = kRTTexture
	});
	uint32_t alignedLen = (len + (4 - 1)) & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes
	ptr += alignedLen;

	rgui_fontInfo info = { 0 };
	memcpy(sb_add(info.fontData, numChars), ptr, sizeof(rgui_fontChar)* numChars);
	info.fontTexture = rres_findTexture(hashString(strBuffer));
	info.firstChar = firstChar;
	info.numChars = numChars;
	info.fontSize = fontSize;
	info.ascent = ascent;
	info.descent = descent;
	info.lineGap = lineGap;
	sb_push(s_fontInfoArray, info);

	rgui_font retVal = { .id = 0 };

	retVal.id = sb_count(s_fontInfoArray);

	return retVal;
}

rgfx_texture rgui_getFontTexture(rgui_font font)
{
	int32_t fontIdx = font.id - 1;
	assert(fontIdx >= 0);
	assert(fontIdx < sb_count(s_fontInfoArray));

	return s_fontInfoArray[fontIdx].fontTexture;
}

float rgui_getFontSize(rgui_font font)
{
	int32_t fontIdx = font.id - 1;
	assert(fontIdx >= 0);
	assert(fontIdx < sb_count(s_fontInfoArray));

	return s_fontInfoArray[fontIdx].fontSize;
}

bool rgui_getFontVMetrics(rgui_font font, float* ascent, float* descent, float*  lineGap)
{
	int32_t fontIdx = font.id - 1;
	assert(fontIdx >= 0);
	assert(fontIdx < sb_count(s_fontInfoArray));

	if (ascent) {
		*ascent = s_fontInfoArray[fontIdx].ascent;
	}
	if (descent) {
		*descent = s_fontInfoArray[fontIdx].descent;
	}
	if (lineGap) {
		*lineGap = s_fontInfoArray[fontIdx].lineGap;
	}

	return true;
}

bool rgui_getFontDataForChar(rgui_font font, int32_t ch, rgui_fontChar* charData)
{
	int32_t fontIdx = font.id - 1;
	assert(fontIdx >= 0);
	assert(fontIdx < sb_count(s_fontInfoArray));
	ch -= s_fontInfoArray[fontIdx].firstChar;
//	assert(ch >= 0);
	assert(ch < s_fontInfoArray[fontIdx].numChars);
	if (ch < 0)
		ch = (int32_t)'X';

	if (charData) {
		*charData = s_fontInfoArray[fontIdx].fontData[ch];
		return true;
	}
	return false;
}
