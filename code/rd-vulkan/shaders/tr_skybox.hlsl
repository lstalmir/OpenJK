
#define TR_TEXTURE0_T TextureCube
#include "tr_shade_calc.hlsl"

struct skyboxVertex_t
{
	float4 position : SV_Position;
	float3 texCoord : DIRECTION;
};

skyboxVertex_t VS_Main(vertex_t v0)
{
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	static const float4x4 s_flipMatrix = float4x4(
		float4(0, 1, 0, 0),
		float4(0, 0, 1, 0),
		float4(1, 0, 0, 0),
		float4(0, 0, 0, 1));
	
	skyboxVertex_t o;
	
	float4x4 viewMatrix = tr_view.world.modelMatrix;
	viewMatrix[0][3] = 0;
	viewMatrix[1][3] = 0;
	viewMatrix[2][3] = 0;
	
	float4 position = v0.position;
	position = mul(viewMatrix, position);
	position = mul(tr_view.projectionMatrix, position);
	o.position = position;
	o.position.z = 0;
	o.texCoord = mul(s_flipMatrix, v0.position).xyz;

	return o;
}

float4 PS_Main(skyboxVertex_t i)
	: SV_Target
{
	return tr_texture_0.Sample(tr_sampler_0, i.texCoord);
}
