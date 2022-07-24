#include "tr_common.hlsl"
#include "tr_noise.hlsl"

#ifdef TR_WIREFRAME
typedef struct {
	float3 color;
} wireframeConstants_t;

[[vk::push_constant]]
wireframeConstants_t tr_wireframe;
#endif

#define TR_WAVE_VALUE( func, base, phase, freq, amplitude ) \
	( ( base ) + func( (uint)( ( ( phase ) + tr.floatTime * ( freq ) ) * TR_FUNCTABLE_SIZE ) ) * ( amplitude ) )

#define TR_WAVEFORM_VALUE( table, wf ) \
	TR_WAVE_VALUE( table, wf.base, wf.phase, wf.frequency, wf.amplitude )

#define TR_WAVE_VALUE_FOR_FUNC( func, base, phase, freq, amplitude, out )                   \
	switch( func ) {                                                                        \
	case GF_SIN:                                                                            \
		out = TR_WAVE_VALUE( R_sinFunc, base, phase, freq, amplitude );             \
		break;                                                                              \
	case GF_TRIANGLE:                                                                       \
		out = TR_WAVE_VALUE( R_triangleFunc, base, phase, freq, amplitude );        \
		break;                                                                              \
	case GF_SQUARE:                                                                         \
		out = TR_WAVE_VALUE( R_squareFunc, base, phase, freq, amplitude );          \
		break;                                                                              \
	case GF_SAWTOOTH:                                                                       \
		out = TR_WAVE_VALUE( R_sawToothFunc, base, phase, freq, amplitude );        \
		break;                                                                              \
	case GF_INVERSE_SAWTOOTH:                                                               \
		out = TR_WAVE_VALUE( R_inverseSawToothFunc, base, phase, freq, amplitude ); \
		break;                                                                              \
	case GF_NONE:                                                                           \
	default:                                                                                \
		out = 0;                                                                            \
	}

#define TR_WAVEFORM_VALUE_FOR_FUNC( func, wf, out ) \
	TR_WAVE_VALUE_FOR_FUNC( func, wf.base, wf.phase, wf.frequency, wf.amplitude, out )


/*
** EvalWaveForm
**
** Evaluates a given waveForm_t, referencing backEnd.refdef.time directly
*/
static float EvalWaveForm( waveForm_t wf ) {
	float value;

	if( wf.func == GF_NOISE ) {
		return ( wf.base + R_NoiseGet( float4( 0, 0, 0, ( tr.floatTime + wf.phase ) * wf.frequency ) ) * wf.amplitude );
	}
	else if( wf.func == GF_RAND ) {
		if( GetNoiseTime( tr.floatTime + wf.phase ) <= wf.frequency ) {
			return ( wf.base + wf.amplitude );
		}
		else {
			return wf.base;
		}
	}

	TR_WAVEFORM_VALUE_FOR_FUNC( wf.func, wf, value );

	return value;
}

static float EvalWaveFormClamped( waveForm_t wf ) {
	return saturate( EvalWaveForm( wf ) );
}

/*
====================================================================

DEFORMATIONS

====================================================================
*/

/*
========================
RB_CalcDeformVertexes

========================
*/
float3 RB_CalcDeformVertex( float3 position, float3 normal, deformStage_t ds ) {
	float3 offset;
	float scale;

	if( ds.deformationWave.frequency == 0 ) {
		scale = EvalWaveForm( ds.deformationWave );
		offset = normal * scale;
		position += offset;
	}
	else {
		float off = ( position.x + position.y + position.z ) * ds.deformationSpread;

		TR_WAVE_VALUE_FOR_FUNC(
			ds.deformationWave.func,
			ds.deformationWave.base,
			ds.deformationWave.amplitude,
			ds.deformationWave.phase + off,
			ds.deformationWave.frequency,
			scale );

		offset = normal * scale;
		position += offset;
	}

	return position;
}

/*
=========================
RB_CalcDeformNormals

Wiggle the normals for wavy environment mapping
=========================
*/
float3 RB_CalcDeformNormal( float3 position, float3 normal, deformStage_t ds ) {
	float scale;
	float3 steps[] = {
		float3( 0, 0, 0 ),
		float3( 100, 0, 0 ),
		float3( 200, 0, 0 )
	};

	for( int i = 0; i < 3; ++i ) {
		scale = R_NoiseGet( float4( position * 0.98 + steps[i], tr.floatTime * ds.deformationWave.frequency ) );
		normal[i] += ds.deformationWave.amplitude * scale;
	}

	return normalize( normal );
}

/*
========================
RB_CalcBulgeVertexes

========================
*/
void RB_CalcBulgeVertex( float3 position, float3 normal, float2 st, deformStage_t ds ) {
	int i;
	float scale;

	if( ds.bulgeSpeed == 0.0f && ds.bulgeWidth == 0.0f ) {
		// We don't have a speed and width, so just use height to expand uniformly
		position += normal * ds.bulgeHeight;
	}
	else {
		// I guess do some extra dumb stuff..the fact that it uses ST seems bad though because skin pages may be set up in certain ways that can cause
		//	very noticeable seams on sufaces ( like on the huge ion_cannon ).
		float now;
		int off;

		now = tr.floatTime * ds.bulgeSpeed * 0.001f;
		off = (float)( TR_FUNCTABLE_SIZE / ( TR_M_PI * 2 ) ) * ( st.x * ds.bulgeWidth + now );

		scale = R_sinFunc( off ) * ds.bulgeHeight;

		position += normal * scale;
	}
}


