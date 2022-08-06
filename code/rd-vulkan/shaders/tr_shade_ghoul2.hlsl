#define VS_Main VS_ShadeMain
#include "tr_shade.hlsl"
#undef VS_Main

StructuredBuffer<mdxaBone_t> tr_bones : register( t0, TR_G2_BONES_SPACE );

shadeVertex_t VS_Main( vertex_t v ) {
	int i;
	uint4 boneIndices;
	float4 boneWeights;
	float3 position = 0;
	float3 normal = 0;

	// apply bone transformation before proceeding with the default vertex shader
	boneIndices = asuint( float4( v.texCoord1, v.texCoord2 ) );
	boneWeights = float4( v.texCoord3, v.texCoord4 );

	for( i = 0; i < 4; ++i ) {
		uint boneIndex = boneIndices[i];
		float boneWeight = boneWeights[i];

		if( boneWeight > 0.0001 ) {
			float4x3 bone = tr_bones[boneIndex].mat;

			position.x += boneWeight * ( dot( bone._11_21_31, v.position.xyz ) + bone._41 );
			position.y += boneWeight * ( dot( bone._12_22_32, v.position.xyz ) + bone._42 );
			position.z += boneWeight * ( dot( bone._13_23_33, v.position.xyz ) + bone._43 );

			normal.x += boneWeight * ( dot( bone._11_21_31, v.normal.xyz ) + bone._41 );
			normal.y += boneWeight * ( dot( bone._12_22_32, v.normal.xyz ) + bone._42 );
			normal.z += boneWeight * ( dot( bone._13_23_33, v.normal.xyz ) + bone._43 );
		}
	}

	v.position.xyz = position;
	v.normal.xyz = normal;

	return VS_ShadeMain( v );
}
