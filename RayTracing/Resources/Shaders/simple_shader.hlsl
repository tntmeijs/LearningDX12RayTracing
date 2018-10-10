struct VSOutput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

VSOutput vs_main(float4 position : POSITION, float4 color : COLOR)
{
	VSOutput result;

	result.position = position;
	result.color = color;

	return result;
}

float4 ps_main(VSOutput input) : SV_TARGET
{
	return input.color;
}