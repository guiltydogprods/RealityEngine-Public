//precision mediump float;
//precision mediump sampler2D;
//precision mediump sampler2DArray;
//precision mediump samplerCube;

out vec4 fragColor;

layout(location = 0) in VS_OUT
{
	vec3 position;
	mat3 TBN;
	vec3 cam_pos;
	vec2 texCoord;
	flat int view_id;
	flat int componentIndex;
} fs_in;

//texture parameters
layout(binding=0) uniform sampler2DArray Albedo;
layout(binding=1) uniform sampler2DArray Normal;
layout(binding=2) uniform sampler2DArray Metallic;
//layout(binding=3) uniform samplerCube EnvironmentMap;

//From Avatar SDK - OVR_Avatar.h ovrAvatarBodyPartType::ovrAvatarBodyPartType_Count
#define ovrAvatarBodyPartType_Count 5

struct MatStruct
{
    vec4 AlbedoTint[ovrAvatarBodyPartType_Count]; // One per renderpart.
    vec4 Intensity[ovrAvatarBodyPartType_Count]; //Diffuse in r, Reflection in g, brow in b, lip smoothness in a
    vec4 Colors[7]; // Expressive Colors
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

layout (binding = 4, std140) uniform FragMaterials
{
    MatStruct Data;
} mats;

//values of roughness texture for selecting a color. Because of
//mips/texture compression, the value wont be exact, round to closest.
// Iris 255/255
// Sclera 238/255
// Lashes 221/255
// Lips 204/255
// Gums 187/255
// Teeth 170/255
// Brows 153/255
const float MASK_MIN = 153.0/255.0;
const float MASK_DIVISOR = 17.0/255.0;
const float MASK_COUNT = 7.0;

// Brightness modifier order: Brows, Teeth, Gums, Lips, Lashes, Sclera, Iris
const float MASK_BRIGHTNESS_MODIFIERS[7] = float[7](1.0, 1.0, 1.0, 1.0, 1.0, 1.1, 2.0);

const int BODY_COMPONENT = 0;
const int BROWS_COLOR_INDEX = 0;
const int TEETH_COLOR_INDEX = 1;
const int GUMS_COLOR_INDEX = 2;
const int LIPS_COLOR_INDEX = 3;
const int LASHES_COLOR_INDEX = 4;
const int SCLERA_COLOR_INDEX = 5;
const int IRIS_COLOR_INDEX = 6;

const float BROWS_LASHES_DIFFUSEINTENSITY = 0.75;
const float LIP_SMOOTHNESS_MULTIPLIER = 0.5;
const float MAX_SMOOTHNESS = 0.5;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 FresnelSchlick(float cosTheta, vec3 F0);

vec3 CalcLightingExpressive(vec3 albedo, vec4 F0_metallic, highp vec3 L, highp vec3 V, highp vec3 N, vec3 radiance, float roughness, float diffuseIntensity, float lipGlossiness, float eyeGlint)
{
    //constants
    const float LIP_GLINT_SPREAD = 150.0;
    const float EYE_GLINT_SPREAD = 600.0;

    vec3 F0 = F0_metallic.xyz;
    float metallic = F0_metallic.w;

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
      
    lipGlossiness *= lipGlossiness > 0.0 ? pow(NdotH, LIP_GLINT_SPREAD) : 0.0;
    eyeGlint *= eyeGlint > 0.0 ? pow(NdotH, EYE_GLINT_SPREAD) : 0.0;
    result.rgb += lipGlossiness + eyeGlint;
    return result;
}

void main()
{
    mat3 tangentToWorld = fs_in.TBN; //mat3(vertTangent, bitangent, vertNormal);
    vec2 uv = fs_in.texCoord;

    int matIndex = fs_in.componentIndex; //fs_in.int(round(VextexNormal.w));

    vec4 albedoTex = texture(Albedo, vec3(uv,matIndex));
      
    // Normal
    vec3 normalDir = texture(Normal, vec3(uv,matIndex)).rgb;
    normalDir = 2.0 * normalDir - 1.0;
    vec3 N = normalize(tangentToWorld * normalDir);

    // View Vector     
    vec3 V = normalize(fs_in.cam_pos - fs_in.position);

    // Surface properties
    vec3 albedoTint = mats.Data.AlbedoTint[matIndex].rgb;
    float diffuseIntensity = 2.0; //mats.Data.Intensity[matIndex].r;

    vec4 roughnessTex = texture(Metallic, vec3(uv,matIndex));
    float roughness = roughnessTex.a; // * mats.Data.Intensity[matIndex].g); // * reflectionColor
      
    // Mask colors
    int colorIndex = int(clamp(round((roughnessTex.b - MASK_MIN) / MASK_DIVISOR), 0.0, MASK_COUNT-1.0));
    vec3 colorMask = mats.Data.Colors[colorIndex].rgb;
    colorMask *= MASK_BRIGHTNESS_MODIFIERS[colorIndex];
    float mixValue = matIndex == BODY_COMPONENT ? roughnessTex.g : 0.0;
    mixValue *= mats.Data.Colors[colorIndex].a;
    albedoTint = mix(albedoTint, colorMask, mixValue);
        
    // Mask regions
    float maskBrowsScalar = colorIndex == BROWS_COLOR_INDEX ? mixValue : 0.0;
    float maskLashesScalar = colorIndex == LASHES_COLOR_INDEX ? mixValue : 0.0;
    float maskLipsScalar = colorIndex == LIPS_COLOR_INDEX ? mixValue : 0.0;
    float maskIrisScalar = colorIndex == IRIS_COLOR_INDEX ? mixValue : 0.0;
    float maskScleraScalar = colorIndex == SCLERA_COLOR_INDEX ? mixValue : 0.0;
    float maskTeethScalar = colorIndex == TEETH_COLOR_INDEX ? mixValue : 0.0;
    float maskGumsScalar = colorIndex == GUMS_COLOR_INDEX ? mixValue : 0.0;

    // Lerp diffuseIntensity with roughness map. This clamps the min diffuseIntensity to the
    // per-component property, and allows it to vary with the texture value up to a maximum.
//    diffuseIntensity = mix(diffuseIntensity, MAX_SMOOTHNESS, roughness);
    // Brows and lashes, teeth and gums modify DiffuseIntensity
    diffuseIntensity *= (1.0 - ((maskBrowsScalar + maskLashesScalar) * BROWS_LASHES_DIFFUSEINTENSITY));
    diffuseIntensity *= (1.0 - (maskTeethScalar + maskGumsScalar));
    roughness = 1.0 - roughness;

    vec3 baseColor = all(equal(albedoTex.rgba, vec4(0.0))) ? albedoTint : albedoTint * albedoTex.rgb;
    vec3 albedoColor = colorIndex == LIPS_COLOR_INDEX ? mix(albedoTex.rgb, colorMask, mixValue) : baseColor;
    albedoColor = (matIndex == 2 /*|| matIndex == 3*/) ? albedoTint : albedoColor;

//    highp vec3 reflection = reflect(-viewDir, normalDir);
    //vec3 reflectionColor = texture(EnvironmentMap, reflection).rgb;
    // Smoothness multiplier on lip region
    float lipGlossiness = maskLipsScalar * (mats.Data.Intensity[matIndex].a * LIP_SMOOTHNESS_MULTIPLIER);
    float eyeGlint = clamp(maskIrisScalar + maskScleraScalar, 0.0, 1.0);

    vec3 F0 = vec3(0.04);
    float metallic = 0.0;
    F0 = mix(F0, albedoColor.xyz, metallic);
    vec4 F0_metallic = vec4(F0, metallic);
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

        vec3 lit = CalcLightingExpressive(albedoColor, F0_metallic, L, V, N, radiance, roughness, diffuseIntensity, lipGlossiness, eyeGlint);
        Lo += lit;
    }
    vec3 color = Lo;
    color = color / (color + vec3(1.0));

    fragColor = vec4(color, 1.0); //albedoTex.a;
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
