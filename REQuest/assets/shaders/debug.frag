precision mediump float;
out vec4 outColor;

layout(location = 0) in VS_OUT
{
    lowp vec3 color;
} fs_in;

void main()
{
	outColor = vec4(fs_in.color, 1.0);
}
