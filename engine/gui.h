#pragma once

#include "math/vec4.h"
#include "rgfx/renderer.h"

typedef struct rguiApi rguiApi;

typedef struct rgui_panel { int32_t id; } rgui_panel;
typedef struct rgui_texture { int32_t id; } rgui_texture;
typedef struct rgui_textureSet { int32_t id; } rgui_textureSet;

typedef void rgui_action(void); // const void*, const void*);

typedef struct rgui_buttonDesc
{
	union
	{
		mat4x4 transform;
		struct
		{
			struct
			{
				vec4 xAxis;
				vec4 yAxis;
				vec4 zAxis;
			} rotation;
			vec4 position;
		};
	};
	rgui_action^ action;
	float x;
	float y;
	float width;
	float height;
	rgui_texture texture;
	rgui_panel panel;
	int32_t buttonQuad;
	uint32_t layoutFlags;
	uint32_t flags;
	bool bDisabled;
	bool pad[3];
}rgui_buttonDesc;

typedef struct rgui_frameDesc
{
	mat4x4 hmdTransform;
	mat4x4 controllerTransform;
	vec4 pointerRayStartPosition;
	vec4 pointerRayEndPosition;
	vec4 pointerRayVec;
	int32_t pointerHand;
	float controllerTrigger;
	float pointerLength;
	int32_t pad;
}rgui_frameDesc;

typedef enum rgui_align
{
	kAlignDefault = 0,					// CLR: Default Alignment?
	kAlignHLeft	= 0x01,
	kAlignHRight = 0x02,
	kAlignHCenter = 0x04,
	kAlignVTop = 0x10,
	kAlignVCenter = 0x20,
	kAlignVBottom = 0x40,
	kAlignVCenterHeading = 0x80,
}rgui_align;

typedef enum rgui_widgetType
{
	kInvalidWidget = 0,
	kPanelWidget,					// CLR: Default Alignment?
	kTexturedQuad,
	kSliderWidget,
	kButtonWidget,
}rgui_widgetType;

typedef struct vec3f
{
	float x, y, z;
}vec3f;

typedef struct rgui_collisionQuad
{
	vec3f points[4];
	rgui_action ^action;
	rgui_widgetType widgetType;
	int32_t widgetIndex;
	uint32_t panelHash;
	bool bDisabled;
	bool pad[3];
}rgui_collisionQuad;

typedef enum rgui_flags
{
	kCollidable = 0x01,
	kGreyedOut,
}rgui_flags;

typedef struct rgui_panelDesc
{
	union
	{
		mat4x4 transform;
		struct
		{
			struct
			{
				vec4 xAxis;
				vec4 yAxis;
				vec4 zAxis;
			} rotation;
			vec4 position;
		};
	};
	vec4 color;
	vec4 collisionColor;
	vec4 bannerColor;
	const char* bannerText;
	float bannerHeight;
	float width;
	float height;
	rgui_texture texture;
	rgui_font bannerFont;
	uint32_t panelHash;
	uint32_t layoutFlags;
	uint32_t flags;
}rgui_panelDesc;

typedef struct rgui_pointerDesc
{
	union
	{
		mat4x4 transform;
		struct
		{
			struct
			{
				vec4 xAxis;
				vec4 yAxis;
				vec4 zAxis;
			} rotation;
			vec4 position;
		};
	};
	vec4 startPosition;
	vec4 endPosition;
	vec4 color;
	float thickness;
	uint32_t layoutFlags;
}rgui_pointerDesc;

typedef struct rgui_quadDesc
{
	vec4 color;
	vec4 collisionColor;
	float x;
	float y;
	float z;
	float width;
	float height;
	rgui_texture texture;
	rgui_panel panel;
	uint32_t layoutFlags;
	uint32_t flags;
}rgui_quadDesc;

typedef struct rgui_sliderDesc
{
	union
	{
		mat4x4 transform;
		struct
		{
			struct
			{
				vec4 xAxis;
				vec4 yAxis;
				vec4 zAxis;
			} rotation;
			vec4 position;
		};
	};
	vec4 color;
	rgui_action^ action;
	float x;
	float y;
	float width;
	float height;
	float thickness;
	float* value;
	float internalValue;
	float minValue;
	float maxValue;
	float step;
	int32_t sliderPanel;
	uint32_t layoutFlags;
	rgui_texture baseTexture;
	rgui_texture sliderTexture;
	rgui_panel panel;
}rgui_sliderDesc;

typedef struct rgui_textDesc
{
	union
	{
		mat4x4 transform;
		struct
		{
			struct
			{
				vec4 xAxis;
				vec4 yAxis;
				vec4 zAxis;
			} rotation;
			vec4 position;
		};
	};
	float x;
	float y;
	float z;
	float scale;
	const char* text;
	uint32_t stringHash;
	rgui_font font;
	rgfx_debugColor color;
	rgui_panel panel;
	uint32_t layoutFlags;
}rgui_textDesc;

typedef struct rgui_textureSetInfo
{
	rgfx_texture texture;
	rsys_hash textureLookup;
}rgui_textureSetInfo;


rguiApi rgui_initialize(void);
void rgui_newFrame(rgui_frameDesc* desc);
bool rgui_button(rgui_buttonDesc* desc);
rgui_panel rgui_beginPanel(const rgui_panelDesc* desc);
void rgui_endPanel(void);
rgui_texture rgui_findTexture(const char* textureName);
int32_t rgui_pointer(const rgui_pointerDesc* desc);
void rgui_mouseRay(vec4 pos0, vec4 pos1);
int32_t rgui_quad(const rgui_quadDesc* desc);
void rgui_registerTextureSet(const char** textureNames, int32_t numTextures);
int32_t rgui_slider(rgui_sliderDesc* desc);
int32_t rgui_text3D(const rgui_textDesc* desc);
void rgui_endFrame(void);
void rgui_update(bool triggerPressed);
void rgui_render(int32_t eye, bool bHmdMounted);

typedef struct rguiApi
{
	bool(*button)(rgui_buttonDesc* desc);
	rgui_panel(*beginPanel)(const rgui_panelDesc* desc);
	void(*endPanel)(void);
	rgui_texture(*findTexture)(const char* textureName);
	int32_t(*pointer)(const rgui_pointerDesc* desc);
	void(*mouseRay)(vec4 pos0, vec4 pos1);
	void(*registerTextureSet)(const char** textureNames, int32_t numTextures);
	int32_t(*slider)(rgui_sliderDesc* desc);
	int32_t(*text3D)(const rgui_textDesc* desc);
	void(*update)(bool triggerPressed);
	void(*newFrame)(const rgui_frameDesc* desc);
	void(*endFrame)(void);
}rguiApi;
