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
layout(location = 2) in vec3 Tangent;
layout(location = 3) in vec2 TexCoord;
layout(location = 4) in ivec2 JointIndices;
layout(location = 5) in vec2 JointWeights;

layout(binding = 0, std140) uniform MODEL_MATRIX_BLOCK
{
	mat4    model_matrix[2];
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
	mediump mat3 TBN;
//	mediump vec3 V;
	lowp vec2 texCoord;
	lowp flat uint view_id;
} vs_out;

struct MatStruct
{
    mat4 Bones[28]; //Skinning bones
};

layout (binding = 2, std140) uniform VertMaterials
{
    MatStruct Data[2];
}mats;

vec4 TransformVertex( highp vec4 oPos )
{
    vec4 hPos =  camera[VIEW_ID].view_proj_matrix * model_matrix[gl_InstanceID] * oPos;
    return hPos;
}

void CalcPoseAnimated(in vec4 position, in vec3 normal, in vec3 tangent, in mat4 joint0, in mat4 joint1, in vec2 weights,
                      out vec4 vertPose, out vec4 normPose, out vec4 tangentPose)
{
    vertPose = joint0 * position * weights.x;
    vertPose += joint1 * position * weights.y;

    vec4 normal4 = vec4( normal, 0.0 );
    normPose = joint0 * normal4 * weights.x;
    normPose += joint1 * normal4 * weights.y;
    normPose = normalize( normPose );

    vec4 tangent4 = vec4( tangent, 0.0 );
    tangentPose = joint0 * tangent4 * weights.x;
    tangentPose += joint1 * tangent4 * weights.y;
    tangentPose = normalize( tangentPose );
}

void main()
{
    vec4 position = Position;
    vec3 normal = Normal;
    vec3 tangent = Tangent;

    mat4 joint0 = mats.Data[gl_InstanceID].Bones[JointIndices.x];
    mat4 joint1 = mats.Data[gl_InstanceID].Bones[JointIndices.y];

    vec4 vertexPose;
    vec4 normalPose;
    vec4 tangentPose;
    CalcPoseAnimated(position, normal, tangent, joint0, joint1, JointWeights, vertexPose, normalPose, tangentPose);

    gl_Position = TransformVertex( vertexPose );
    vec3 vertexWorldPos = ( model_matrix[gl_InstanceID] * vertexPose ).xyz;
    normalPose = model_matrix[gl_InstanceID] * normalPose;
    tangentPose = model_matrix[gl_InstanceID] * tangentPose;

    vec3 vertNormal = normalize(normalPose.xyz);
    vec3 vertTangent = normalize(tangentPose.xyz);
    vec3 bitangent = cross(vertNormal, vertTangent);

    vs_out.TBN = mat3(vertTangent, bitangent, vertNormal);
//    vs_out.V = camera[VIEW_ID].position.xyz - vertexWorldPos.xyz;

    vs_out.view_id = VIEW_ID;
    vs_out.texCoord = TexCoord;
    vs_out.position = vertexWorldPos;
}
