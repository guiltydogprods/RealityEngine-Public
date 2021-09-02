precision highp float;
precision lowp sampler2D;
precision lowp sampler2DArray;
precision lowp samplerCube;
out lowp vec4 fragColor;

layout(location = 0) in VS_OUT
{
	highp vec3 position;
	mediump vec3 N;
	mediump vec3 V;
	lowp vec2 texCoord;
	lowp flat uint view_id;
} fs_in;

//texture parameters
layout(binding=0) uniform sampler2DArray Albedo;
//layout(binding=1) uniform sampler2DArray Normal;

#define NUM_VIEWS 2
layout(binding = 1, std140) uniform TRANSFORM_BLOCK
{
	mat4    view_matrix[NUM_VIEWS];
	mat4    proj_matrix[NUM_VIEWS];
	vec4	cam_pos[NUM_VIEWS];
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

uniform vec4 AlbedoTint;
uniform int Hand; //0 = left, 1 = right

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 FresnelSchlick(float cosTheta, vec3 F0);

vec3 CalcLighting(vec3 albedo, vec3 F0, highp vec3 L, highp vec3 V, highp vec3 N, vec3 radiance, float roughness, float diffuseIntensity)
{
    float metallic = 0.0f;

    //preliminaries
    vec3 H = normalize(L + V);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    // cook-torrance brdf
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
    vec3 specular = numerator / denominator;
    // add to outgoing radiance Lo
    vec3 result = (kD * albedo.xyz / PI + specular) * radiance * diffuseIntensity * NdotL;
      
    return result;
}


void main()
{
    vec2 uv = fs_in.texCoord; //vec2(WorldViewDir.w, WorldLightDir.w);

    vec4 albedoTex = texture(Albedo, vec3(uv, Hand));
      
    // Normal
    vec3 N = normalize(fs_in.N);

    // View Vector     
    vec3 V = normalize(fs_in.V);

    vec3 albedoColor = albedoTex.rgb * vec3(1.0); //AlbedoTint.rgb;

    vec3 F0 = vec3(0.04);
    float roughness = 1.0;

    // reflectance equation
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < 4; ++i)
    {
        // calculate per-light radiance
        vec3 L = normalize(lights[i].position.xyz - fs_in.position);
        vec3 H = normalize(V + L);
        vec3 vecPL = lights[i].position.xyz - fs_in.position;
        float squaredDistPL = dot(vecPL, vecPL);
        float attenuation = 1.0 / squaredDistPL;
        vec3 radiance = lights[i].color.xyz * attenuation;

        vec3 lit = CalcLighting(albedoColor, F0, L, V, N, radiance, roughness, 2.0);
        Lo += lit;
    }

    fragColor.rgb = Lo / (Lo + vec3(1.0));
    fragColor.a = 1.0; //albedoTex.a;
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
