
#include "tr_common.hlsl"

namespace fxaa
{
#define FXAA_PC 1
#define FXAA_HLSL_5 1
#define FXAA_QUALITY__PRESET 12
#include "fxaa3_11.hlsl"
}

struct fxaaVertex_t
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD0;
};

fxaaVertex_t VS_Main(uint vindex : SV_VertexID)
{
	fxaaVertex_t v;
	VS_FullScreenQuad(vindex, v.position, v.texCoord);
	return v;
}

float4 PS_Main(fxaaVertex_t input) : SV_Target
{
	uint dx, dy;
	tr_texture_0.GetDimensions(dx, dy);
	float2 rcpro = rcp(float2(dx, dy));

	fxaa::FxaaTex t;
	t.smpl = tr_linearClampSampler;
	t.tex = tr_texture_0;

	return fxaa::FxaaPixelShader(input.texCoord, 0, t, t, t, rcpro, 0, 0, 0, 1.0, 0.166, 0.0312, 0, 0, 0, 0);
}
