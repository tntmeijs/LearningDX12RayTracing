Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

cbuffer SceneConstantBuffer : register(b0)
{
	float4x4 view_projection_matrix;
};

cbuffer ModelConstantBuffer : register(b1)
{
	float4x4 model_matrix;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

VSOutput vs_main(float4 position : POSITION, float2 uv : TEXCOORD)
{
	VSOutput result;

	float4x4 mvp = mul(model_matrix, view_projection_matrix);

	result.position = mul(mvp, position);
	result.uv = uv;

	return result;
}

float4 ps_main(VSOutput input) : SV_TARGET
{
	return g_texture.Sample(g_sampler, input.uv);
}