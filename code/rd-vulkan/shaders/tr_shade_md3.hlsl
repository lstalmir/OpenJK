
#define VS_Main VS_ShadeMain
#include "tr_shade.hlsl"
#undef VS_Main

/*
** LerpMeshVertexes
*/
static vertex_t LerpMeshVertexes( vertex_t v0, vertex_t v1, float backlerp ) {
	vertex_t v = v0;
	if ( backlerp == 0 )
		return v;
	v.position.xyz = lerp( v0.position.xyz, v1.position.xyz, backlerp );
	v.normal.xyz = lerp( v0.normal.xyz, v1.normal.xyz, backlerp );
	return v;
}

shadeVertex_t VS_Main( vertex_t v0, oldVertex_t v1 ) {
	vertex_t v;

	v = LerpMeshVertexes( v0, v1, tr_model.e.backlerp );

	return VS_ShadeMain( v );
}
