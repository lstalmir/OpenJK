#include "tr_common.hlsl"

typedef struct shadowInput_s {
	uint vindex : SV_VertexID;
} shadowInput_t;

typedef struct shadowVertex_s {
	float4 position : SV_Position;
	float2 texcoord : TEXCOORD0;
} shadowVertex_t;

Texture2D				tr_sceneColor : register( t0, space4 );
Texture2D				tr_scenePosition : register( t1, space4 );
Texture2D				tr_sceneNormal : register( t2, space4 );
Texture2D				tr_sceneDepth : register( t3, space4 );
Texture2DArray			tr_sunLightShadows : register( t4, space4 );
Texture2DArray			tr_directionalLightShadows : register( t5, space4 );
TextureCubeArray		tr_pointLightShadows : register( t6, space4 );

SamplerComparisonState	tr_shadowSampler : register( s7, space4 );

#if 0
float3 RB_GetShadowTexcoord( float4 worldPos, float4x4 vpShadowMatrix ) {
	float2 texcoord;
	float4 shadowPos;

	shadowPos = mul( vpShadowMatrix, worldPos );
	shadowPos /= shadowPos.w;

	texcoord.x = shadowPos.x / 2 + 0.5;
	texcoord.y = -shadowPos.y / 2 + 0.5;

	return float3( texcoord, shadowPos.z );
}

float RB_SampleShadow( Texture2DArray shadowMap, uint index, float3 texcoord, float2 poisson, float bias ) {
	if( ( ( texcoord.z - bias ) < 1.0 ) &&
		( ( texcoord.z - bias ) > 0.0 ) ) {
		return 1 - shadowMap.SampleCmpLevelZero(
						tr_shadowSampler, index,
						texcoord.xy + poisson / 1500,
						texcoord.z - bias );
	}
	return 0;
}

float RB_GetShadow( float4 worldPos ) {
	int i, j;
	float3 static_shadow_texcoord = RB_GetShadowTexcoord( worldPos, c_world.vpStaticShadowMatrix );
	float3 dynamic_shadow_texcoord = RB_GetShadowTexcoord( worldPos, c_world.vpDynamicShadowMatrix );

	// Multisample for smoother shadows
	const float2 poissonDisk[4] = {
		{ -0.94201624, -0.39906216 },
		{ 0.94558609, -0.76890725 },
		{ -0.094184101, -0.92938870 },
		{ 0.34495938, 0.29387760 }
	};

	const float static_bias = 0.00035;
	const float dynamic_bias = 0.000035;

	// Don't make deep shadows
	float total_shadow = 1;

	// Apply directional light shadows.
	for( i = 0; i < tr.numDirectionalLights; ++i ) {
		for( int i = 0; i < 4; ++i ) {
			total_shadow += 1 - saturate(
									RB_SampleShadow( t_static_shadow_map, static_shadow_texcoord, poissonDisk[i], static_bias ) +
									RB_SampleShadow( t_dynamic_shadow_map, dynamic_shadow_texcoord, poissonDisk[i], dynamic_bias ) );
		}
	}

	return ( 0.9 + total_shadow ) / 4;
}
#endif

shadowVertex_t VS_Main( shadowInput_t i ) {
	shadowVertex_t o;

	// construct a full-screen quad
	VS_FullScreenQuad( i.vindex, o.position, o.texcoord );

	return o;
}

float4 PS_Main( shadowVertex_t i )
	: SV_Target0 {
	return tr_sceneColor.Load( uint3( i.position.xy, 0 ) );
}
