
#pragma once
#include "rgfx/renderer.h"

typedef struct header
{
    unsigned char       identifier[12];
    unsigned int        endianness;
    unsigned int        gltype;
    unsigned int        gltypesize;
    unsigned int        glformat;
    unsigned int        glinternalformat;
    unsigned int        glbaseinternalformat;
    unsigned int        pixelwidth;
    unsigned int        pixelheight;
    unsigned int        pixeldepth;
    unsigned int        arrayelements;
    unsigned int        faces;
    unsigned int        miplevels;
    unsigned int        keypairbytes;
}header;

union keyvaluepair
{
    unsigned int        size;
    unsigned char       rawbytes[4];
};

rgfx_texture ktx_load(const char* filename, const rgfx_textureLoadDesc* desc);
bool ktx_save(const char* filename, unsigned int target, unsigned int tex);

