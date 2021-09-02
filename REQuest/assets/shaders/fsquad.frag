precision mediump float;
out vec4 outColor;

layout(location = 0) in VS_OUT
{
    vec2 outUV;
} fs_in;

layout(binding=0) uniform sampler2D tex;

void main()
{
    vec4 tex = texture(tex, fs_in.outUV.xy); // * vec4(materials[matId].diffuse.xyz, 1.0);

	outColor = tex;
}