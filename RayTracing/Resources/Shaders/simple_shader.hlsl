Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

cbuffer SceneConstantBuffer : register(b0)
{
	float2 positionOffset;
};

cbuffer ModelConstantBuffer : register(b1)
{
	float2 test;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

VSOutput vs_main(float4 position : POSITION, float2 uv : TEXCOORD)
{
	VSOutput result;

	result.position = position + float4(positionOffset.x, positionOffset.y, 0.0, 0.0);
	result.uv = uv;

	return result;
}

float4 ps_main(VSOutput input) : SV_TARGET
{
	float4 textureColor = g_texture.Sample(g_sampler, input.uv);
	return textureColor + float4(0.0, test.x, test.y, 0.0);
}