/*
======================
RB_CalcMoveVertexes

A deformation that can move an entire surface along a wave path
======================
*/
float3 RB_CalcMoveVertex( float3 position, deformStage_t ds ) {
	float scale;
	float3 offset;

	TR_WAVE_VALUE_FOR_FUNC(
		ds.deformationWave.func,
		ds.deformationWave.base,
		ds.deformationWave.amplitude,
		ds.deformationWave.phase,
		ds.deformationWave.frequency,
		scale );

	offset = ds.moveVector * scale;
	position += offset;

	return position;
}


#if 0
/*
=============
DeformText

Change a polygon into a bunch of text polygons
=============
*/
void DeformText( const char *text ) {
	int		i;
	float3	origin, width, height;
	int		len;
	int		ch;
	byte	color[4];
	float	bottom, top;
	vec3_t	mid;

	height[0] = 0;
	height[1] = 0;
	height[2] = -1;
	width = cross( tess.normal[0], height );

	// find the midpoint of the box
	VectorClear( mid );
	bottom = WORLD_SIZE;	//999999;	// WORLD_SIZE instead of MAX_WORLD_COORD so guaranteed to be...
	top = -WORLD_SIZE;		//-999999;	// ... outside the legal range.
	for ( i = 0 ; i < 4 ; i++ ) {
		VectorAdd( tess.xyz[i], mid, mid );
		if ( tess.xyz[i][2] < bottom ) {
			bottom = tess.xyz[i][2];
		}
		if ( tess.xyz[i][2] > top ) {
			top = tess.xyz[i][2];
		}
	}
	VectorScale( mid, 0.25f, origin );

	// determine the individual character size
	height[0] = 0;
	height[1] = 0;
	height[2] = ( top - bottom ) * 0.5f;

	VectorScale( width, height[2] * -0.75f, width );

	// determine the starting position
	len = strlen( text );
	VectorMA( origin, (len-1), width, origin );

	// clear the shader indexes
	tess.numIndexes = 0;
	tess.numVertexes = 0;

	color[0] = color[1] = color[2] = color[3] = 255;

	// draw each character
	for ( i = 0 ; i < len ; i++ ) {
		ch = text[i];
		ch &= 255;

		if ( ch != ' ' ) {
			int		row, col;
			float	frow, fcol, size;

			row = ch>>4;
			col = ch&15;

			frow = row * 0.0625;
			fcol = col * 0.0625;
			size = 0.0625;

			RB_AddQuadStampExt( origin, width, height, color, fcol, frow, fcol + size, frow + size );
		}
		
		origin = (origin * -2) + width;
	}
}
#endif

/*
==================
GlobalVectorToLocal
==================
*/
static float3 GlobalVectorToLocal( float3 vec ) {
	float3 o;
	o.x = dot( vec, tr.viewParms.ori.axis[0] );
	o.y = dot( vec, tr.viewParms.ori.axis[1] );
	o.z = dot( vec, tr.viewParms.ori.axis[2] );
	return o;
}

#if 0
/*
=====================
AutospriteDeform

Assuming all the triangles for this shader are independant
quads, rebuild them as forward facing sprites
=====================
*/
static void AutospriteDeform( void ) {
	int		i;
	int		oldVerts;
	float	*xyz;
	vec3_t	mid, delta;
	float	radius;
	vec3_t	left, up;
	vec3_t	leftDir, upDir;

	if ( tess.numVertexes & 3 ) {
		Com_Error( ERR_DROP, "Autosprite shader %s had odd vertex count", tess.shader->name );
	}
	if ( tess.numIndexes != ( tess.numVertexes >> 2 ) * 6 ) {
		Com_Error( ERR_DROP, "Autosprite shader %s had odd index count", tess.shader->name );
	}

	oldVerts = tess.numVertexes;
	tess.numVertexes = 0;
	tess.numIndexes = 0;

	if ( backEnd.currentEntity != &tr.worldEntity ) {
		GlobalVectorToLocal( backEnd.viewParms.ori.axis[1], leftDir );
		GlobalVectorToLocal( backEnd.viewParms.ori.axis[2], upDir );
	} else {
		VectorCopy( backEnd.viewParms.ori.axis[1], leftDir );
		VectorCopy( backEnd.viewParms.ori.axis[2], upDir );
	}

	for ( i = 0 ; i < oldVerts ; i+=4 ) {
		// find the midpoint
		xyz = tess.xyz[i];

		mid[0] = 0.25f * (xyz[0] + xyz[4] + xyz[8] + xyz[12]);
		mid[1] = 0.25f * (xyz[1] + xyz[5] + xyz[9] + xyz[13]);
		mid[2] = 0.25f * (xyz[2] + xyz[6] + xyz[10] + xyz[14]);

		VectorSubtract( xyz, mid, delta );
		radius = VectorLength( delta ) * 0.707f;		// / sqrt(2)

		VectorScale( leftDir, radius, left );
		VectorScale( upDir, radius, up );

		if ( backEnd.viewParms.isMirror ) {
			VectorSubtract( vec3_origin, left, left );
		}

		RB_AddQuadStamp( mid, left, up, tess.vertexColors[i] );
	}
}


/*
=====================
Autosprite2Deform

Autosprite2 will pivot a rectangular quad along the center of its long axis
=====================
*/
static const uint32_t edgeVerts[6][2] = {
	{ 0, 1 },
	{ 0, 2 },
	{ 0, 3 },
	{ 1, 2 },
	{ 1, 3 },
	{ 2, 3 }
};

