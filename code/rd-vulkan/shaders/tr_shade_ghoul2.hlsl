#define VS_Main VS_ShadeMain
#include "tr_shade.hlsl"
#undef VS_Main

StructuredBuffer<mdxaBone_t> tr_bones : register( t0, TR_G2_BONES_SPACE );

shadeVertex_t VS_Main( vertex_t v ) {
	int i;
	uint4 boneIndices;
	float4 boneWeights;
	float3 position = v.position.xyz;
	float3 normal = v.normal.xyz;

	// apply bone transformation before proceeding with the default vertex shader
	boneIndices = asuint( float4( v.texCoord1, v.texCoord2 ) );
	boneWeights = float4( v.texCoord3, v.texCoord4 );

	for( i = 0; i < 4; ++i ) {
		uint boneIndex = boneIndices[i];
		float boneWeight = boneWeights[i];

		if( boneWeight > 0.0001 ) {
			float4x3 bone =tr_bones[boneIndex].mat;
			position += mul( bone, v.position.xyz ).xyz * boneWeight;
			normal += mul( bone, v.normal.xyz ).xyz * boneWeight;
		}
	}

	v.position.xyz = position;
	v.normal.xyz = normal;

	return VS_ShadeMain( v );
}
