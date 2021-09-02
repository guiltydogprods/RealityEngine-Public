#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#define STB_DEFINE
#include "stb/stb.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "stb/stretchy_buffer.h"

#include <stdint.h>
#include <stdbool.h>

#include "engine/font.h"

// used both to compute SDF and in 'shader'
float sdf_size = 64.0;          // the larger this is, the better large font sizes look
float pixel_dist_scale = 64.0;  // trades off precision w/ ability to handle *smaller* sizes
int onedge_value = 128;
int padding = 3; // not used in shader

typedef struct
{
   float advance;
   signed char xoff;
   signed char yoff;
   unsigned char w,h;
   unsigned char *data;
} fontchar;

fontchar fdata[128];

#define BITMAP_W  1200
#define BITMAP_H  800
unsigned char bitmap[BITMAP_H][BITMAP_W][3] ;
#define BITMAP_SDF_W  512   //1200
#define BITMAP_SDF_H  512   //800
unsigned char bitmapSDF[BITMAP_SDF_H][BITMAP_SDF_W];

#if 1
char* sample = "Hello World!"; // size% d!"; // This is goofy text, size% d!";
char* small_sample = "Hello World!"; // size% d!"; //"This is goofy text, size% d!Really needs in - shader supersampling to look good.";

void blend_pixel(int x, int y, int color, float alpha)
{
   int i;
   for (i=0; i < 3; ++i)
      bitmap[y][x][i] = (unsigned char) (stb_lerp(alpha, bitmap[y][x][i], color)+0.5); // round
}

void draw_char(float px, float py, char c, float relative_scale)
{
   int x,y;
   fontchar *fc = &fdata[c];
   float fx0 = px + fc->xoff*relative_scale;
   float fy0 = py + fc->yoff*relative_scale;
   float fx1 = fx0 + fc->w*relative_scale;
   float fy1 = fy0 + fc->h*relative_scale;
   int ix0 = (int) floor(fx0);
   int iy0 = (int) floor(fy0);
   int ix1 = (int) ceil(fx1);
   int iy1 = (int) ceil(fy1);
   // clamp to viewport
   if (ix0 < 0) ix0 = 0;
   if (iy0 < 0) iy0 = 0;
   if (ix1 > BITMAP_W) ix1 = BITMAP_W;
   if (iy1 > BITMAP_H) iy1 = BITMAP_H;

   for (y=iy0; y < iy1; ++y) {
      for (x=ix0; x < ix1; ++x) {
         float sdf_dist, pix_dist;
//		 float myunlerpx = stb_unlerp(x, fx0, fx1);
//		 float mybmx = stb_lerp(myunlerpx, 0, fc->w);
//		 float myunlerpy = stb_unlerp(y, fy0, fy1);
//		 float mybmy = stb_lerp(myunlerpy, 0, fc->h);
		 float bmx = stb_linear_remap(x, fx0, fx1, 0, fc->w);
         float bmy = stb_linear_remap(y, fy0, fy1, 0, fc->h);
         int v00,v01,v10,v11;
         float v0,v1,v;
         int sx0 = (int) bmx;
         int sx1 = sx0+1;
         int sy0 = (int) bmy;
         int sy1 = sy0+1;
         // compute lerp weights
         bmx = bmx - sx0;
         bmy = bmy - sy0;
         // clamp to edge
         sx0 = stb_clamp(sx0, 0, fc->w-1);
         sx1 = stb_clamp(sx1, 0, fc->w-1);
         sy0 = stb_clamp(sy0, 0, fc->h-1);
         sy1 = stb_clamp(sy1, 0, fc->h-1);
         // bilinear texture sample
         v00 = fc->data[sy0*fc->w+sx0];
         v01 = fc->data[sy0*fc->w+sx1];
         v10 = fc->data[sy1*fc->w+sx0];
         v11 = fc->data[sy1*fc->w+sx1];
         v0 = stb_lerp(bmx,v00,v01);
         v1 = stb_lerp(bmx,v10,v11);
         v  = stb_lerp(bmy,v0 ,v1 );
         #if 0
         // non-anti-aliased
         if (v > onedge_value)
            blend_pixel(x,y,0,1.0);
         #else
         // Following math can be greatly simplified

         // convert distance in SDF value to distance in SDF bitmap
         sdf_dist = stb_linear_remap(v, onedge_value, onedge_value+pixel_dist_scale, 0, 1);
         // convert distance in SDF bitmap to distance in output bitmap
         pix_dist = sdf_dist * relative_scale;
         // anti-alias by mapping 1/2 pixel around contour from 0..1 alpha
         v = stb_linear_remap(pix_dist, -0.5f, 0.5f, 0, 1);
         if (v > 1) v = 1;
         if (v > 0)
            blend_pixel(x,y,0,v);
         #endif
      }
   }
}


void print_text(float x, float y, char *text, float scale)
{
   int i;
   for (i=0; text[i]; ++i) {
      if (fdata[text[i]].data)
         draw_char(x,y,text[i],scale);
      x += fdata[text[i]].advance * scale;
   }
}
#endif

