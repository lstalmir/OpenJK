#include "tr_common.hlsl"

TR_ENUM( Dissolve_e,
	eDISSOLVE_RT_TO_LT = 0,
	eDISSOLVE_LT_TO_RT = 1,
	eDISSOLVE_TP_TO_BT = 2,
	eDISSOLVE_BT_TO_TP = 3,
	eDISSOLVE_CIRCULAR_OUT = 4,
	eDISSOLVE_RAND_LIMIT = 5,
	eDISSOLVE_CIRCULAR_IN = 6,
	eDISSOLVE_NUMBEROF = 7 );

typedef struct {
	Dissolve_e		type;
	int				width;
	int				height;
	float			percentage;
} dissolveParms_t;

typedef struct vertInput_s {
	uint			vindex : SV_VertexID;
} vertInput_t;

typedef struct vertOutput_s {
	float4			position : SV_Position;
	float2			tex : TEXCOORD;
} vertOutput_t;


// Dissolve descriptor space (4)
cbuffer dissolveParms : register( b0, TR_CUSTOM_SPACE_0 ) {
	dissolveParms_t	tr_dissolve;
};

Texture2D			tr_dissolveImage : register( t1, TR_CUSTOM_SPACE_0 );
Texture2D			tr_oldImage : register( t2, TR_CUSTOM_SPACE_0 );


vertOutput_t VS_Main( vertInput_t i ) {
	vertOutput_t o;

	// construct a full-screen quad
	VS_FullScreenQuad( i.vindex, o.position, o.tex );

	return o;
}

float4 PS_Main( vertOutput_t i )
	: SV_Target {
	uint2 alphaTexSize;
	float2 alphaTex;
	float alpha;
	float3 color;

	tr_dissolveImage.GetDimensions( alphaTexSize.x, alphaTexSize.y );

	// compute the alpha texture coords
	switch( tr_dissolve.type ) {
	case eDISSOLVE_RT_TO_LT: {
		float fXboundary = tr_dissolve.width - ( ( tr_dissolve.width + alphaTexSize.x ) * tr_dissolve.percentage );
		alphaTex.x = saturate( ( fXboundary - i.position.x ) / alphaTexSize.x );
		alphaTex.y = i.tex.y;
	} break;

	case eDISSOLVE_LT_TO_RT: {
		float fXboundary = ( ( tr_dissolve.width + ( 2 * alphaTexSize.x ) ) * tr_dissolve.percentage ) - tr_dissolve.width;
		alphaTex.x = saturate( ( fXboundary - i.position.x ) / alphaTexSize.x );
		alphaTex.y = i.tex.y;
	} break;

	case eDISSOLVE_TP_TO_BT: {
		float fYboundary = ( ( tr_dissolve.height + ( 2 * alphaTexSize.x ) ) * tr_dissolve.percentage ) - alphaTexSize.x;
		alphaTex.x = saturate( ( fYboundary - i.position.y ) / alphaTexSize.x );
		alphaTex.y = i.tex.x;
	} break;

	case eDISSOLVE_BT_TO_TP: {
		float fYboundary = tr_dissolve.height - ( ( tr_dissolve.height + alphaTexSize.x ) * tr_dissolve.percentage );
		alphaTex.x = saturate( ( fYboundary - i.position.y ) / alphaTexSize.x );
		alphaTex.y = i.tex.x;
	} break;

#if 0
	case eDISSOLVE_CIRCULAR_IN: {
		float fDiagZoom = tr_dissolve.width * 0.8 * ( 1 - tr_dissolve.percentage );

		//
		// blit circular graphic...
		//
		x0 = fXScaleFactor * ( ( Dissolve.iWidth / 2 ) - fDiagZoom );
		y0 = fYScaleFactor * ( ( tr_dissolve.height / 2 ) - fDiagZoom );
		x1 = fXScaleFactor * ( ( Dissolve.iWidth / 2 ) + fDiagZoom );
		y1 = y0;
		x2 = x1;
		y2 = fYScaleFactor * ( ( tr_dissolve.height / 2 ) + fDiagZoom );
		x3 = x0;
		y3 = y2;

	} break;

	case eDISSOLVE_CIRCULAR_OUT: {
		float fDiagZoom = tr_dissolve.width * 0.8 * tr_dissolve.percentage;

		//
		// blit circular graphic...
		//
		x0 = fXScaleFactor * ( ( Dissolve.iWidth / 2 ) - fDiagZoom );
		y0 = fYScaleFactor * ( ( tr_dissolve.height / 2 ) - fDiagZoom );
		x1 = fXScaleFactor * ( ( Dissolve.iWidth / 2 ) + fDiagZoom );
		y1 = y0;
		x2 = x1;
		y2 = fYScaleFactor * ( ( tr_dissolve.height / 2 ) + fDiagZoom );
		x3 = x0;
		y3 = y2;

	} break;
#endif

	default: {
		discard;
	}
	}

	// read the alpha
	alpha = tr_dissolveImage.Sample( tr_linearClampSampler, alphaTex ).r;
	if( alpha == 0 )
		discard;

	// sample the old image
	color = tr_oldImage.Sample( tr_linearClampSampler, i.tex ).rgb;

	// blend the second image with the current color attachment
	return float4( color, alpha );
}
