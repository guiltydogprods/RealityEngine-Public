#ifndef DISABLE_MULTIVIEW
#define DISABLE_MULTIVIEW 0
#endif

//#define USE_FOG
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
	mat4    model_matrix[256];
};

struct camera_params
{
	mat4	view_proj_matrix;
	vec4	position;
};

layout(binding = 1, std140) uniform TRANSFORM_BLOCK
{
	camera_params camera[NUM_VIEWS];
#if defined(USE_FOG)
	vec4	fogPlane;
#endif
};

#if defined(USE_SKINNING)
layout(binding = 4, std140) uniform SKINNING_MATRIX_BLOCK
{
	mat4    bone_palette[20];
};
#endif

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out VS_OUT
{
	vec3 position;
#if defined(USE_NORMALMAP)
	mat3 TBN;
#else
	vec3 N;
#endif
	vec3 V;
	vec2 texCoord;
#if defined(USE_FOG)
	vec3 unormV;
	vec3 fogParams;		//.x = fv, .y = u1, .z = u2
	float eyeDist;
#endif
	flat int view_id;
	flat int material_id;
} vs_out;


vec3 light_pos = vec3(100.0, 100.0, 100.0);

void main()
{
	int instId = gl_InstanceID;
//	mat4 view_proj_matrix = proj_matrix[VIEW_ID] * view_matrix[VIEW_ID];
#if defined(USE_SKINNING)
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
#else
	vec4 P = model_matrix[instId] * vec4(position, 1.0);
  #if defined(USE_NORMALMAP)
	vec3 N = normalize(mat3(model_matrix[instId]) * normal);
	vec3 T = normalize(mat3(model_matrix[instId]) * tangent);
	vec3 B = normalize(mat3(model_matrix[instId]) * binormal);
	vs_out.TBN = mat3(T, B, N);
  #else
    vs_out.N = normalize(mat3(model_matrix[instId]) * normal);
  #endif
#endif
	vec4 camPos = camera[VIEW_ID].position;
	vec3 V = camPos.xyz - P.xyz;
#if defined(USE_FOG)
	vec4 fogPlane = fogPlane;
	float fDotC = dot(fogPlane, camPos);
	float m = fDotC < 0.0 ? 1.0 : 0.0;
	float signFdotC = sign(fDotC);
	float fDotP = dot(fogPlane, P);
	float fv = dot(fogPlane, vec4(V, 0.0));
	float u1 = m * (fDotC + fDotP);
	float u2 = fDotP * signFdotC;
	vs_out.unormV = V;
	vs_out.fogParams.xyz = vec3(fv, u1, u2);
	vs_out.eyeDist = length(P.xyz - camPos.xyz);
#endif //! USE_FOG
	
	vs_out.V = normalize(V);
	vs_out.view_id = VIEW_ID;
	vs_out.material_id = instId;
	vs_out.texCoord = texCoord;
	vs_out.position = P.xyz;

	gl_Position = camera[VIEW_ID].view_proj_matrix * P;
}
