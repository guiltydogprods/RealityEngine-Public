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

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out VS_OUT
{
	vec2 outUV;
} vs_out;

const vec2 quadVertices[4] = { vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0) };

void main() 
{
    vec2 outUV = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vs_out.outUV = outUV * vec2(1.0, -1.0);
    gl_Position = vec4(outUV * 2.0f + -1.0f, 0.0f, 1.0f);
}
