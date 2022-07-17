#ifndef TR_COMMON_HLSL_
#define TR_COMMON_HLSL_

#define TR_HLSL 1
#include "../tr_shader.h"

// Frame globals space (0)
cbuffer trGlobals : register( b0, TR_GLOBALS_SPACE ) {
	trGlobals_t				tr;
};

cbuffer trFuncTables : register( b1, TR_GLOBALS_SPACE ) {
	trFuncTables_t			tr_funcs;
};

StructuredBuffer<mgrid_t>	tr_lightGridData : register( t2, TR_GLOBALS_SPACE );
Buffer<uint>				tr_lightGridArray : register( t3, TR_GLOBALS_SPACE );

StructuredBuffer<fog_t>		tr_fogs : register( t4, TR_GLOBALS_SPACE );

Texture2D<float4>			tr_noise : register( t5, TR_GLOBALS_SPACE );

// Static sampler space (1)
SamplerState				tr_pointClampSampler : register( s0, TR_SAMPLERS_SPACE );
SamplerState				tr_pointRepeatSampler : register( s1, TR_SAMPLERS_SPACE );
SamplerState				tr_linearClampSampler : register( s2, TR_SAMPLERS_SPACE );
SamplerState				tr_linearRepeatSampler : register( s3, TR_SAMPLERS_SPACE );

// Shader globals space (2)
cbuffer shaderStage : register( b0, TR_SHADER_SPACE ) {
	shaderStage_t			tr_shader;
};

// Model globals space (3)
cbuffer model : register( b0, TR_MODEL_SPACE ) {
	model_t					tr_model;
};

// Texture space (4)
Texture2D					tr_texture_0 : register( t0, TR_TEXTURE_SPACE_0 );
SamplerState				tr_sampler_0 : register( s1, TR_TEXTURE_SPACE_0 );

// Texture space (5)
Texture2D					tr_texture_1 : register( t0, TR_TEXTURE_SPACE_1 );
SamplerState				tr_sampler_1 : register( s1, TR_TEXTURE_SPACE_1 );

// Texture space (6)
Texture2D					tr_texture_2 : register( t0, TR_TEXTURE_SPACE_2 );
SamplerState				tr_sampler_2 : register( s1, TR_TEXTURE_SPACE_2 );

// Texture space (7)
Texture2D					tr_texture_3 : register( t0, TR_TEXTURE_SPACE_3 );
SamplerState				tr_sampler_3 : register( s1, TR_TEXTURE_SPACE_3 );


// Math constants
#define TR_M_PI			3.14159265359
#define TR_M_HALF_PI	1.57079632679
#define TR_M_2PI		6.28318530718


bool Q_isnan(float v) {
	return !( v > 0 || v < 0 || v == 0 );
}

bool2 Q_isnan( float2 v ) {
	return !( v > 0 || v < 0 || v == 0 );
}

bool3 Q_isnan( float3 v ) {
	return !( v > 0 || v < 0 || v == 0 );
}

bool4 Q_isnan( float4 v ) {
	return !( v > 0 || v < 0 || v == 0 );
}


void VS_FullScreenQuad( uint vindex, out float4 position, out float2 texcoord ) {
	static const float2 vertices[3] = {
		float2( -1, 3 ),
		float2( -1, -1 ),
		float2( 3, -1 )
	};
	float2 v = vertices[vindex];
	position = float4( v, 0, 1 );
	texcoord = ( v + 1 ) / 4;
}

float3 ProjectPointOnPlane( float3 p, float3 normal ) {
	float d;
	float3 n;
	float inv_denom;

	inv_denom = dot( normal, normal );
	inv_denom = 1.0f / inv_denom;

	d = dot( normal, p ) * inv_denom;
	n = normal * inv_denom;

	return p - d * n;
}

/*
** assumes "src" is normalized
*/
float3 PerpendicularVector( float3 src ) {
	int pos;
	int i;
	float minelem = 1.0F;
	float3 tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for( pos = 0, i = 0; i < 3; i++ ) {
		if( abs( src[i] ) < minelem ) {
			pos = i;
			minelem = abs( src[i] );
		}
	}
	tempvec = 0;
	tempvec[pos] = 1;

	/*
	** project the point onto the plane defined by src
	*/
	return normalize( ProjectPointOnPlane( tempvec, src ) );
}

#endif // TR_COMMON_HLSL_
