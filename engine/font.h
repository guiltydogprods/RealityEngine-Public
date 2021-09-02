#pragma once

#include "rgfx/renderer.h"
#include "rgfx/mesh.h"

typedef struct rgui_font { int32_t id; } rgui_font;

typedef struct rgui_rect
{
	float x0, y0, x1, y1;
}rgui_rect;

typedef struct rgui_fontChar
{
	rgui_rect rect;
	float xoff;
	float yoff;
	float w;
	float h;
	float advance;
}rgui_fontChar;

typedef struct rgui_fontHeader
{
	ChunkId		chunkId;
	uint32_t	versionMajor;
	uint32_t	versionMinor;
	int16_t		textureNameLength;
	int16_t		firstChar;
	int16_t		numChars;
	int16_t		fontSize;
	float		ascent;
	float		descent;
	float		lineGap;
}rgui_fontHeader;

typedef struct rgui_fontDataChunk
{
	ChunkId		chunkId;
	uint32_t	numFonts;
}rgui_fontDataChunk;

typedef struct rgui_fontInfo
{
	rgui_fontChar*	fontData;
	rgfx_texture	fontTexture;
	int16_t			firstChar;
	int16_t			numChars;
	float			fontSize;
	float			ascent;
	float			descent;
	float			lineGap;
}rgui_fontInfo;


rgui_font rgui_loadFont(const char* filename);
rgfx_texture rgui_getFontTexture(rgui_font font);
float rgui_getFontSize(rgui_font font);
bool rgui_getFontDataForChar(rgui_font font, int32_t ch, rgui_fontChar* charData);
