#include "tr_shade_calc.hlsl"

#ifdef TR_WIREFRAME
typedef struct {
	float3 color;
} wireframeConstants_t;

[[vk::push_constant]] wireframeConstants_t tr_wireframe;
#endif

typedef struct {
	float4 position : SV_Position;
	float3 entPosition : POSITION;
	float3 entNormal : NORMAL;
	float2 texcoord0 : TEXCOORD0;
	float2 texcoord1 : TEXCOORD1;
	float4 color : COLOR;
} shadeVertex_t;

shadeVertex_t VS_Main( vertex_t v ) {
	stageVars_t stageVars;
	shadeVertex_t o;

	o.color = v.vertexColor;

	float4 position = float4( v.position.xyz, 1 );
	float4 normal = float4( v.normal.xyz, 0 );
	VS_ApplyModelTransform( position, normal );

	ComputeTexCoords( v, stageVars );
	o.texcoord0 = stageVars.texcoords[0];
	o.texcoord1 = stageVars.texcoords[1];

	o.entPosition = position.xyz;
	o.entNormal = normal.xyz;

	VS_ApplyViewTransform( position );
	o.position = position;

#ifdef TR_WIREFRAME
	// previous color computation should be optimized-out
	o.color = float4( tr_wireframe.color, 1 );
#endif
	return o;
}

float4 PS_Main( shadeVertex_t i )
	: SV_Target {
#ifdef TR_WIREFRAME
	return i.color;
#endif
	float4 color = ComputeColor( i.entPosition, i.entNormal, i.color, 0, 0 );
	float4 tex_0 = tr_texture_0.Sample( tr_sampler_0, i.texcoord0 );
	return color * tex_0;
}
