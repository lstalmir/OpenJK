
#define PS_Main PS_SkyboxMain
#include "tr_skybox.hlsl"
#undef PS_Main

float4 PS_Main(skyboxVertex_t i)
	: SV_Target
{
	// get fragment depth
	float depth = tr_texture_1.Load(int3(i.position.xy, 0)).x;
	if (!depth)
		discard; // :( - TODO: enable depth test
	
	float atmosphere;
	atmosphere = saturate(depth * tr.skyParms.atmosphereDistance);
	atmosphere = 1 - atmosphere; // 0 is at infinity
	// apply atmosphere falloff factor
	atmosphere = pow(atmosphere, tr.skyParms.atmosphereFalloff);
	// apply atmosphere density
	atmosphere = atmosphere * tr.skyParms.atmosphereDensity;

	// get atmosphere color at this location
	return float4(tr_texture_0.Sample(tr_skyFogColorSampler, i.texCoord).rgb, atmosphere);
}
