
#define USE_PBR
//#define USE_FOG

out vec4 outColor;

layout(location = 0) in VS_OUT
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
} fs_in;


struct MaterialProperties
{
    vec4 diffuse;
    vec4 specular;
};

#define NUM_VIEWS 2
struct camera_params
{
	mat4	view_proj_matrix;
	vec4	position;
};

layout(binding = 1, std140) uniform TRANSFORM_BLOCK
{
	camera_params camera[NUM_VIEWS];
};

// Material properties
layout(binding = 2, std140) uniform MATERIALS
{
    MaterialProperties materials[1000];
};

struct Light
{
    vec4	position;
    vec4	color;
};

layout(binding = 3, std140) uniform LIGHTS
{
    Light lights[4];
};

#if defined(USE_INSTANCED_ALBEDO)
layout(binding=0) uniform sampler2DArray albedoSampler;
#else
layout(binding=0) uniform sampler2D albedoSampler;
#endif
layout(binding=1) uniform sampler2D normalSampler;
layout(binding=2) uniform sampler2D metallicSampler;
layout(binding=3) uniform sampler2D roughnessSampler;
layout(binding=4) uniform sampler2D emissiveSampler;
#if defined(USE_INSTANCED_AO)
layout(binding=5) uniform sampler2DArray aoSampler;
#else
layout(binding=5) uniform sampler2D aoSampler;
#endif

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 FresnelSchlick(float cosTheta, vec3 F0);
float ComputeLinearFogFactor(float eyeDist);
vec3 ApplyHalfspaceFog(vec3 shadedColor, vec3 v, float fv, float u1, float u2);

void main()
{
    int matId = fs_in.material_id;
 
#if defined(USE_DIFFUSEMAP)
#if defined(USE_INSTANCED_ALBEDO)
    int texIndex = int(materials[matId].specular.x);
    vec4 albedo = texture(albedoSampler, vec3(fs_in.texCoord.xy, texIndex));
#else
    vec4 albedo = texture(albedoSampler, fs_in.texCoord.xy); // * vec4(materials[matId].diffuse.xyz, 1.0);
#endif
#else
    vec4 albedo = materials[matId].diffuse;
#endif

#if defined(USE_NORMALMAP)
	vec3 normal = texture(normalSampler, fs_in.texCoord.xy).xyz;
#endif

#if defined(USE_PBR)
  #if defined(USE_ROUGHNESSMAP)
    float roughness = texture(roughnessSampler, fs_in.texCoord.xy).x;
  #else
    float roughness = materials[matId].specular.w;
  #endif
#else
    vec3 Ks = materials[matId].specular.xyz;
    float m = materials[matId].specular.w;
#endif

#if defined(USE_NORMALMAP)
	normal = normalize(normal * 2.0 - 1.0);   
    vec3 N = normalize(fs_in.TBN * normal); 
#else
    vec3 N = normalize(fs_in.N);
#endif

    vec3 V = normalize(camera[fs_in.view_id].position.xyz - fs_in.position);

#if defined(USE_PBR)
    vec3 F0 = vec3(0.04);
  #if defined(USE_METALLICMAP)
    float metallic = texture(metallicSampler, fs_in.texCoord.xy).x;
  #else
    float metallic = 0.0;
  #endif
    F0 = mix(F0, albedo.xyz, metallic);

  #if defined(USE_AOMAP)
    #if defined(USE_INSTANCED_AO)
      int texIndex = int(materials[matId].specular.x);
      float ao = texture(aoSampler, vec3(fs_in.texCoord.xy, texIndex)).x;
    #else
      float ao = texture(aoSampler, fs_in.texCoord.xy * vec2(1.0/2.0)).x;
    #endif
  #endif
#endif


    // reflectance equation
    vec3 Lo = vec3(0.0);

    //	float normFactor = (m + 2.0f)/(4.0f*PI*( 2.0f-exp2(-m / 2.0f)));

    for (int i = 0; i < 4; ++i)
    {
#if !defined(USE_PBR)
        float distance = length(lights[i].position.xyz - fs_in.position);
        float attenuation = 1.0 / (distance); // * distance);

        vec3 L = normalize(lights[i].position.xyz - fs_in.position);
        vec3 H = normalize(L + V);

        float nDotL = clamp(dot(N, L), 0.0, 1.0);
        float nDotH = clamp(dot(N, H), 0.0, 1.0);

        //		vec3 ambient = albedo.xyz * 0.25;
        vec3 diffuse = albedo.xyz * nDotL;
        vec3 specular = (Ks * pow(nDotH, m)); // * normFactor;
//		Lo += ((albedo.xyz * 0.25) + (albedo.xyz * nDotL * 0.85) + (Ks * pow(nDotH, m))) * attenuation;

        Lo += (/*ambient +*/ diffuse + specular) * attenuation;
#else
        // calculate per-light radiance
        vec3 L = normalize(lights[i].position.xyz - fs_in.position);
        vec3 H = normalize(V + L);
        vec3 vecPL = lights[i].position.xyz - fs_in.position;
        float squaredDistPL = dot(vecPL, vecPL);
        float attenuation = 1.0 / squaredDistPL;
        vec3 radiance = lights[i].color.xyz * attenuation;

        // cook-torrance brdf
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
        vec3 specular = numerator / denominator;
        // add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo.xyz / PI + specular) * radiance * vec3(NdotL);
#endif
    }
    vec3 color = Lo; //clamp(Lo + emissive, 0.0, 1.0);	// * ao;	

#if defined(USE_AOMAP)
    color = color * vec3(ao);
#endif

#if defined(USE_FOG)
//    float fogFactor = ComputeLinearFogFactor(fs_in.eyeDist);
//    const vec3 kFogColor = vec3(0.5, 0.5, 0.5);
//    color = mix(kFogColor, color, fogFactor);
    float fv = fs_in.fogParams.x;
    float u1 = fs_in.fogParams.y;
    float u2 = fs_in.fogParams.z;
    color = ApplyHalfspaceFog(color, fs_in.unormV, fv, u1, u2);
#endif //! USE_FOG

    color = color / (color + vec3(1.0));
    outColor = vec4(color, albedo.w);
#if defined(USE_EMISSIVEMAP)
	vec4 Kd = texture(emissiveSampler, fs_in.texCoord.xy);
	outColor = vec4(Kd.xyz, 1.0);
#endif
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float ComputeLinearFogFactor(float eyeDist)
{
    const float kFogMaxDist = 10.0;
    const float kFogMinDist = 1.0;
 
    float factor = (kFogMaxDist - eyeDist) / (kFogMaxDist - kFogMinDist);
    factor = clamp(factor, 0.0, 1.0);

    return factor;
}

vec3 ApplyHalfspaceFog(vec3 shadedColor, vec3 v, float fv, float u1, float u2)
{
    const float kFogEpsilon = 0.0001;
    const float kFogDensity = 0.25;
    const vec3 kFogColor = vec3(0.5, 0.5, 0.5);

    float x = min(u2, 0.0);
    float tau = 0.5 * kFogDensity * length(v) * (u1 - x * x / (abs(fv) + kFogEpsilon));
    return mix(kFogColor, shadedColor, exp(tau));
}