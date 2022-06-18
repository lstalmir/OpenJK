#include "tr_common.hlsl"

#ifdef BLUR_COMBINE_PASS
typedef struct blurParms_s {
	float2 texelOffset0;
	float2 texelOffset1;
	float2 texelOffset2;
	float2 texelOffset3;
	float4 weight;
} blurParms_t;

typedef struct blurInput_s {
	uint vindex : SV_VertexID;
} blurInput_t;

typedef struct blurVertex_s {
	float4 position : SV_Position;
	float2 tex0 : TEXCOORD0;
	float2 tex1 : TEXCOORD1;
	float2 tex2 : TEXCOORD2;
	float2 tex3 : TEXCOORD3;
} blurVertex_t;

[[vk::push_constant]] cbuffer blurParms : register( b0, space1 ) {
	blurParms_t blurParms;
};

Texture2D blurImage : register( t1, space1 );

blurVertex_t VS_Main( blurInput_t i ) {
	blurVertex_t o;
	float2 baseTex;

	// construct a full-screen quad
	VS_FullScreenQuad( i.vindex, o.position, baseTex );

	o.tex0 = baseTex + blurParms.texelOffset0;
	o.tex1 = baseTex + blurParms.texelOffset1;
	o.tex2 = baseTex + blurParms.texelOffset2;
	o.tex3 = baseTex + blurParms.texelOffset3;
	return o;
}

float4 PS_Main( blurVertex_t i )
	: SV_Target {
	float4 result = 0;
	result += blurParms.weight * blurImage.Sample( linearClampSampler, i.tex0 );
	result += blurParms.weight * blurImage.Sample( linearClampSampler, i.tex1 );
	result += blurParms.weight * blurImage.Sample( linearClampSampler, i.tex2 );
	result += blurParms.weight * blurImage.Sample( linearClampSampler, i.tex3 );
	return result;
}
#endif
