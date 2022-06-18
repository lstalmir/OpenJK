
SamplerState pointClampSampler : register( s1, space0 );
SamplerState pointRepeatSampler : register( s2, space0 );
SamplerState linearClampSampler : register( s3, space0 );
SamplerState linearRepeatSampler : register( s4, space0 );


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
