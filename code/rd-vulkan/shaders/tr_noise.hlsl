#ifndef TR_NOISE_HLSL_
#define TR_NOISE_HLSL_

#include "tr_common.hlsl"

#define TR_NOISE_SIZE 256
#define TR_NOISE_MASK ( TR_NOISE_SIZE - 1 )

float GetNoiseTime( int time ) {
	float4 noise;
	float s = time & TR_NOISE_MASK;
	float t = time / TR_NOISE_SIZE;

	noise = tr_noise.Sample( tr_linearRepeatSampler, float2( s, t ) );

	return ( 1 + dot( noise, noise ) / 4 );
}

float R_NoiseGet( float4 seed ) {
	float4 noiseXY, noiseZW;

	noiseXY = tr_noise.Sample( tr_linearRepeatSampler, seed.xy );
	noiseZW = tr_noise.Sample( tr_linearRepeatSampler, seed.zw );

	return dot( noiseXY, noiseZW ) / 4;
}

#endif // TR_NOISE_HLSL_
