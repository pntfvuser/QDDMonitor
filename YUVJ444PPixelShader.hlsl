struct PixelShaderInput
{
	float4 pos         : SV_POSITION;
	float2 texCoord    : TEXCOORD0;
};

Texture2D<float>  luminanceChannel    : t0;
Texture2D<float>  chrominanceUChannel : t1;
Texture2D<float>  chrominanceVChannel : t2;
SamplerState      defaultSampler      : s0;

static const float3x3 YUVtoRGBCoeffMatrix =
{
	1.022831f,  1.022831f, 1.022831f,
	0.000000f, -0.391762f, 2.017232f,
	1.596027f, -0.812968f, 0.000000f
};

float3 ConvertJYUVtoRGB(float3 yuv)
{
	yuv -= float3(0.000000f, 0.501960f, 0.501960f);
	yuv = mul(yuv, YUVtoRGBCoeffMatrix);

	return saturate(yuv);
}

float4 YUVJ444PPixelShader(PixelShaderInput input) : SV_TARGET
{
	float y = luminanceChannel.Sample(defaultSampler, input.texCoord);
	float u = chrominanceUChannel.Sample(defaultSampler, input.texCoord);
	float v = chrominanceVChannel.Sample(defaultSampler, input.texCoord);

	return float4(ConvertJYUVtoRGB(float3(y, u, v)), 1.f);
}
