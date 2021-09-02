//precision mediump float;
//precision mediump sampler2D;
//precision mediump sampler2DArray;
//precision mediump samplerCube;
precision highp float;
precision lowp sampler2D;
precision lowp sampler2DArray;
out lowp vec4 fragColor;

#define NUM_VIEWS 2

layout(location = 0) in VS_OUT
{
	highp vec3 position;
	mediump mat3 TBN;
//	mediump vec3 V;
	lowp vec2 texCoord;
	lowp flat uint view_id;
	lowp flat int componentIndex;
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

lowp const int BODY_COMPONENT = 0;
lowp const int BROWS_COLOR_INDEX = 0;
lowp const int TEETH_COLOR_INDEX = 1;
lowp const int GUMS_COLOR_INDEX = 2;
lowp const int LIPS_COLOR_INDEX = 3;
lowp const int LASHES_COLOR_INDEX = 4;
lowp const int SCLERA_COLOR_INDEX = 5;
lowp const int IRIS_COLOR_INDEX = 6;

const float BROWS_LASHES_DIFFUSEINTENSITY = 0.75;
const float LIP_SMOOTHNESS_MULTIPLIER = 0.5;
const float MAX_SMOOTHNESS = 0.5;

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

vec3 CalcLightingExpressive(lowp vec3 albedo, lowp vec4 F0_metallic, mediump vec3 L, mediump vec3 V, highp vec3 N, lowp vec3 radiance, lowp float roughness, lowp float diffuseIntensity, lowp float lipGlossiness, lowp float eyeGlint)
{
    //constants
    mediump const float LIP_GLINT_SPREAD = 150.0;
    mediump const float EYE_GLINT_SPREAD = 600.0;

    lowp vec3 F0 = F0_metallic.xyz;
    lowp float metallic = F0_metallic.w;

    //preliminaries
    highp vec3 H = normalize(L + V);
    lowp float NdotV = max(dot(N, V), 0.0);
    lowp float NdotL = max(dot(N, L), 0.0);
    highp float NdotH = max(dot(N, H), 0.0);

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

    lipGlossiness *= lipGlossiness > 0.0 ? pow(NdotH, LIP_GLINT_SPREAD) : 0.0;
    eyeGlint *= eyeGlint > 0.0 ? pow(NdotH, EYE_GLINT_SPREAD) : 0.0;
    result.rgb += lipGlossiness + eyeGlint;

    return result;
}

void main()
{
    mediump mat3 tangentToWorld = fs_in.TBN; //mat3(vertTangent, bitangent, vertNormal);
    vec2 uv = fs_in.texCoord;

    int matIndex = fs_in.componentIndex; //fs_in.int(round(VextexNormal.w));

    mediump vec4 albedoTex = texture(Albedo, vec3(uv,matIndex));
      
    // Normal
    highp vec3 normalDir = texture(Normal, vec3(uv,matIndex)).rgb;
    normalDir = 2.0 * normalDir - 1.0;
    highp vec3 N = normalize(tangentToWorld * normalDir);

    // View Vector     
//    vec3 V = normalize(fs_in.V);
    mediump vec3 V = normalize(camera[fs_in.view_id].position.xyz-fs_in.position.xyz);

    // Surface properties
    lowp vec3 albedoTint = mats.Data.AlbedoTint[matIndex].rgb;
    mediump float diffuseIntensity = 2.0; //*/mats.Data.Intensity[matIndex].r;

    lowp vec4 roughnessTex = texture(Metallic, vec3(uv,matIndex));
    lowp float roughness = roughnessTex.a;
      
    // Mask colors
    int colorIndex = int(clamp(round((roughnessTex.b - MASK_MIN) / MASK_DIVISOR), 0.0, MASK_COUNT-1.0));
    lowp vec3 colorMask = mats.Data.Colors[colorIndex].rgb;
    colorMask *= MASK_BRIGHTNESS_MODIFIERS[colorIndex];
    lowp float mixValue = matIndex == BODY_COMPONENT ? roughnessTex.g : 0.0;
    mixValue *= mats.Data.Colors[colorIndex].a;
    albedoTint = mix(albedoTint, colorMask, mixValue);
        
    // Mask regions
    lowp float maskBrowsScalar = colorIndex == BROWS_COLOR_INDEX ? mixValue : 0.0;
    lowp float maskLashesScalar = colorIndex == LASHES_COLOR_INDEX ? mixValue : 0.0;
    lowp float maskLipsScalar = colorIndex == LIPS_COLOR_INDEX ? mixValue : 0.0;
    lowp float maskIrisScalar = colorIndex == IRIS_COLOR_INDEX ? mixValue : 0.0;
    lowp float maskScleraScalar = colorIndex == SCLERA_COLOR_INDEX ? mixValue : 0.0;
    lowp float maskTeethScalar = colorIndex == TEETH_COLOR_INDEX ? mixValue : 0.0;
    lowp float maskGumsScalar = colorIndex == GUMS_COLOR_INDEX ? mixValue : 0.0;

    // Lerp diffuseIntensity with roughness map. This clamps the min diffuseIntensity to the
    // per-component property, and allows it to vary with the texture value up to a maximum.
    diffuseIntensity = mix(diffuseIntensity, MAX_SMOOTHNESS, roughness);
    // Brows and lashes, teeth and gums modify DiffuseIntensity
    diffuseIntensity *= (1.0 - ((maskBrowsScalar + maskLashesScalar) * BROWS_LASHES_DIFFUSEINTENSITY));
    diffuseIntensity *= (1.0 - (maskTeethScalar + maskGumsScalar));
    roughness = 1.0 - roughness;

    lowp vec3 baseColor = all(equal(albedoTex.rgba, vec4(0.0))) ? albedoTint : albedoTint * albedoTex.rgb;
    lowp vec3 albedoColor = colorIndex == LIPS_COLOR_INDEX ? mix(albedoTex.rgb, colorMask, mixValue) : baseColor;
    albedoColor = (matIndex == 2 /*|| matIndex == 3*/) ? albedoTint : albedoColor;

//    highp vec3 reflection = reflect(-viewDir, normalDir);
    //vec3 reflectionColor = texture(EnvironmentMap, reflection).rgb;
    // Smoothness multiplier on lip region
    lowp float lipGlossiness = maskLipsScalar * (mats.Data.Intensity[matIndex].a * LIP_SMOOTHNESS_MULTIPLIER);
    lowp float eyeGlint = clamp(maskIrisScalar + maskScleraScalar, 0.0, 1.0);

    lowp vec3 F0 = vec3(0.04);
    lowp float metallic = 0.0;
    F0 = mix(F0, albedoColor.xyz, metallic);
    lowp vec4 F0_metallic = vec4(F0, metallic);
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

        lowp vec3 lit = CalcLightingExpressive(albedoColor, F0_metallic, L, V, N, radiance, roughness, diffuseIntensity, lipGlossiness, eyeGlint);
        Lo += lit;
    }
    lowp vec3 color = Lo;
    color = color / (color + vec3(1.0));

    fragColor = vec4(color, 1.0); //albedoTex.a;
}