static void Autosprite2Deform( void ) {
	int		i, j, k;
	int		indexes;
	float	*xyz;
	vec3_t	forward;

	if ( tess.numVertexes & 3 ) {
		ri.Printf( PRINT_WARNING, "Autosprite shader %s had odd vertex count", tess.shader->name );
	}
	if ( tess.numIndexes != ( tess.numVertexes >> 2 ) * 6 ) {
		ri.Printf( PRINT_WARNING, "Autosprite shader %s had odd index count", tess.shader->name );
	}

	if ( backEnd.currentEntity != &tr.worldEntity ) {
		GlobalVectorToLocal( backEnd.viewParms.ori.axis[0], forward );
	} else {
		VectorCopy( backEnd.viewParms.ori.axis[0], forward );
	}

	// this is a lot of work for two triangles...
	// we could precalculate a lot of it is an issue, but it would mess up
	// the shader abstraction
	for ( i = 0, indexes = 0 ; i < tess.numVertexes ; i+=4, indexes+=6 ) {
		float	lengths[2];
		int		nums[2];
		vec3_t	mid[2];
		vec3_t	major, minor;
		float	*v1, *v2;

		// find the midpoint
		xyz = tess.xyz[i];

		// identify the two shortest edges
		nums[0] = nums[1] = 0;
		lengths[0] = lengths[1] = WORLD_SIZE;//999999;	// ... instead of MAX_WORLD_COORD, so guaranteed to be outside legal range

		for ( j = 0 ; j < 6 ; j++ ) {
			float	l;
			vec3_t	temp;

			v1 = xyz + 4 * edgeVerts[j][0];
			v2 = xyz + 4 * edgeVerts[j][1];

			VectorSubtract( v1, v2, temp );

			l = DotProduct( temp, temp );
			if ( l < lengths[0] ) {
				nums[1] = nums[0];
				lengths[1] = lengths[0];
				nums[0] = j;
				lengths[0] = l;
			} else if ( l < lengths[1] ) {
				nums[1] = j;
				lengths[1] = l;
			}
		}

		for ( j = 0 ; j < 2 ; j++ ) {
			v1 = xyz + 4 * edgeVerts[nums[j]][0];
			v2 = xyz + 4 * edgeVerts[nums[j]][1];

			mid[j][0] = 0.5f * (v1[0] + v2[0]);
			mid[j][1] = 0.5f * (v1[1] + v2[1]);
			mid[j][2] = 0.5f * (v1[2] + v2[2]);
		}

		// find the vector of the major axis
		VectorSubtract( mid[1], mid[0], major );

		// cross this with the view direction to get minor axis
		CrossProduct( major, forward, minor );
		VectorNormalize( minor );

		// re-project the points
		for ( j = 0 ; j < 2 ; j++ ) {
			float	l;

			v1 = xyz + 4 * edgeVerts[nums[j]][0];
			v2 = xyz + 4 * edgeVerts[nums[j]][1];

			l = 0.5 * sqrt( lengths[j] );

			// we need to see which direction this edge
			// is used to determine direction of projection
			for ( k = 0 ; k < 5 ; k++ ) {
				if ( tess.indexes[ indexes + k ] == i + edgeVerts[nums[j]][0]
					&& tess.indexes[ indexes + k + 1 ] == i + edgeVerts[nums[j]][1] ) {
					break;
				}
			}

			if ( k == 5 ) {
				VectorMA( mid[j], l, minor, v1 );
				VectorMA( mid[j], -l, minor, v2 );
			} else {
				VectorMA( mid[j], -l, minor, v1 );
				VectorMA( mid[j], l, minor, v2 );
			}
		}
	}
}
#endif


/*
=====================
RB_DeformTessGeometry

=====================
*/
void RB_DeformTessGeometry( float3 position, float3 normal, float2 st ) {
	int i;
	deformStage_t ds;

	for( i = 0; i < tr_shader.numDeforms; i++ ) {
		ds = tr_shader.deforms[i];

		switch( ds.deformation ) {
		case DEFORM_NONE:
			break;
		case DEFORM_NORMALS:
			RB_CalcDeformNormal( position, normal, ds );
			break;
		case DEFORM_WAVE:
			RB_CalcDeformVertex( position, normal, ds );
			break;
		case DEFORM_BULGE:
			RB_CalcBulgeVertex( position, normal, st, ds );
			break;
		case DEFORM_MOVE:
			RB_CalcMoveVertex( position, ds );
			break;
#if 0
		case DEFORM_PROJECTION_SHADOW:
			RB_ProjectionShadowDeform();
			break;
		case DEFORM_AUTOSPRITE:
			AutospriteDeform();
			break;
		case DEFORM_AUTOSPRITE2:
			Autosprite2Deform();
			break;
		case DEFORM_TEXT0:
		case DEFORM_TEXT1:
		case DEFORM_TEXT2:
		case DEFORM_TEXT3:
		case DEFORM_TEXT4:
		case DEFORM_TEXT5:
		case DEFORM_TEXT6:
		case DEFORM_TEXT7:
//			DeformText( backEnd.refdef.text[ds->deformation - DEFORM_TEXT0] );
			DeformText( "Raven Software" );
			break;
#endif
		}
	}
}

/*
====================================================================

COLORS

====================================================================
*/


/*
** RB_UnpackColor
*/
float4 RB_UnpackColor( uint packed ) {
	float4 unpacked;

	unpacked.r = ( packed & 0xff ) / 255.0;
	unpacked.g = ( ( packed >> 8 ) & 0xff ) / 255.0;
	unpacked.b = ( ( packed >> 16 ) & 0xff ) / 255.0;
	unpacked.a = ( packed >> 24 ) / 255.0;

	return unpacked;
}

