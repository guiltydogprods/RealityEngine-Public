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

precision highp float;

layout(location = 0) in vec4 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;
layout(location = 3) in ivec4 JointIndices;
layout(location = 4) in vec4 JointWeights;

layout(binding = 0, std140) uniform MODEL_MATRIX_BLOCK
{
	mat4    model_matrix;
};

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
	highp vec3 position;
	mediump vec3 N;
	mediump vec3 V;
	lowp vec2 texCoord;
	lowp flat uint view_id;
} vs_out;

struct MatStruct
{
    mat4 Bones[28]; //Skinning bones
};

layout (binding = 2, std140) uniform VertMaterials
{
    MatStruct Data;
}mats;

vec4 TransformVertex( highp vec4 oPos )
{
    vec4 hPos =  camera[VIEW_ID].view_proj_matrix * model_matrix * oPos;
    return hPos;
}

void CalcPoseAnimated(in vec4 position, in vec3 normal, in mat4 joint0, in mat4 joint1, in mat4 joint2, in mat4 joint3, in vec4 weights,
                      out vec4 vertPose, out vec4 normPose)
{
    vertPose = joint0 * position * weights.x;
    vertPose += joint1 * position * weights.y;
    vertPose += joint2 * position * weights.z;
    vertPose += joint3 * position * weights.w;

    vec4 normal4 = vec4( normal, 0.0 );
    normPose = joint0 * normal4 * weights.x;
    normPose += joint1 * normal4 * weights.y;
    normPose += joint2 * normal4 * weights.z;
    normPose += joint3 * normal4 * weights.w;
    normPose = normalize( normPose );
}

void main()
{
    vec4 position = Position;
    vec3 normal = Normal;

    mat4 joint0 = mats.Data.Bones[JointIndices.x];
    mat4 joint1 = mats.Data.Bones[JointIndices.y];
    mat4 joint2 = mats.Data.Bones[JointIndices.z];
    mat4 joint3 = mats.Data.Bones[JointIndices.w];

    vec4 vertexPose;
    vec4 normalPose;
    CalcPoseAnimated(position, normal, joint0, joint1, joint2, joint3, JointWeights, vertexPose, normalPose);

    gl_Position = TransformVertex( vertexPose );
    vec3 vertexWorldPos = ( model_matrix * vertexPose ).xyz;
    normalPose = model_matrix * normalPose;

    vs_out.N = normalize(normalPose.xyz);
    vs_out.V = camera[VIEW_ID].position.xyz - vertexWorldPos.xyz;

    vs_out.view_id = VIEW_ID;
    vs_out.texCoord = TexCoord;
    vs_out.position = vertexWorldPos;
}
