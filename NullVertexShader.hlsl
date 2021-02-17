struct VertexShaderInput
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD;
};

struct VertexShaderOutput
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD;
};

VertexShaderOutput NullVertexShader(VertexShaderInput input)
{
    return input;
}