bool blit_packChar(stbtt_pack_context* ctx, const fontchar* fc, rgui_fontChar* charData)
{
	static int32_t destX = 0;
	static int32_t destY = 0;
	static int32_t maxHeightForRow = 0;

	bool retVal = false;

	if (fc->data == NULL)
	{	
		if (charData)
			charData->advance = fc->advance;
		return retVal;
	}
/*
	float fx0 = px + fc->xoff * relative_scale;
	float fy0 = py + fc->yoff * relative_scale;
	float fx1 = fx0 + fc->w * relative_scale;
	float fy1 = fy0 + fc->h * relative_scale;
	int ix0 = (int)floor(fx0);
	int iy0 = (int)floor(fy0);
	int ix1 = (int)ceil(fx1);
	int iy1 = (int)ceil(fy1);
	// clamp to viewport
	if (ix0 < 0) ix0 = 0;
	if (iy0 < 0) iy0 = 0;
	if (ix1 > BITMAP_W) ix1 = BITMAP_W;
	if (iy1 > BITMAP_H) iy1 = BITMAP_H;
*/
	maxHeightForRow = fc->h > maxHeightForRow ? fc->h : maxHeightForRow;

	if (destX + fc->w > ctx->width)
	{
		destY += maxHeightForRow;
		destX = 0;
		maxHeightForRow = 0;
	}
//	*outRect = { //blit_rect ret = {
	if (charData)
	{
		charData->rect.x0 = (float)destX / (float)ctx->width;
		charData->rect.y0 = (float)destY / (float)ctx->height;
		charData->rect.x1 = ((float)destX + (float)fc->w) / (float)ctx->width;
		charData->rect.y1 = ((float)destY + (float)fc->h) / (float)ctx->height;
		charData->xoff = (float)fc->xoff;
		charData->yoff = (float)fc->yoff;
		charData->w = (float)fc->w;
		charData->h = (float)fc->h;
		charData->advance = fc->advance;
		retVal = true;
	};

	for (int32_t y = 0; y < fc->h; ++y)
	{
		uint8_t* destPtr = ctx->pixels + ((destY + y) * ctx->stride_in_bytes) + destX;
		for (int32_t x = 0; x < fc->w; ++x)
		{
			uint8_t value = fc->data[(y * fc->w) + x];
			destPtr[x] = value;
		}
	}
	destX += fc->w;

	return retVal;
}


