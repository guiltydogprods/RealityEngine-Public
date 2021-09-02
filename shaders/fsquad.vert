
out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out VS_OUT
{
	vec2 outUV;
} vs_out;

const vec2 quadVertices[4] = { vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0) };

void main() 
{
    vec2 outUV = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vs_out.outUV = outUV * vec2(1.0, -1.0);
    gl_Position = vec4(outUV * 2.0f + -1.0f, 0.0f, 1.0f);
//    gl_Position = vec4(quadVertices[gl_VertexID], 0.0, 1.0);
}
