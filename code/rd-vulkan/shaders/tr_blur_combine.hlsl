
#include "tr_common.hlsl"

typedef
struct blurCombineVertex_s
{
	float4 position : SV_Position;
	float2 tex : TEXCOORD0;
} blurCombineVertex_t;

Texture2D tr_blurImage : register( t0, TR_TEXTURE_SPACE_0 );


blurCombineVertex_t VS_Main(uint i : SV_VertexID)
{
	// construct a full-screen quad
	blurCombineVertex_t o;
	VS_FullScreenQuad(i, o.position, o.tex);
	return o;
}

float4 PS_Main(blurCombineVertex_t i)
	: SV_Target
{
	return tr_blurImage.Sample(tr_linearClampSampler, i.tex);
}
