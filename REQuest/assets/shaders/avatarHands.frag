precision highp float;
precision lowp sampler2D;
precision lowp sampler2DArray;
precision lowp samplerCube;
out lowp vec4 fragColor;

#define NUM_VIEWS 2

layout(location = 0) in VS_OUT
{
	highp vec3 position;
	mediump mat3 TBN;
//	mediump vec3 V;
	lowp vec2 texCoord;
	lowp flat uint view_id;
} fs_in;

//texture parameters
layout(binding=0) uniform sampler2DArray Albedo;
layout(binding=1) uniform sampler2DArray Normal;

struct camera_params
{
	mediump mat4	view_proj_matrix;
	mediump vec4	position;
};

layout(binding = 1, std140) uniform TRANSFORM_BLOCK
{
	camera_params camera[NUM_VIEWS];
};

struct Light
{
    mediump vec4	position;
    mediump vec4	color;
};

layout(binding = 3, std140) uniform LIGHTS
{
    Light lights[4];
};

uniform vec4 AlbedoTint;
uniform int Hand; //0 = left, 1 = right

mediump const float PI = 3.14159265359;

vec3 FresnelSchlick(lowp float cosTheta, lowp vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}  

float DistributionGGX(highp vec3 N, highp vec3 H, lowp float roughness)
{
    lowp float a      = roughness*roughness;
    lowp float a2     = a*a;
    highp float NdotH  = max(dot(N, H), 0.0);
    highp float NdotH2 = NdotH*NdotH;
	
    lowp float nom   = a2;
    highp float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return nom / denom;
}

float GeometrySchlickGGX(lowp float NdotV, lowp float roughness)
{
    lowp float r = (roughness + 1.0);
    lowp float k = (r*r) / 8.0;

    lowp float nom   = NdotV;
    lowp float denom = NdotV * (1.0 - k) + k;
	
    return nom / denom;
}

float GeometrySmith(lowp float NdotV, lowp float NdotL, lowp float roughness)
{
    lowp float ggx1  = GeometrySchlickGGX(NdotL, roughness);
    lowp float ggx2  = GeometrySchlickGGX(NdotV, roughness);
	
    return ggx1 * ggx2;
}

vec3 CalcLighting(lowp vec3 albedo, lowp vec3 F0, mediump vec3 L, mediump vec3 V, highp vec3 N, lowp vec3 radiance, lowp float roughness, lowp float diffuseIntensity)
{
    float metallic = 0.0f;

    //preliminaries
    highp vec3 H = normalize(L + V);
    lowp float NdotV = max(dot(N, V), 0.0);
    lowp float NdotL = max(dot(N, L), 0.0);

    // cook-torrance brdf
    lowp float NDF = DistributionGGX(N, H, roughness);        
    lowp float G   = GeometrySmith(NdotV, NdotL, roughness);      
    lowp vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);       

    mediump vec3 kS = F;
    mediump vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    lowp vec3 numerator    = NDF * G * F;
    lowp float denominator = 4.0 * NdotV * NdotL + 0.001; 
    lowp vec3 specular = numerator / denominator;
    // add to outgoing radiance Lo
	lowp vec3 result = (kD * albedo.xyz / PI + specular) * radiance * diffuseIntensity * NdotL;
      
    return result;
}


void main()
{
    mat3 tangentToWorld = fs_in.TBN; //mat3(vertTangent, bitangent, vertNormal);
    vec2 uv = fs_in.texCoord; //vec2(WorldViewDir.w, WorldLightDir.w);

    lowp vec4 albedoTex = texture(Albedo, vec3(uv, Hand));
      
    // Normal
    highp vec3 normalDir = texture(Normal, vec3(uv, Hand)).rgb;
    normalDir = 2.0 * normalDir - 1.0;
    highp vec3 N = normalize(tangentToWorld * normalDir);

    // View Vector     
//    vec3 V = normalize(fs_in.V);
    mediump vec3 V = normalize(camera[fs_in.view_id].position.xyz-fs_in.position.xyz);

    lowp vec3 albedoColor = albedoTex.rgb * vec3(1.0); //AlbedoTint.rgb;

    lowp vec3 F0 = vec3(0.04);
    lowp float roughness = 1.0;

    // reflectance equation
    lowp vec3 Lo = vec3(0.0);

    for (int i = 0; i < 4; ++i)
    {
        // calculate per-light radiance
        highp vec3 L = normalize(lights[i].position.xyz - fs_in.position);
        highp vec3 H = normalize(V + L);
        mediump vec3 vecPL = lights[i].position.xyz - fs_in.position;
        mediump float squaredDistPL = dot(vecPL, vecPL);
        mediump float attenuation = 1.0 / squaredDistPL;
        lowp vec3 radiance = lights[i].color.xyz * attenuation;

        lowp vec3 lit = CalcLighting(albedoColor, F0, L, V, N, radiance, roughness, 2.0);
        Lo += lit;
    }

    fragColor.rgb = Lo / (Lo + vec3(1.0));
    fragColor.a = 1.0; //albedoTex.a;
}
