
#ifndef TR_NOISE_HLSL_
#define TR_NOISE_HLSL_

#include "tr_common.hlsl"

#define TR_NOISE_SIZE 256
#define TR_NOISE_MASK ( TR_NOISE_SIZE - 1 )

float GetNoiseTime( int time ) {
	float4 noise;
	float s = time & TR_NOISE_MASK;
	float t = time / TR_NOISE_SIZE;

	noise = tr_noise.SampleLevel( tr_linearRepeatSampler, float2( s, t ), 0 );

	return ( 1 + dot( noise, noise ) / 4 );
}

float R_NoiseGet( float4 seed ) {
	float4 noiseXY, noiseZW;

	noiseXY = tr_noise.SampleLevel( tr_linearRepeatSampler, seed.xy, 0 );
	noiseZW = tr_noise.SampleLevel( tr_linearRepeatSampler, seed.zw, 0 );

	return dot( noiseXY, noiseZW ) / 4;
}

#endif // TR_NOISE_HLSL_