/*
** RB_PackColor
*/
uint RB_PackColor( float4 color ) {
	uint packed;
	uint4 clamped;

	clamped = (uint4)( saturate( color ) * 255.0 ) & 0xff;

	packed = clamped.r;
	packed |= clamped.g << 8;
	packed |= clamped.b << 16;
	packed |= clamped.a << 24;

	return packed;
}

/*
** RB_CalcColorFromEntity
*/
void RB_CalcColorFromEntity( out float4 dstColor ) {
	dstColor = RB_UnpackColor( tr_model.color );
}

/*
** RB_CalcColorFromOneMinusEntity
*/
void RB_CalcColorFromOneMinusEntity( out float4 dstColor ) {
	dstColor = 1 - RB_UnpackColor( tr_model.color );
}

/*
** RB_CalcAlphaFromEntity
*/
void RB_CalcAlphaFromEntity( inout float4 dstColor ) {
	dstColor.a = RB_UnpackColor( tr_model.color ).a;
}

/*
** RB_CalcAlphaFromOneMinusEntity
*/
void RB_CalcAlphaFromOneMinusEntity( inout float4 dstColor ) {
	dstColor.a = 1 - RB_UnpackColor( tr_model.color ).a;
}

/*
** RB_CalcWaveColor
*/
void RB_CalcWaveColor( waveForm_t wf, out float4 dstColor ) {
	float glow;

	if( wf.func == GF_NOISE ) {
		glow = wf.base + R_NoiseGet( float4( 0, 0, 0, ( tr.floatTime + wf.phase ) * wf.frequency ) ) * wf.amplitude;
	}
	else {
		glow = EvalWaveForm( wf ) * tr.identityLight;
	}
	glow = saturate( glow );

	dstColor.xyz = glow;
}

/*
** RB_CalcWaveAlpha
*/
float RB_CalcWaveAlpha( waveForm_t wf ) {
	return EvalWaveFormClamped( wf );
}

/*
====================================================================

TEX COORDS

====================================================================
*/

/*
========================
RB_CalcFogTexCoords

To do the clipped fog plane really correctly, we should use
projected textures, but I don't trust the drivers and it
doesn't fit our shader data.
========================
*/

float2 RB_CalcFogTexCoords( float3 v ) {
	float2 st;
	float s, t;
	float eyeT;
	bool eyeOutside;
	fog_t fog;
	float3 localVec;
	float4 fogDistanceVector, fogDepthVector;

	fog = tr_fogs[tr_shader.fogNum];

	// all fogging distance is based on world Z units
	localVec = tr_model.ori.origin - tr.viewParms.ori.origin;
	fogDistanceVector.xyz = -tr.viewParms.ori.modelMatrix[2].xyz;
	fogDistanceVector[3] = dot( localVec, tr.viewParms.ori.axis[0] );

	// scale the fog vectors based on the fog's thickness
	fogDistanceVector *= fog.tcScale;

	// rotate the gradient vector for this orientation
	if( fog.hasSurface ) {
		fogDepthVector[0] = fog.surface[0] * tr.viewParms.ori.axis[0][0] +
							fog.surface[1] * tr.viewParms.ori.axis[0][1] + fog.surface[2] * tr.viewParms.ori.axis[0][2];
		fogDepthVector[1] = fog.surface[0] * tr.viewParms.ori.axis[1][0] +
							fog.surface[1] * tr.viewParms.ori.axis[1][1] + fog.surface[2] * tr.viewParms.ori.axis[1][2];
		fogDepthVector[2] = fog.surface[0] * tr.viewParms.ori.axis[2][0] +
							fog.surface[1] * tr.viewParms.ori.axis[2][1] + fog.surface[2] * tr.viewParms.ori.axis[2][2];
		fogDepthVector[3] = -fog.surface[3] + dot( tr.viewParms.ori.origin, fog.surface.xyz );

		eyeT = dot( tr.viewParms.ori.viewOrigin, fogDepthVector.xyz ) + fogDepthVector[3];
	}
	else {
		eyeT = 1; // non-surface fog always has eye inside
		fogDepthVector = float4( 0, 0, 0, 1 );
	}

	// see if the viewpoint is outside
	// this is needed for clipping distance even for constant fog

	if( eyeT < 0 ) {
		eyeOutside = true;
	}
	else {
		eyeOutside = false;
	}

	fogDistanceVector.w += 1.0 / 512;

	// calculate the length in fog
	s = dot( v, fogDistanceVector.xyz ) + fogDistanceVector.w;
	t = dot( v, fogDepthVector.xyz ) + fogDepthVector.w;

	// partially clipped fogs use the T axis
	if( eyeOutside ) {
		if( t < 1.0 ) {
			t = 1.0 / 32; // point is outside, so no fogging
		}
		else {
			t = 1.0 / 32 + 30.0 / 32 * t / ( t - eyeT ); // cut the distance at the fog plane
		}
	}
	else {
		if( t < 0 ) {
			t = 1.0 / 32; // point is outside, so no fogging
		}
		else {
			t = 31.0 / 32;
		}
	}

	st.x = Q_isnan( s ) ? 0 : s;
	st.y = Q_isnan( t ) ? 0 : t;

	return st;
}

