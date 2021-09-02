out vec4 color;

layout(location = 0) in VS_OUT
{
    vec4 color;
} fs_in;

void main(void) 
{
    color = fs_in.color;
}
