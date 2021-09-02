precision highp float;
precision lowp sampler2D;
precision lowp sampler2DArray;

out lowp vec4 outColor;

#define USE_PBR
#define NUM_VIEWS 2

layout (location = 0) in VS_OUT
{
	highp vec3 position;
#if defined(USE_NORMALMAP)
	mediump mat3 TBN;
#else
	mediump vec3 N;
#endif
//	mediump vec3 V;             // CLR - Investigating whether calculating V in the vertex shader is slightly out?
	lowp vec2 texCoord;
	lowp flat int material_id;
    lowp flat uint view_id;
} fs_in;


struct MaterialProperties
{
	lowp vec4 diffuse;
	lowp vec4 specular;
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

// Material properties
layout(binding = 2, std140) uniform MATERIALS
{
	MaterialProperties materials[1000];
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

#if !defined(USE_INSTANCED_ALBEDO)
layout(binding=0) uniform sampler2D albedoSampler;
#else
layout(binding=0) uniform sampler2DArray albedoSampler;
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

void main()
{
	lowp int matId = fs_in.material_id; 

#if defined(USE_DIFFUSEMAP)
  #if !defined(USE_INSTANCED_ALBEDO)
    lowp vec4 albedo = texture(albedoSampler, fs_in.texCoord.xy);
  #else
    lowp float texId = materials[matId].specular.x;
    lowp vec4 albedo = texture(albedoSampler, vec3(fs_in.texCoord.xy, texId));
  #endif
#else
    lowp vec4 albedo = materials[matId].diffuse;
#endif

#if defined(USE_NORMALMAP)
	highp vec3 normal = texture(normalSampler, fs_in.texCoord.xy).xyz;
#endif

#if defined(USE_PBR)
  #if defined(USE_ROUGHNESSMAP)
    lowp float roughness = texture(roughnessSampler, fs_in.texCoord.xy).x;
  #else
    lowp float roughness = materials[matId].specular.w;
  #endif
#else
	lowp vec3 Ks = materials[matId].specular.xyz;
	lowp float m = materials[matId].specular.w;
#endif

#if defined(USE_NORMALMAP)
	normal = normalize(normal * 2.0 - 1.0);   
	highp vec3 N = normalize(fs_in.TBN * normal); 
#else
    highp vec3 N = normalize(fs_in.N);
#endif

//    mediump vec3 V = normalize(fs_in.V);
    mediump vec3 V = normalize(camera[fs_in.view_id].position.xyz-fs_in.position.xyz);

#if defined(USE_PBR)
    lowp vec3 F0 = vec3(0.04); 
  #if defined(USE_METALLICMAP)
    lowp float metallic = texture(metallicSampler, fs_in.texCoord.xy).x;
  #else
    lowp float metallic = 0.0;
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
    lowp vec3 Lo = vec3(0.0);

    for(int i = 0; i < 4; ++i) 
    {
#if !defined(USE_PBR)
		highp float distance = length(lights[i].position.xyz - fs_in.position);
		highp float attenuation = 1.0 / (distance); // * distance);

		mediump vec3 L = normalize(lights[i].position.xyz - fs_in.position);
		mediump vec3 H = normalize(L + V);
 
		mediump float nDotL = clamp(dot(N, L), 0.0, 1.0);
		mediump float nDotH = clamp(dot(N, H), 0.0, 1.0);

//		vec3 ambient = albedo.xyz * 0.25;
		vec3 diffuse = albedo.xyz * nDotL;
		vec3 specular = (Ks * pow(nDotH, m)); // * normFactor;
//		Lo += ((albedo.xyz * 0.25) + (albedo.xyz * nDotL * 0.85) + (Ks * pow(nDotH, m))) * attenuation;

		Lo += (/*ambient +*/ diffuse + specular) * attenuation;
#else
        // calculate per-light radiance
        mediump vec3 vecPL = lights[i].position.xyz - fs_in.position;
        mediump float squaredDistPL = dot(vecPL, vecPL);
        mediump float attenuation = 1.0 / squaredDistPL;
		lowp vec3 radiance = lights[i].color.xyz * attenuation;        

		highp vec3 L = normalize(vecPL);
        highp vec3 H = normalize(V + L);
        
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
		Lo += (kD * albedo.xyz / PI + specular) * radiance * NdotL;
#endif
	}
	lowp vec3 color = Lo;
  #if defined(USE_AOMAP)
    color = color * vec3(ao);
  #endif
    color = color / (color + vec3(1.0));

	outColor = vec4(color, albedo.w);
#if defined(USE_EMISSIVEMAP)
	lowp vec4 Kd = texture(emissiveSampler, fs_in.texCoord.xy);
	outColor = vec4(Kd.xyz, 1.0);
#endif
}