int main(int argc, char **argv)
{
//	const char fontName[] = "times.ttf";
//	const char fontName[] = "opensansb.ttf";
	const char fontName[] = "calibri.ttf";
	//	float sdf_size = 64.0;          // the larger this is, the better large font sizes look
//	float pixel_dist_scale = 64.0;  // trades off precision w/ ability to handle *smaller* sizes
//	int onedge_value = 128;
//	int padding = 3; // not used in shader

	const int32_t kNumSearchPaths = 2;
	const char* kFontSearchPaths[kNumSearchPaths] = {
		{ "c:/windows/fonts/"},
		{ "sourceAssets/Fonts/" }
	};
	int ch;
	float scale, ypos;
	stbtt_fontinfo font;
	void* data = NULL;
	for (int32_t path = 0; data == NULL && path < kNumSearchPaths; ++path)
	{
		char filename[1024] = { 0 };
		strcpy(filename, kFontSearchPaths[path]); // S"c:/windows/fonts/")
		strcat(filename, fontName);
		data = stb_file(filename, NULL);
	}

	if (data == NULL)
	{
		fprintf(stderr, "Can't find font: %s\n", fontName);
		exit(1);
	}
	//data = stb_file("sourceAssets/Fonts/opensans.ttf", NULL);
	////	void* data = stb_file("c:/windows/fonts/calibri.ttf", NULL);
	stbtt_InitFont(&font, data, 0);

	scale = stbtt_ScaleForPixelHeight(&font, sdf_size);

	stbtt_pack_context ctx;
	stbtt_PackBegin(&ctx, (uint8_t*)bitmapSDF, BITMAP_SDF_W, BITMAP_SDF_H, 0, 1, NULL);

//	rgui_fontChar fontData[128] = { 0 };
	rgui_fontChar* fontDataArray = NULL;
	const int kFirstChar = 32;
	const int kLastChar = 127;
	for (int ch=kFirstChar; ch < kLastChar; ++ch)
	{
        int xoff,yoff,w,h, advance;
		uint8_t* data = stbtt_GetCodepointSDF(&font, scale, ch, padding, onedge_value, pixel_dist_scale, &w, &h, &xoff, &yoff);
        stbtt_GetCodepointHMetrics(&font, ch, &advance, NULL);
		rgui_fontChar charData = { 0 };
		blit_packChar(&ctx, &(fontchar) {
			.data = data,
			.xoff = xoff,
			.yoff = yoff,
			.w = w,
			.h = h,
			.advance = advance * scale,
		}, &charData);
		sb_push(fontDataArray, charData);
		fontchar fc;
		fc.data = data;
		fc.xoff = xoff;
		fc.yoff = yoff;
		fc.w = w;
		fc.h = h;
		fc.advance = advance * scale;
		fdata[ch] = fc;
	}
	stbtt_PackEnd(&ctx);
	int ascent = 0;
	int descent = 0;
	int lineGap = 0;
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
//	const char kTextureName[256] = "times_32_sdf.png";
//	const char kTextureName[256] = "opensans_32_sdf.png";
	char textureSuffix[256] = { 0 };
	sprintf_s(textureSuffix, sizeof(textureSuffix), "_%d_sdf.png", (int32_t)sdf_size);
	char outputTextureName[256] = { 0 }; // = "opensans_32_sdf.png";
	strcpy(outputTextureName, fontName);
	strncpy(outputTextureName + (strlen(outputTextureName) - 4), textureSuffix, strlen(textureSuffix)+1);

	char fullTextureName[MAX_PATH] = { 0 };
	strcpy(fullTextureName, "sourceAssets/Textures/");
	strcat(fullTextureName, outputTextureName);
//	sprintf_s(fullTextureName, sizeof(fullTextureName), "sourceAssets/Textures/%s", kTextureName);
	stbi_write_png(fullTextureName, BITMAP_SDF_W, BITMAP_SDF_H, 1, bitmapSDF, 0);

	size_t textureNameLength = strlen(outputTextureName);
	assert(textureNameLength < MAX_PATH);

//	char fontSuffix[256] = { 0 };
//	sprintf_s(fontSuffix, sizeof(fontSuffix), "_%d_sdf.fnt", (int32_t)sdf_size);

	char fontFileName[MAX_PATH] = { 0 };
	strcpy_s(fontFileName, sizeof(fontFileName), "assets/Fonts/");
	strcat_s(fontFileName, sizeof(fontFileName), fontName);
	strncpy_s(fontFileName + (strlen(fontFileName) - 3), sizeof(fontFileName), "fnt", 3);
//	strncpy(fontFileName + (strlen(fontFileName) - 4), fontSuffix, strlen(fontSuffix)+1);

	FILE* fptr = 0;
//	fptr = fopen("assets/Fonts/opensans.fnt", "wb");
	fptr = fopen(fontFileName, "wb");
	if (fptr)
	{
		fwrite(&(rgui_fontHeader) {
			.chunkId = kFourCC_FONT,
			.versionMajor = kFontVersionMajor,
			.versionMinor = kFontVersionMinor,
			.textureNameLength = (int16_t)textureNameLength,
			.firstChar = kFirstChar,
			.numChars = sb_count(fontDataArray),
			.fontSize = (int16_t)sdf_size,
			.ascent = ascent * scale,
			.descent = descent * scale,
			.lineGap = lineGap * scale,
		}, sizeof(rgui_fontHeader), 1, fptr);
		uint32_t alignedLen = (textureNameLength + (4 - 1)) & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes
		fwrite(outputTextureName, sizeof(char), alignedLen, fptr);
		fwrite(fontDataArray, sizeof(rgui_fontChar), sb_count(fontDataArray), fptr);
		fclose(fptr);
	}
	sb_free(fontDataArray);
	fontDataArray = NULL;

#if 0
   memset(bitmap, 0, sizeof(bitmapSDF));

   stbtt_PackBegin(&ctx, bitmapSDF, BITMAP_SDF_W, BITMAP_SDF_H, 0, 1, NULL);

   stbtt_packedchar chars[128] = { 0 };
   stbtt_PackFontRange(&ctx, data, 0, sdf_size, 32, 128 - 32, chars);
   stbtt_PackEnd(&ctx);

   stbi_write_png("sdf_test3.png", BITMAP_SDF_W, BITMAP_SDF_H, 1, bitmapSDF, 0);

   memset(bitmapSDF, 0, sizeof(bitmapSDF));
#endif
//----------------------------------------------------
#if 0
   stbtt_pack_context ctx2;
   stbtt_PackBegin(&ctx2, bitmap, BITMAP_W, BITMAP_H, 0, 1, NULL);

   stbtt_packedchar chars[128] = { 0 };
   stbtt_PackFontRangeSDF(&ctx, data, 0, sdf_size, 32, 128 - 32, chars);
   stbtt_PackEnd(&ctx);

   stbi_write_png("sdf_test2.png", BITMAP_W, BITMAP_H, 1, bitmap, 0);
#endif
#if 0
	ypos = 60;
	memset(bitmap, 255, sizeof(bitmap));
	print_text(400, ypos+30, stb_sprintf("sdf bitmap height %d", (int) sdf_size), 30/sdf_size);
	ypos += 80;
	for (scale = 8.0; scale < 120.0; scale *= 1.33f)
	{
		print_text(80, ypos + scale, stb_sprintf(scale == 8.0 ? small_sample : sample, (int)scale), sdf_size / sdf_size); // scale / sdf_size);
		ypos += sdf_size * 1.05f + 20; //scale*1.05f + 20;
	}

	stbi_write_png("sdf_test.png", BITMAP_W, BITMAP_H, 3, bitmap, 0);
#endif
   return 0;
}
