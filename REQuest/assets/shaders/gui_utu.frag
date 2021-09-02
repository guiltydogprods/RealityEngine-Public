precision lowp float;
precision lowp sampler2DArray;
out vec4 color;

layout(location = 0) in VS_OUT
{
    mediump vec2 texCoord;
    lowp vec4 color;
    lowp flat uint texIdx;
} fs_in;

layout (binding = 0) uniform sampler2DArray guiTexture;

const uint kTexIdxMask = 0x7fu;
const uint kDisabledBitMask = 0x80u;
const vec3 kLum = vec3(0.21, 0.71, 0.07);

void main(void) 
{
    uint texIdx = fs_in.texIdx & kTexIdxMask;
    bool disabled = (fs_in.texIdx & kDisabledBitMask) > 0u;
    vec4 tex = texture(guiTexture, vec3(fs_in.texCoord.xy, texIdx));
    vec4 greyed = vec4(vec3(dot(tex.rgb, kLum.rgb)), tex.a);
    color = mix(tex, greyed, disabled) * fs_in.color; 
}
