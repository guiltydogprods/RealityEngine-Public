#include "stdafx.h"
#include "dds.h"
#include <assert.h>

#ifndef MAX_PATH
#define MAX_PATH (260)
#endif

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;

typedef struct DDS_PIXELFORMAT
{
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwFourCC;
	DWORD dwRGBBitCount;
	DWORD dwRBitMask;
	DWORD dwGBitMask;
	DWORD dwBBitMask;
	DWORD dwABitMask;
}DDS_PIXELFORMAT;

typedef struct DDS_HEADER
{
	DWORD           dwSize;
	DWORD           dwFlags;
	DWORD           dwHeight;
	DWORD           dwWidth;
	DWORD           dwPitchOrLinearSize;
	DWORD           dwDepth;
	DWORD           dwMipMapCount;
	DWORD           dwReserved1[11];
	DDS_PIXELFORMAT ddspf;
	DWORD           dwCaps;
	DWORD           dwCaps2;
	DWORD           dwCaps3;
	DWORD           dwCaps4;
	DWORD           dwReserved2;
}DDS_HEADER;

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
                ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#endif //defined(MAKEFOURCC)

#define FOURCC_DXT1	MAKEFOURCC('D', 'X', 'T', '1')
#define FOURCC_DXT3	MAKEFOURCC('D', 'X', 'T', '3')
#define FOURCC_DXT5	MAKEFOURCC('D', 'X', 'T', '5')
#define FOURCC_DX10	MAKEFOURCC('D', 'X', '1', '0')

rgfx_texture dds_load(const char *textureName, const rgfx_textureLoadDesc* desc)
{
	rgfx_texture tex = { 0 };
	int32_t slice = -1;
	if (desc != 0)
	{
		tex = desc->texture;
		slice = desc->slice;
	}
	char filename[MAX_PATH];
	strcpy(filename, textureName);

	rsys_file file = file_open(filename, "rb");
	if (!file_is_open(file))
	{
		printf("Error: Unable to open file %s\n", filename);
		exit(1);
	}

	size_t fileLen = file_length(file);
	uint8_t *buffer = malloc(fileLen);
	memset(buffer, 0, fileLen);
	file_read(buffer, fileLen, file);
	file_close(file);
	//Why isn't this working?
	bool bSrgb = (strstr(filename, "albedo") != NULL) || (strstr(filename, "emissive") != NULL);

	DDS_HEADER *header = (DDS_HEADER *)(buffer + 4);

//	uint32_t headerSize = header->dwSize;
//	uint32_t flags = header->dwFlags;
	uint32_t height = header->dwHeight;
	uint32_t width = header->dwWidth;
//	uint32_t linearSize = header->dwPitchOrLinearSize;
//	uint32_t texDepth = header->dwDepth;
	uint32_t mipMapCount = header->dwMipMapCount;
	uint32_t fourCC = header->ddspf.dwFourCC;

	uint8_t *imageData = buffer + sizeof(DDS_HEADER) + 4;
//	unsigned int components = (fourCC == FOURCC_DXT1) ? 3 : 4;
	rgfx_format format = kFormatRGBADXT1;
/*
	switch (fourCC)
	{
	case FOURCC_DXT1:
		format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		break;
	case FOURCC_DXT3:
		format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		break;
	case FOURCC_DXT5:
		format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		break;
	default:
		assert(0);
	}
*/
	if (tex.id == 0)
	{
		tex = rgfx_createTexture(&(rgfx_textureDesc) {
			.type = kTexture2D,
			.minFilter = kFilterLinearMipmapLinear,
			.magFilter = kFilterLinear,
			.anisoFactor = 1.0f,
		});
	}
/*
	else if (slice = 0)
	{
		switch (desc->type)
		{
		case kTexture2D:
			glTexStorage2D(target, mipCount, internalFormat, width, height);
			break;
		case kTexture2DArray:
			glTexStorage3D(target, mipCount, internalFormat, width, height, depth);
			break;
		default:
			rsys_print("Currently unsupported texture type.\n");
			assert(0);
		}

		.width = tex->sizeX,
		.height = tex->sizeY,
		.depth = 5, // CLR - Textures for each of the 5 parts are stored in a texture array.
		.mipCount = tex->mipCount,
	}
*/
	switch (fourCC)
	{
	case FOURCC_DXT1:
		format = bSrgb ? kFormatSRGBDXT1 : kFormatRGBADXT1;
		break;
	case FOURCC_DXT3:
		format = bSrgb ? kFormatSRGBDXT3 : kFormatRGBADXT3;
		break;
	case FOURCC_DXT5:
		format = bSrgb ? kFormatSRGBDXT5 : kFormatRGBADXT5;
		break;
	default:
		assert(0);
	}

	unsigned int blockSize = (format == kFormatRGBADXT1 || format == kFormatSRGBDXT1) ? 8 : 16;
//	unsigned int blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT || format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;
	unsigned int offset = 0;

	/* load the mipmaps */
	for (unsigned int level = 0; level < mipMapCount && (width || height); ++level)
	{
		unsigned int size = ((width + 3) / 4) * ((height + 3) / 4) * blockSize;
		if (slice < 0)
		{
			//		glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, size, buffer + offset);
			rgfx_writeCompressedMipImageData(tex, level, format, width, height, size, imageData + offset);
			offset += size;
			width /= 2;
			height /= 2;
		}
		else
		{
			rgfx_writeCompressedTexSubImage3D(tex, level, 0, 0, slice, width, height, 1, format, size, imageData + offset);
			offset += size;
			width /= 2;
			height /= 2;
			width = max(width, 1u);
			height = max(height, 1u);
		}
	}
	free(buffer);
	return tex;
}