#if 0
/*
** RB_CalcEnvironmentTexCoords
*/
void RB_CalcEnvironmentTexCoords( float3 v, float3 normal, inout float2 st ) {
	float3 viewer;
	float d;

	if( backEnd.currentEntity && backEnd.currentEntity->e.renderfx & RF_FIRST_PERSON ) // this is a view model so we must use world lights instead of vieworg
	{
		d = dot( normal, backEnd.currentEntity->lightDir );
		st = normal.xy * d - backEnd.currentEntity->lightDir.xy;
	}
	else { // the normal way
		viewer = normalize( tr.viewParms.ori.viewOrigin - v );

		d = dot( normal, viewer );
		st = normal.xy * d - 0.5 * viewer.xy;
	}
}
#endif

/*
** RB_CalcTurbulentTexCoords
*/
void RB_CalcTurbulentTexCoord( waveForm_t wf, float3 v, inout float2 st ) {
	float now;

	now = ( wf.phase + tr.floatTime * wf.frequency );

	st.x += R_sinFunc( (int)( ( ( v.x + v.z ) * 1.0 / 128 * 0.125 + now ) * TR_FUNCTABLE_SIZE ) ) * wf.amplitude;
	st.y += R_sinFunc( (int)( ( v.y * 1.0 / 128 * 0.125 + now ) * TR_FUNCTABLE_SIZE ) ) * wf.amplitude;
}

/*
** RB_CalcScaleTexCoords
*/
void RB_CalcScaleTexCoords( float2 scale, inout float2 st ) {
	st *= scale;
}

/*
** RB_CalcScrollTexCoords
*/
void RB_CalcScrollTexCoords( float2 scrollSpeed, inout float2 st ) {
	float timeScale = tr.floatTime;
	float2 adjustedScroll;

	adjustedScroll = scrollSpeed * timeScale;

	// clamp so coordinates don't continuously get larger, causing problems
	// with hardware limits
	adjustedScroll = adjustedScroll - floor( adjustedScroll );

	st += adjustedScroll;
}

/*
** RB_CalcTransformTexCoords
*/
void RB_CalcTransformTexCoords( texModInfo_t tmi, inout float2 st ) {
	float s = st.x;
	float t = st.y;

	st.x = s * tmi.mat[0][0] + t * tmi.mat[1][0] + tmi.translate[0];
	st.y = s * tmi.mat[0][1] + t * tmi.mat[1][1] + tmi.translate[1];
}

/*
** RB_CalcStretchTexCoords
*/
void RB_CalcStretchTexCoords( waveForm_t wf, inout float2 st ) {
	float p;
	texModInfo_t tmi;

	p = 1.0f / EvalWaveForm( wf );

	tmi.mat[0][0] = p;
	tmi.mat[1][0] = 0;
	tmi.translate[0] = 0.5f - 0.5f * p;

	tmi.mat[0][1] = 0;
	tmi.mat[1][1] = p;
	tmi.translate[1] = 0.5f - 0.5f * p;

	RB_CalcTransformTexCoords( tmi, st );
}

/*
** RB_CalcRotateTexCoords
*/
void RB_CalcRotateTexCoords( float degsPerSecond, inout float2 st ) {
	float timeScale = tr.floatTime;
	float degs;
	float rads;
	float sinValue, cosValue;
	texModInfo_t tmi;

	degs = -degsPerSecond * timeScale;
	rads = radians( degs );

	sincos( rads, sinValue, cosValue );

	tmi.mat[0][0] = cosValue;
	tmi.mat[1][0] = -sinValue;
	tmi.translate[0] = 0.5 - 0.5 * cosValue + 0.5 * sinValue;

	tmi.mat[0][1] = sinValue;
	tmi.mat[1][1] = cosValue;
	tmi.translate[1] = 0.5 - 0.5 * sinValue - 0.5 * cosValue;

	RB_CalcTransformTexCoords( tmi, st );
}

static void ComputeTexCoords( vertex_t vertex, out stageVars_t svars ) {
	int i;
	int b;

	for( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		int tm;

		//
		// generate the texture coordinates
		//
		switch( tr_shader.bundle[b].tcGen ) {
		case TCGEN_IDENTITY:
			svars.texcoords[b] = float2( 0, 0 );
			break;
		case TCGEN_TEXTURE:
			svars.texcoords[b] = vertex.texCoord0;
			break;
		case TCGEN_LIGHTMAP:
			svars.texcoords[b] = vertex.texCoord1;
			break;
		case TCGEN_LIGHTMAP1:
			svars.texcoords[b] = vertex.texCoord2;
			break;
		case TCGEN_LIGHTMAP2:
			svars.texcoords[b] = vertex.texCoord3;
			break;
		case TCGEN_LIGHTMAP3:
			svars.texcoords[b] = vertex.texCoord4;
			break;
		case TCGEN_VECTOR:
			svars.texcoords[b].x = dot( vertex.position.xyz, tr_shader.bundle[b].tcGenVectors[0].xyz );
			svars.texcoords[b].y = dot( vertex.position.xyz, tr_shader.bundle[b].tcGenVectors[1].xyz );
			break;
		case TCGEN_FOG:
			svars.texcoords[b] = RB_CalcFogTexCoords( vertex.position.xyz );
			break;
		case TCGEN_ENVIRONMENT_MAPPED:
#if 0
			if( r_environmentMapping->integer )
				svars.texcoords[b] = RB_CalcEnvironmentTexCoords();
			else
#endif
			svars.texcoords[b] = float2( 0, 0 );
			break;
		case TCGEN_BAD:
			return;
		}

		//
		// alter texture coordinates
		//
		for( tm = 0; tm < tr_shader.bundle[b].numTexMods; tm++ ) {
			switch( tr_shader.bundle[b].texMods[tm].type ) {
			case TMOD_NONE:
				tm = TR_MAX_TEXMODS; // break out of for loop
				break;

			case TMOD_TURBULENT:
				RB_CalcTurbulentTexCoord( tr_shader.bundle[b].texMods[tm].wave, vertex.position.xyz, svars.texcoords[b] );
				break;

			case TMOD_ENTITY_TRANSLATE:
				RB_CalcScrollTexCoords( tr_model.texCoordScrollSpeed, svars.texcoords[b] );
				break;

			case TMOD_SCROLL:
				RB_CalcScrollTexCoords( tr_shader.bundle[b].texMods[tm].translate, svars.texcoords[b] ); // union scroll into translate
				break;

			case TMOD_SCALE:
				RB_CalcScaleTexCoords( tr_shader.bundle[b].texMods[tm].translate, svars.texcoords[b] ); // union scroll into translate
				break;

			case TMOD_STRETCH:
				RB_CalcStretchTexCoords( tr_shader.bundle[b].texMods[tm].wave, svars.texcoords[b] );
				break;

			case TMOD_TRANSFORM:
				RB_CalcTransformTexCoords( tr_shader.bundle[b].texMods[tm], svars.texcoords[b] );
				break;

			case TMOD_ROTATE:
				RB_CalcRotateTexCoords( tr_shader.bundle[b].texMods[tm].translate[0], svars.texcoords[b] ); // union rotateSpeed into translate[0]
				break;

			default:
				break;
			}
		}
	}
}

