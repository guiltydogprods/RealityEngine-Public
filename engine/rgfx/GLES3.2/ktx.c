
// Original code based on sb7ktx.c/h from the Book OpenGL Shader Bible 7 by Graham Sellers.  Original copyright below.
// Reality Engine modifications Copyright 2019-2020 Claire Rogers.
/*
 * Copyright � 2012-2015 Graham Sellers
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "stdafx.h"
#include "ktx.h"
#include "rgfx/renderer.h"

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS 1
#endif /* _MSC_VER */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX_PATH
#define MAX_PATH (260)
#endif

static const unsigned char identifier[] =
{
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};
#if 0
static unsigned int swap32(const unsigned int u32)
{
    union
    {
        unsigned int u32;
        unsigned char u8[4];
    } a, b;

    a.u32 = u32;
    b.u8[0] = a.u8[3];
    b.u8[1] = a.u8[2];
    b.u8[2] = a.u8[1];
    b.u8[3] = a.u8[0];

    return b.u32;
}
#endif
/*
static unsigned short swap16(const unsigned short u16)
{
    union
    {
        unsigned short u16;
        unsigned char u8[2];
    } a, b;

    a.u16 = u16;
    b.u8[0] = a.u8[1];
    b.u8[1] = a.u8[0];

    return b.u16;
}
*/
#if 0
static unsigned int calculate_stride(const header *h, unsigned int width, unsigned int pad)
{
    unsigned int channels = 0;

    switch (h->glbaseinternalformat)
    {
        case GL_RED:    channels = 1;
            break;
        case GL_RG:     channels = 2;
            break;
 //       case GL_BGR:
        case GL_RGB:    channels = 3;
            break;
  //      case GL_BGRA_EXT:
        case GL_RGBA:   channels = 4;
            break;
    }

    unsigned int stride = h->gltypesize * channels * width;

    stride = (stride + (pad - 1)) & ~(pad - 1);

    return stride;
}
#endif
#if 0
static unsigned int calculate_face_size(const header *h)
{
    unsigned int stride = calculate_stride(h, h->pixelwidth, 4);

    return stride * h->pixelheight;
}
#endif 

rgfx_texture ktx_load(const char* textureName, const rgfx_textureLoadDesc* desc)
{
	rgfx_texture tex = { 0 };
	int32_t slice = -1;
	if (desc != 0)
	{
		tex = desc->texture;
		slice = desc->slice;
	}

	GLenum target = GL_NONE;

	char filename[MAX_PATH];
#ifdef RE_PLATFORM_WIN64
	strcpy(filename, "assets/");
	strcat(filename, textureName);
#else
	strcpy(filename, textureName);
#endif

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

	bool bSrgb = (strstr(filename, "albedo") != NULL) || (strstr(filename, "emissive") != NULL);

	header *h = (header *)buffer;

	if (memcmp(h->identifier, identifier, sizeof(identifier)) != 0)
		goto fail_header;

	if (h->endianness == 0x04030201)
	{
		// No swap needed
	}
	else if (h->endianness == 0x01020304)
	{
	// Swap needed
/*
		h.endianness            = swap32(h.endianness);
		h.gltype                = swap32(h.gltype);
		h.gltypesize            = swap32(h.gltypesize);
		h.glformat              = swap32(h.glformat);
		h.glinternalformat      = swap32(h.glinternalformat);
		h.glbaseinternalformat  = swap32(h.glbaseinternalformat);
		h.pixelwidth            = swap32(h.pixelwidth);
		h.pixelheight           = swap32(h.pixelheight);
        h.pixeldepth            = swap32(h.pixeldepth);
        h.arrayelements         = swap32(h.arrayelements);
        h.faces                 = swap32(h.faces);
        h.miplevels             = swap32(h.miplevels);
        h.keypairbytes          = swap32(h.keypairbytes);
*/
	}
    else
    {
        goto fail_header;
    }

    // Guess target (texture type)
    if (h->pixelheight == 0)
    {
		//CLR - Assert Here?
    }
    else if (h->pixeldepth == 0)
    {
        if (h->arrayelements == 0)
        {
            if (h->faces == 1)
            {
                target = GL_TEXTURE_2D;
            }
            else
            {
                target = GL_TEXTURE_CUBE_MAP;
            }
        }
        else
        {
            if (h->faces == 0)
            {
                target = GL_TEXTURE_2D_ARRAY;
            }
            else
            {
                target = GL_TEXTURE_CUBE_MAP_ARRAY;
            }
        }
    }
    else
    {
        target = GL_TEXTURE_3D;
    }

    // Check for insanity...
    if (target == GL_NONE || (h->pixelwidth == 0) || (h->pixelheight == 0 && h->pixeldepth != 0))
    {
        goto fail_header;
    }

	if (tex.id == 0)
	{
		tex = rgfx_createTexture(&(rgfx_textureDesc) {
			.type = kTexture2D,
			.minFilter = kFilterLinearMipmapLinear,
			.magFilter = kFilterLinear,
			.anisoFactor = 1.0f,
		});
	}

//    data_start = buffer + sizeof(header) + h->keypairbytes;
//    data_end = buffer + fileLen; //(fp);
	uint8_t* data = buffer + sizeof(header) + h->keypairbytes;

	if (h->miplevels == 0)
	{
		h->miplevels = 1;
	}

//    h->glinternalformat = bSrgb ? GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4 : GL_COMPRESSED_RGBA_ASTC_4x4; // (strstr(filename, "albedo") != NULL) || (strstr(filename, "emissive") != NULL);
    rgfx_format format = bSrgb ? kFormatSRGBASTC4x4 : kFormatRGBAASTC4x4;
    uint32_t width = h->pixelwidth;
	uint32_t height = h->pixelheight;
	switch (target)
	{
        case GL_TEXTURE_2D:
			for (uint32_t mip = 0; mip < h->miplevels; ++mip)
			{
				GLsizei imageSize = *(int32_t*)data;
				data += sizeof(int32_t);
                if (slice < 0)
                {
                    rgfx_writeCompressedMipImageData(tex, mip, format, width, height, imageSize, data);
                    data += imageSize;
                    width >>= 1;
                    height >>= 1;
                }
                else
                {
					rgfx_writeCompressedTexSubImage3D(tex, mip, 0, 0, slice, width, height, 1, format, imageSize, data);
					data += imageSize;
					width /= 2;
					height /= 2;
//					width = max(width, 1u);
//					height = max(height, 1u);
                }
			}
            break;
        default:                                               // Should never happen
            goto fail_target;
    }

    if (h->miplevels == 1)
    {
        glGenerateMipmap(target);
    }

fail_target:
    free(buffer);

fail_header:

    return tex;
}

bool ktx_save(const char *filename, unsigned int target, unsigned int tex)
{
	(void)filename;
    header h;

    memset(&h, 0, sizeof(h));
    memcpy(h.identifier, identifier, sizeof(identifier));
    h.endianness = 0x04030201;

    glBindTexture(target, tex);

    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, (GLint *)&h.pixelwidth);
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, (GLint *)&h.pixelheight);
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_DEPTH, (GLint *)&h.pixeldepth);

    return true;
}
