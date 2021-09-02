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
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec3 binormal;
layout(location = 4) in vec2 texCoord;
layout(location = 5) in ivec4 boneIndices;
layout(location = 6) in vec4 boneWeights;

layout(binding = 0, std140) uniform MODEL_MATRIX_BLOCK
{
	mat4    model_matrix[1000];
};

layout(binding = 1, std140) uniform TRANSFORM_BLOCK
{
	mat4    view_matrix[NUM_VIEWS];
	mat4    proj_matrix[NUM_VIEWS];
	vec4	cam_pos[NUM_VIEWS];
};

layout(binding = 4, std140) uniform SKINNING_MATRIX_BLOCK
{
	mat4    bone_palette[20];
};

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out VS_OUT
{
	vec3 position;
	vec3 N;
	vec3 V;
	vec2 texCoord;
	flat int view_id;
	flat int material_id;
} vs_out;


vec3 light_pos = vec3(100.0, 100.0, 100.0);

void main()
{
	int instId = gl_InstanceID;
	mat4 view_proj_matrix = proj_matrix[VIEW_ID] * view_matrix[VIEW_ID];

	int baseIndex = 0; //11 * gl_DrawIDARB;	//CLR - Massive Hack!!!  As we only have the two hand meshes that are skin, we can hard code the bone count... For now...
	vec4 skinnedPosition = boneWeights.x * (bone_palette[baseIndex + boneIndices.x] * vec4(position, 1.0));
	skinnedPosition += boneWeights.y * (bone_palette[baseIndex + boneIndices.y] * vec4(position, 1.0));
	skinnedPosition += boneWeights.z * (bone_palette[baseIndex + boneIndices.z] * vec4(position, 1.0));
	skinnedPosition += boneWeights.w * (bone_palette[baseIndex + boneIndices.w] * vec4(position, 1.0));

	vec4 skinnedNormal = boneWeights.x * (bone_palette[baseIndex + boneIndices.x] * vec4(normal, 0.0));
	skinnedNormal += boneWeights.y * (bone_palette[baseIndex + boneIndices.y] * vec4(normal, 0.0));
	skinnedNormal += boneWeights.z * (bone_palette[baseIndex + boneIndices.z] * vec4(normal, 0.0));
	skinnedNormal += boneWeights.w * (bone_palette[baseIndex + boneIndices.w] * vec4(normal, 0.0));

	vec4 P = model_matrix[instId] * vec4(skinnedPosition.xyzw);

	vs_out.N = normalize(mat3(model_matrix[instId]) * skinnedNormal.xyz);
	vs_out.V = cam_pos[VIEW_ID].xyz - P.xyz;

	vs_out.material_id = instId;
	vs_out.view_id = VIEW_ID;
	vs_out.texCoord = texCoord;
	vs_out.position = P.xyz;

	gl_Position = view_proj_matrix * P;
}