/*
================
R_FogFactor

Returns a 0.0 to 1.0 fog density value
This is called for each texel of the fog texture on startup
and for each vertex of transparent shaders in fog dynamically
================
*/
float R_FogFactor( float2 st ) {
	float d;

	st.x -= 1.0 / 512;
	if( st.x < 0 ) {
		return 0;
	}
	if( st.y < 1.0 / 32 ) {
		return 0;
	}
	if( st.y < 31.0 / 32 ) {
		st.x *= ( st.y - 1.0f / 32.0f ) / ( 30.0f / 32.0f );
	}

	// we need to leave a lot of clamp range
	st.x *= 8;

	if( st.x > 1.0 ) {
		st.x = 1.0;
	}

	return sqrt( st.x );
}

/*
** RB_CalcModulateColorsByFog
*/
void RB_CalcModulateColorsByFog( float3 position, inout float4 color ) {
	float2 texCoords;

	// calculate texcoords so we can derive density
	// this is not wasted, because it would only have
	// been previously called if the surface was opaque
	texCoords = RB_CalcFogTexCoords( position );

	float f = 1.0 - R_FogFactor( texCoords );
	color.rgb *= f;
}

/*
** RB_CalcModulateAlphasByFog
*/
void RB_CalcModulateAlphasByFog( float3 position, inout float4 color ) {
	float2 texCoords;

	// calculate texcoords so we can derive density
	// this is not wasted, because it would only have
	// been previously called if the surface was opaque
	texCoords = RB_CalcFogTexCoords( position );

	float f = 1.0 - R_FogFactor( texCoords );
	color.a *= f;
}

/*
** RB_CalcModulateRGBAsByFog
*/
void RB_CalcModulateRGBAsByFog( float3 position, inout float4 color ) {
	float2 texCoords;

	// calculate texcoords so we can derive density
	// this is not wasted, because it would only have
	// been previously called if the surface was opaque
	texCoords = RB_CalcFogTexCoords( position );

	float f = 1.0 - R_FogFactor( texCoords );
	color *= f;
}

/*
** RB_CalcSpecularAlpha
**
** Calculates specular coefficient and places it in the alpha channel
*/
float RB_CalcSpecularAlpha( float3 position, float3 normal ) {
	float3 lightOrigin = { -960, 1980, 96 }; // FIXME: track dynamically

	float3 viewer, reflected;
	float l, d;
	float ilength;
	float3 lightDir = tr_model.lightDir;

	// calculate the specular color
	d = 2 * dot( normal, lightDir );

	// we don't optimize for the d < 0 case since this tends to
	// cause visual artifacts such as faceted "snapping"
	reflected = normal * d - lightDir;

	viewer = tr.viewParms.ori.viewOrigin - position;
	ilength = 1 / sqrt( dot( viewer, viewer ) );
	l = dot( reflected, viewer );
	l *= ilength;

	if( l < 0 ) {
		return 0;
	}
	else {
		l = l * l;
		l = l * l;
		return saturate( l );
	}
}

/*
** RB_CalcDiffuseColor
**
** The basic vertex lighting calc
*/
void RB_CalcDiffuseColor( float3 normal, inout float4 color ) {
	int j;
	float incoming;
	float4 ambientLightColor;
	float3 ambientLight;
	float3 lightDir;
	float3 directedLight;

	ambientLightColor = RB_UnpackColor( tr_model.ambientLightInt );
	ambientLight = tr_model.ambientLight.xyz;
	directedLight = tr_model.directedLight.xyz;
	lightDir = tr_model.lightDir.xyz;

	incoming = dot( normal, lightDir );
	if( incoming <= 0 ) {
		color = ambientLightColor;
		return;
	}

	color.rgb = saturate( ambientLight + incoming * directedLight );
	color.a = 1;
}

/*
** RB_CalcDiffuseColorEntity
**
** The basic vertex lighting calc * Entity Color
*/
void RB_CalcDiffuseEntityColor( float3 normal, inout float4 color ) {
	float4 entColor = RB_UnpackColor( tr_model.e.shaderRGBA );

	RB_CalcDiffuseColor( normal, color );

	color.rgb *= entColor.rgb;
	color.a = entColor.a;
}

