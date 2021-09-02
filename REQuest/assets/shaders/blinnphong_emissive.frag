precision mediump float;
out lowp vec4 outColor;

layout (location = 0) in VS_OUT
{
	highp vec3 position;
	mediump vec3 N;
	mediump vec3 V;
	lowp vec2 texCoord;
	lowp flat int material_id;
} fs_in;


struct MaterialProperties
{
	lowp vec4 diffuse;
	lowp vec4 specular;
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

uniform sampler2D emissiveSampler;

void main()
{
	lowp vec4 Kd = texture(emissiveSampler, fs_in.texCoord.xy);

	outColor = vec4(Kd.xyz, 1.0);
}
