#include "tr_common.hlsl"

typedef struct sunVertexInput_s {
	uint vindex : SV_VertexID;
} sunVertexInput_t;

typedef struct sunVertex_s {
	float4 position : SV_Position;
	float2 tex0 : TEXCOORD0;
} sunVertex_t;

sunVertex_t VS_Main( sunVertexInput_t i ) {
	sunVertex_t o;
	float4 vert;
	float3 origin, dir;
	float3 vec1, vec2;
	float dist, size;
	uint vindex;

	const uint vindices[6] = { 0, 1, 2, 0, 2, 3 };
	const float4 vertexes[4] = {
		float4( -1, -1, 0, 0 ), // x,y factors and u,v texcoords
		float4( 1, -1, 0, 1 ),
		float4( 1, 1, 1, 1 ),
		float4( -1, 1, 1, 0 )
	};

	dist = tr_view.zFar / 1.75; // div sqrt(3)
	size = dist * 0.4;

	dir = tr.sunParms.sunDirection.xyz;

	origin = dir * dist;
	vec1 = PerpendicularVector( dir );
	vec2 = cross( dir, vec1 );

	vec1 *= size;
	vec2 *= size;

	vindex = vindices[i.vindex];
	vert = vertexes[vindex];

	o.position = float4( origin + vert.x * vec1 + vert.y * vec2, 1 );
	o.tex0 = vert.zw;

	// transform the vertex
	o.position.xyz += tr_view.ori.origin;
	o.position = mul( tr_view.world.modelMatrix, o.position );
	//todo: viewMatrix
	o.position = mul( tr_view.projectionMatrix, o.position );

	return o;
}

float4 PS_Main( sunVertex_t i )
	: SV_Target {
	float4 result = 0;


	return result;
}
