#ifndef DISABLE_MULTIVIEW
#define DISABLE_MULTIVIEW 0
#endif

#define NUM_VIEWS 2
#if defined( GL_OVR_multiview2 ) && ! DISABLE_MULTIVIEW
    #extension GL_OVR_multiview2 : enable
    layout(num_views = NUM_VIEWS) in;
    #define VIEW_ID gl_ViewID_OVR
#else
    uniform lowp int ViewID;
    #define VIEW_ID ViewID
#endif

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec4 color;
layout(location = 3) in uint texIdx;

/*
layout(binding = 0, std140) uniform MODEL_MATRIX_BLOCK
{
	mat4    model_matrix;
};
*/

struct camera_params
{
	mat4	view_proj_matrix;
	vec4	position;
};

layout(binding = 1, std140) uniform TRANSFORM_BLOCK
{
	camera_params camera[NUM_VIEWS];
};

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out VS_OUT
{	
    mediump vec2 texCoord;
    lowp vec4 color;
    lowp flat uint texIdx;
} vs_out;

void main()
{
    vs_out.texCoord = texCoord;
    vs_out.color = color;
    vs_out.texIdx = texIdx;
	gl_Position = camera[VIEW_ID].view_proj_matrix * vec4(position.xyz, 1.0);
}
