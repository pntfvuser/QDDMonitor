struct PixelShaderInput
{
	float4 pos         : SV_POSITION;
	float2 texCoord    : TEXCOORD0;
};

Texture2D<float4> rgbxChannel        : t0;
SamplerState      defaultSampler     : s0;

float4 RGBXPixelShader(PixelShaderInput input) : SV_TARGET
{
	float4 rgbx = rgbxChannel.Sample(defaultSampler, input.texCoord);
	return float4(rgbx.rgb, 1.f);
}