//---------------------------------------------------------
void RB_CalcDisintegrateColors( float3 position, inout float4 color, colorGen_t rgbGen ) {
	float dis, threshold;
	float3 temp;
	float4 entColor = RB_UnpackColor( tr_model.e.shaderRGBA );

	// calculate the burn threshold at the given time, anything that passes the threshold will get burnt
	threshold = ( ( tr.floatTime * 1000 ) - tr_model.e.endTime ) * 0.045f; // endTime is really the start time, maybe I should just use a completely meaningless substitute?

	if( tr_model.e.renderfx & RF_DISINTEGRATE1 ) {
		// this handles the blacken and fading out of the regular player model
		temp = tr_model.e.oldorigin - position;
		dis = dot( temp, temp );

		if( dis < threshold * threshold ) {
			// completely disintegrated
			color = float4( 0, 0, 0, 0 );
		}
		else if( dis < threshold * threshold + 60 ) {
			// blacken before fading out
			color = float4( 0, 0, 0, 1 );
		}
		else if( dis < threshold * threshold + 150 ) {
			// darken more
			if( rgbGen == CGEN_LIGHTING_DIFFUSE_ENTITY ) {
				color.rgb = entColor.rgb * 0.43529;
			}
			else {
				color.rgb = 0.43529;
			}
			color.a = 1;
		}
		else if( dis < threshold * threshold + 180 ) {
			// darken at edge of burn
			if( rgbGen == CGEN_LIGHTING_DIFFUSE_ENTITY ) {
				color.rgb = entColor.rgb * 0.68627;
			}
			else {
				color.rgb = 0.68627;
			}
			color.a = 1;
		}
		else {
			// not burning at all yet
			if( rgbGen == CGEN_LIGHTING_DIFFUSE_ENTITY ) {
				color.rgb = entColor.rgb;
			}
			else {
				color.rgb = float3( 1, 1, 1 );
			}
			color.a = 1;
		}
	}
	else if( tr_model.e.renderfx & RF_DISINTEGRATE2 ) {
		// this handles the glowing, burning bit that scales away from the model
		temp = tr_model.e.oldorigin - position;
		dis = dot( temp, temp );

		if( dis < threshold * threshold ) {
			// done burning
			color = float4( 0, 0, 0, 0 );
		}
		else {
			// still full burn
			color = float4( 1, 1, 1, 1 );
		}
	}
}

//---------------------------------------------------------
void RB_CalcDisintegrateVertDeform( inout float3 position, float3 normal ) {
	float scale;
	float3 temp;

	if( tr_model.e.renderfx & RF_DISINTEGRATE2 ) {
		float threshold = ( ( tr.floatTime * 1000 ) - tr_model.e.endTime ) * 0.045f;

		temp = tr_model.e.oldorigin - position.xyz;
		scale = dot( temp, temp );

		if( scale < threshold * threshold ) {
			position.xy += normal.xy * 2;
			position.z += normal.z * 0.5;
		}
		else if( scale < threshold * threshold + 50 ) {
			position.xy += normal.xy;
		}
	}
}

