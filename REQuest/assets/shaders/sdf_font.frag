precision mediump float;
//layout (location = 0) out vec4 color;
out vec4 color;

layout (binding = 0) uniform sampler2D sdf_texture;
//layout (binding = 0) uniform sampler2DArray sdf_texture;

//in vec2 uv;
//in vec3 uv;
/*
layout(location = 0) in VS_OUT
{
	vec2 outUV;
} fs_in;
*/
layout(location = 0) in VS_OUT
{
    mediump vec2 texCoord;
    lowp vec4 color;
} fs_in;

const float smoothing = 1.0/6.0; //16.0; //16.0;
const float outlineDistance = 0.0; //.0;
const vec4 v_color = vec4(1.0, 1.0, 1.0, 1.0);
const vec4 outlineColor = vec4(0.0, 0.0, 1.0, 1.0);

void main(void) 
{
    float distance = texture(sdf_texture, fs_in.texCoord).x;
    float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, distance);
    color = vec4(fs_in.color.rgb, fs_in.color.a * alpha);
//    const vec4 c2 = vec4(0.1, 0.1, 0.2, 1.0);
//    const vec4 c1 = vec4(0.8, 0.9, 1.0, 1.0);
//    color = mix(c1, c2, alpha);
/*
    float distance = texture(sdf_texture, fs_in.outUV).x;
    float outlineFactor = smoothstep(0.5 - smoothing, 0.5 + smoothing, distance);
    vec4 mycolor = mix(outlineColor, v_color, outlineFactor);
    float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, distance);
    color = vec4(mycolor.rgb, mycolor.a * alpha);
*/
//    color = vec4(fs_in.color, 1.0);
}
/*
void main(void)
{
    const vec4 c2 = vec4(0.1, 0.1, 0.2, 0.0);
    const vec4 c1 = vec4(1.0, 0.0, 0.0, 1.0); //0.8, 0.9, 1.0, 1.0);
    float val = texture(sdf_texture, fs_in.outUV).x; //vec3(uv.xyz)).x;

    color = mix(c1, c2, smoothstep(0.52, 0.48, val));
}
*/