out vec4 outColor;

layout(location = 0) in VS_OUT
{
	vec3 N;
	vec3 position;
	vec3 V;
	vec2 texCoord;
	flat int view_id;
	flat int material_id;
} fs_in;


struct MaterialProperties
{
	vec4 diffuse;
	vec4 specular;
};

#define NUM_VIEWS 2
layout(binding = 1, std140) uniform TRANSFORM_BLOCK
{
	mat4    view_matrix[NUM_VIEWS];
	mat4    proj_matrix[NUM_VIEWS];
	vec4	cam_pos[NUM_VIEWS];
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
	vec4 Kd = texture(emissiveSampler, fs_in.texCoord.xy);

	outColor = vec4(Kd.xyz, 1.0);
}