/*
===============
ComputeColors
===============
*/
static float4 ComputeColor( float3 position, float3 normal, float4 vertexColor, alphaGen_t forceAlphaGen, colorGen_t forceRGBGen ) {
	float4 entColor = RB_UnpackColor( tr_model.e.shaderRGBA );
	float4 shaderColor = RB_UnpackColor( tr_shader.constantColor );
	float4 color;

	if( tr_model.e.renderfx & ( RF_DISINTEGRATE1 | RF_DISINTEGRATE2 ) ) {
		RB_CalcDisintegrateColors( position, color, tr_shader.rgbGen );

		// We've done some custom alpha and color stuff, so we can skip the rest.  Let it do fog though
		forceRGBGen = CGEN_SKIP;
		forceAlphaGen = AGEN_SKIP;
	}

	//
	// rgbGen
	//
	if( !forceRGBGen ) {
		forceRGBGen = tr_shader.rgbGen;
	}

	if( tr_model.e.renderfx & RF_VOLUMETRIC ) // does not work for rotated models, technically, this should also be a CGEN type, but that would entail adding new shader commands....which is too much work for one thing
	{
		float d;

		d = dot( normal, tr.viewaxis[0].xyz );
		d *= d * d * d;

		if( d < 0.2f ) // so low, so just clamp it
		{
			d = 0.0f;
		}

		color = entColor.r * ( 1 - d );

		forceRGBGen = CGEN_SKIP;
		forceAlphaGen = AGEN_SKIP;
	}

	if( !forceAlphaGen ) // set this up so we can override below
	{
		forceAlphaGen = tr_shader.alphaGen;
	}

	switch( forceRGBGen ) {
	case CGEN_SKIP:
		break;
	case CGEN_IDENTITY:
		color = float4( 1, 1, 1, 1 );
		break;
	default:
	case CGEN_IDENTITY_LIGHTING:
		color = tr.identityLight;
		break;
	case CGEN_LIGHTING_DIFFUSE:
		RB_CalcDiffuseColor( normal, color );
		break;
	case CGEN_LIGHTING_DIFFUSE_ENTITY:
		RB_CalcDiffuseEntityColor( normal, color );
		if( forceAlphaGen == AGEN_IDENTITY && entColor.a == 1 ) {
			forceAlphaGen = AGEN_SKIP; // already got it in this set since it does all 4 components
		}
		break;
	case CGEN_EXACT_VERTEX:
		color = vertexColor;
		break;
	case CGEN_CONST:
		color = shaderColor;
		break;
	case CGEN_VERTEX:
		color = float4( vertexColor.xyz * tr.identityLight, vertexColor.a );
		break;
	case CGEN_ONE_MINUS_VERTEX:
		color = float4( ( 1 - vertexColor.xyz ) * tr.identityLight, 1 - vertexColor.a );
		break;
	case CGEN_FOG: {
		fog_t fog = tr_fogs[tr_shader.fogNum];
		color = RB_UnpackColor( fog.colorInt );
	} break;
	case CGEN_WAVEFORM:
		RB_CalcWaveColor( tr_shader.rgbWave, color );
		break;
	case CGEN_ENTITY:
		RB_CalcColorFromEntity( color );
		if( forceAlphaGen == AGEN_IDENTITY && entColor.a == 1 ) {
			forceAlphaGen = AGEN_SKIP; // already got it in this set since it does all 4 components
		}
		break;
	case CGEN_ONE_MINUS_ENTITY:
		RB_CalcColorFromOneMinusEntity( color );
		break;
#if 0
	case CGEN_LIGHTMAPSTYLE:
		byteAlias_t *baDest = (byteAlias_t *)&tess.svars.colors[i],
					*baSource = (byteAlias_t *)&styleColors[pStage->lightmapStyle];
		baDest->ui = baSource->ui;
		break;
#endif
	}

	//
	// alphaGen
	//

	switch( forceAlphaGen ) {
	case AGEN_SKIP:
		break;
	case AGEN_IDENTITY:
		if( forceRGBGen != CGEN_IDENTITY && forceRGBGen != CGEN_LIGHTING_DIFFUSE ) {
			if( ( forceRGBGen == CGEN_VERTEX && tr.identityLight != 1 ) ||
				forceRGBGen != CGEN_VERTEX ) {
				color.a = 1;
			}
		}
		break;
	case AGEN_CONST:
		if( forceRGBGen != CGEN_CONST ) {
			color.a = shaderColor.a;
		}
		break;
	case AGEN_WAVEFORM:
		color.a = RB_CalcWaveAlpha( tr_shader.alphaWave );
		break;
	case AGEN_LIGHTING_SPECULAR:
		color.a = RB_CalcSpecularAlpha( position, normal );
		break;
	case AGEN_ENTITY:
		if( forceRGBGen != CGEN_ENTITY ) { // already got it in the CGEN_entity since it does all 4 components
			RB_CalcAlphaFromEntity( color );
		}
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity( color );
		break;
	case AGEN_VERTEX:
		if( forceRGBGen != CGEN_VERTEX ) {
			color.a = vertexColor.a;
		}
		break;
	case AGEN_ONE_MINUS_VERTEX:
		color.a = 1 - vertexColor.a;
		break;
	case AGEN_PORTAL: {
		float alpha;
		float len;
		float3 v;

		v = position - tr.viewParms.ori.origin;
		len = length( v );
		alpha = saturate( len / tr_shader.portalRange );

		color.a = alpha;
	} break;
	case AGEN_BLEND:
#if 0
		if( forceRGBGen != CGEN_VERTEX ) {
			color.a = vertexAlphas[pStage->index];
		}
#endif
		break;
	default:
		break;
	}

	//
	// fog adjustment for colors to fade out as fog increases
	//
	if( tr_shader.fogNum ) {
		switch( tr_shader.adjustColorsForFog ) {
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog( position, color );
			break;
		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog( position, color );
			break;
		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog( position, color );
			break;
		case ACFF_NONE:
			break;
		default:
			break;
		}
	}

	return color;
}

static void ForceAlpha( inout float4 color, float alpha ) {
	color.a = alpha;
}


void VS_ApplyModelTransform( inout float4 position, inout float4 normal ) {
	//float4x4 modelMatrix = tr_model.ori.modelMatrix;
	//
	//RB_CalcDisintegrateVertDeform( position.xyz, normal.xyz );
	//
	//position = mul( modelMatrix, position );
	//normal = mul( modelMatrix, normal );

	// convert to 640x480
	position.xy /= float2( 320, 240 );
	position.xy -= 1;
}


typedef struct {
	float4 position : SV_Position;
	float3 entPosition : POSITION;
	float3 entNormal : NORMAL;
	float2 texcoord : TEXCOORD;
	float4 color : COLOR;
} shadeVertex_t;

shadeVertex_t VS_Main( vertex_t v ) {
	shadeVertex_t o;
	o.texcoord = v.texCoord0;
	o.color = v.vertexColor[0];

	float4 position = float4( v.position.xyz, 1 );
	float4 normal = float4( v.normal.xyz, 0 );
	VS_ApplyModelTransform( position, normal );

	o.entPosition = position.xyz;
	o.entNormal = normal.xyz;

	// VS_ApplyViewTransform( o.position, o.normal );
	o.position = position;

#ifdef TR_WIREFRAME
	// previous color computation should be optimized-out
	o.color = float4( tr_wireframe.color, 1 );
#endif
	return o;
}

float4 PS_Main( shadeVertex_t i )
	: SV_Target {
#ifdef TR_WIREFRAME
	return i.color;
#endif
	float4 color = ComputeColor( i.entPosition, i.entNormal, i.color, 0, 0 );
	float4 tex_0 = tr_texture_0.Sample( tr_sampler_0, i.texcoord );
	return color * tex_0;
}
