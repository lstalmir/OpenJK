/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// tr_shade.c

#include "../server/exe_headers.h"

#include "tr_local.h"

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

shaderCommands_t	tess;

color4ub_t	styleColors[MAX_LIGHT_STYLES];
bool		styleUpdated[MAX_LIGHT_STYLES];

extern bool g_bRenderGlowingObjects;


static void R_DrawElements( shaderCommands_t *input ) {
	vertexBuffer_t *draw;
	VkBuffer vertexBuffer;
	VkDeviceSize vertexOffset;
	VkDeviceSize indexOffset;
	int i;

	for( i = 0; i < input->numDraws; ++i ) {
		draw = input->draws[i];

		vertexBuffer = draw->b.buf;
		vertexOffset = (VkDeviceSize)draw->vertexOffset;
		indexOffset = (VkDeviceSize)draw->indexOffset;

		// bind vertex buffers
		vkCmdBindVertexBuffers( backEndData->cmdbuf, 0, 1, &vertexBuffer, &vertexOffset );
		vkCmdBindIndexBuffer( backEndData->cmdbuf, vertexBuffer, indexOffset, g_scIndexType );

		// draw
		vkCmdDrawIndexed( backEndData->cmdbuf, draw->numIndexes, 1, 0, 0, 0 );
	}
}


/*
=============================================================

SURFACE SHADERS

=============================================================
*/


void VK_BindImage( image_t *image, int loc ) {
	vkCmdBindDescriptorSets( backEndData->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, backEndData->pipelineLayout,
		TR_TEXTURE_SPACE_0 + loc, 1, &image->descriptorSet, 0, NULL );
}


/*
=================
R_BindAnimatedImage

=================
*/
void R_BindAnimatedImage( const tr_shader::textureBundle_t *bundle, int loc = 1 ) {
	int		index;

	if ( bundle->isVideoMap ) {
		ri.CIN_RunCinematic( bundle->cppData.videoMapHandle );
		ri.CIN_UploadCinematic( bundle->cppData.videoMapHandle );
		return;
	}

	if ((r_fullbright->integer || tr.refdef.doLAGoggles || (tr.refdef.rdflags & RDF_doFullbright) ) && bundle->isLightmap)
	{
		VK_BindImage( tr.whiteImage, loc );
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		VK_BindImage( bundle->cppData.image, loc );
		return;
	}

	if (backEnd.currentEntity->e.renderfx & RF_SETANIMINDEX )
	{
		index = backEnd.currentEntity->e.skinNum;
	}
	else
	{
		// it is necessary to do this messy calc to make sure animations line up
		// exactly with waveforms of the same frequency
		index = Q_ftol( backEnd.refdef.floatTime * bundle->imageAnimationSpeed * TR_FUNCTABLE_SIZE );
		index >>= TR_FUNCTABLE_SIZE2;

		if ( index < 0 ) {
			index = 0;	// may happen with shader time offsets
		}
	}

	if ( bundle->oneShotAnimMap )
	{
		if ( index >= bundle->numImageAnimations )
		{
			// stick on last frame
			index = bundle->numImageAnimations - 1;
		}
	}
	else
	{
		// loop
		index %= bundle->numImageAnimations;
	}

	VK_BindImage( *( (image_t **)bundle->cppData.image + index ), loc );
}


/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris (shaderCommands_t *input) {
	VkPipeline pipeline;
	vec3_t color;

	// Check if GPU supports wireframe rasterization.
	if (!vkState.wireframeRasterizationSupported) {
		return;
	}

	if ( r_showtriscolor->integer )
	{
		int i = r_showtriscolor->integer;
		if (i == 42) {
			i = Q_irand(0,8);
		}
		switch (i)
		{
		case 1:
			VectorSet( color, 1.0, 0.0, 0.0 ); // red
			break;
		case 2:
			VectorSet( color, 0.0, 1.0, 0.0 ); // green
			break;
		case 3:
			VectorSet( color, 1.0, 1.0, 0.0); //yellow
			break;
		case 4:
			VectorSet( color, 0.0, 0.0, 1.0 ); // blue
			break;
		case 5:
			VectorSet( color, 0.0, 1.0, 1.0 ); // cyan
			break;
		case 6:
			VectorSet( color, 1.0, 0.0, 1.0 ); //magenta
			break;
		case 7:
			VectorSet( color, 0.8f, 0.8f, 0.8f); //white/grey
			break;
		case 8:
			VectorSet( color, 0.0, 0.0, 0.0); //black
			break;
		}
	}
	else
	{
		VectorSet( color, 1.0, 1.0, 1.0 ); // white
	}

	if ( r_showtris->integer == 2 )
	{
		// tries to do non-xray style showtris
		pipeline = tr.wireframePipeline;
	}
	else
	{
		// same old showtris
		pipeline = tr.wireframeXRayPipeline;
	}

	vkCmdBindPipeline( backEndData->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
	vkCmdPushConstants( backEndData->cmdbuf, tr.wireframePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( color ), color );

	backEndData->pipeline = pipeline;
	backEndData->pipelineLayout = tr.wireframePipelineLayout;

	R_DrawElements( input );
}

#if 0
/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals (shaderCommands_t *input) {
	int		i;
	vec3_t	temp;

	VK_BindImage( tr.whiteImage );
	qglColor3f (1,1,1);
	qglDepthRange( 0, 0 );	// never occluded
	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );

	qglBegin (GL_LINES);
	for (i = 0 ; i < input->numVertexes ; i++) {
		qglVertex3fv (input->xyz[i]);
		VectorMA (input->xyz[i], 2, input->normal[i], temp);
		qglVertex3fv (temp);
	}
	qglEnd ();

	qglDepthRange( 0, 1 );
}
#endif


/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader, int fogNum ) {
//	shader_t *state = (shader->remappedShader) ? shader->remappedShader : shader;
	shader_t *state = shader;

	RB_SetShader( shader );

	tess.numDraws = 0;
	tess.shader = state;//shader;
	tess.fogNum = fogNum;
	tess.dlightBits = 0;		// will be OR'd in by surface functions

	tess.SSInitializedWind = qfalse;	//is this right?

	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;

	tess.shader = shader;
	tess.currentStageIteratorFunc = shader->sky ? RB_StageIteratorSky : RB_StageIteratorGeneric;

	tess.fading = false;

	tess.registration++;
}

/*
==============
RB_DrawSurface

Appends a surface to the draw list for the current shader.
==============
*/
void RB_DrawSurface( vertexBuffer_t *vertexBuffer ) {
	tess.draws[tess.numDraws++] = vertexBuffer;
}

//--EF_old dlight code...reverting back to Quake III dlight to see if people like that better
// Lifted the whole function because someone hacked the heck out of this and it doesn't seem to
//	be a case where it's as easy as just changing the blend mode....
/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
/*
static void ProjectDlightTexture( void ) {
	int		l;
	vec3_t	origin;
	float	*texCoords;
	byte	*colors;
	byte	clipBits[SHADER_MAX_VERTEXES];
	float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	byte	colorArray[SHADER_MAX_VERTEXES][4];
	unsigned	hitIndexes[SHADER_MAX_INDEXES];

	if ( !backEnd.refdef.num_dlights ) {
		return;
	}

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		int		numIndexes;
		vec3_t	floatColor;
		float	scale;
		float	radius, chord;
		dlight_t	*dl;
		int i;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}
		texCoords = texCoordsArray[0];
		colors = colorArray[0];

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;
		chord = radius*radius*0.25f;
		scale = 1.0f / radius;
		floatColor[0] = dl->color[0] * 255f;
		floatColor[1] = dl->color[1] * 255f;
		floatColor[2] = dl->color[2] * 255f;

		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords += 2, colors += 4 ) {
			vec3_t	distVec;
			int		clip;
			float	tempColor;
			float	modulate, dist;

//			if ( 0 ) {
//				clipBits[i] = 255;	// definately not dlighted
//				continue;
//			}
//
			backEnd.pc.c_dlightVertexes++;

			VectorSubtract( origin, tess.xyz[i], distVec );
			dist = VectorLengthSquared(distVec);

			texCoords[0] = 0.5 + distVec[0] * scale;	//xy projection
			texCoords[1] = 0.5 + distVec[1] * scale;

			clip = 0;
			if ( texCoords[0] < 0 ) {
				clip |= 1;
			} else if ( texCoords[0] > 1 ) {
				clip |= 2;
			}
			if ( texCoords[1] < 0 ) {
				clip |= 4;
			} else if ( texCoords[1] > 1 ) {
				clip |= 8;
			}
			clipBits[i] = clip;

			// modulate the strength based on the height and color
			if ( dist > chord) {
				clip |= 16;
				modulate = 255*1.0ff;
			} else {
				modulate = 255*2*dist*scale*scale;
			}
			tempColor = floatColor[0] + modulate;
			colors[0] = tempColor > 255 ? 255: Q_ftol(tempColor);

			tempColor = floatColor[1] + modulate;
			colors[1] = tempColor > 255 ? 255: Q_ftol(tempColor);

			tempColor = floatColor[2] + modulate;
			colors[2] = tempColor > 255 ? 255: Q_ftol(tempColor);

//			colors[3] = 255;
			if ( distVec[2] > radius ) {
				colors[3] = 0;
			} else if ( distVec[2] < -radius ) {
				colors[3] = 0;
			} else {
				if ( distVec[2] < 0 ) {
					distVec[2] = -distVec[2];
				}
				if ( distVec[2] < radius * 0.5 ) {
					colors[3] = 255;
				} else {
					colors[3] = Q_ftol(255* (radius - distVec[2]) * scale);
				}
			}

		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			int		a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i+1];
			c = tess.indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}

		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );

		qglEnableClientState( GL_COLOR_ARRAY );
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );

		VK_BindImage( tr.dlightImage );

		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_SRC_COLOR | GLS_DEPTHFUNC_EQUAL);//our way
//		GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );	//Id way
		R_DrawElements( numIndexes, hitIndexes );
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
}
*/

#if 0
// Lifted from Quake III to see if people like this kind of dlight better
/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
static void ProjectDlightTexture2( void ) {
	int		i, l;
	vec3_t	origin;
	byte	clipBits[SHADER_MAX_VERTEXES];
	float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	float	oldTexCoordsArray[SHADER_MAX_VERTEXES][2];
	float	vertCoordsArray[SHADER_MAX_VERTEXES][4];
	unsigned int		colorArray[SHADER_MAX_VERTEXES];
	uint32_t	hitIndexes[SHADER_MAX_INDEXES];
	int		numIndexes;
	float	radius;
#ifndef JK2_MODE
	int		fogging;
#endif
	shaderStage_t *dStage;
	vec3_t	posa;
	vec3_t	posb;
	vec3_t	posc;
	vec3_t	dist;
	vec3_t	e1;
	vec3_t	e2;
	vec3_t	normal;
	float	fac,modulate;
	vec3_t	floatColor;
	byte colorTemp[4];

	int		needResetVerts=0;

	if ( !backEnd.refdef.num_dlights )
	{
		return;
	}

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ )
	{
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;

		int		clipall = 63;
		for ( i = 0 ; i < tess.numVertexes ; i++)
		{
			int		clip;
			VectorSubtract( origin, tess.xyz[i], dist );

			clip = 0;
			if (  dist[0] < -radius )
			{
				clip |= 1;
			}
			else if ( dist[0] > radius )
			{
				clip |= 2;
			}
			if (  dist[1] < -radius )
			{
				clip |= 4;
			}
			else if ( dist[1] > radius )
			{
				clip |= 8;
			}
			if (  dist[2] < -radius )
			{
				clip |= 16;
			}
			else if ( dist[2] > radius )
			{
				clip |= 32;
			}

			clipBits[i] = clip;
			clipall &= clip;
		}
		if ( clipall )
		{
			continue;	// this surface doesn't have any of this light
		}
		floatColor[0] = dl->color[0] * 255.0f;
		floatColor[1] = dl->color[1] * 255.0f;
		floatColor[2] = dl->color[2] * 255.0f;
		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 )
		{
			int		a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i+1];
			c = tess.indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] )
			{
				continue;	// not lighted
			}

			// copy the vertex positions
			VectorCopy(tess.xyz[a],posa);
			VectorCopy(tess.xyz[b],posb);
			VectorCopy(tess.xyz[c],posc);

			VectorSubtract( posa, posb,e1);
			VectorSubtract( posc, posb,e2);
			CrossProduct(e1,e2,normal);
//			fac=DotProduct(normal,origin)-DotProduct(normal,posa);
//			if (fac <= 0.0f || // backface
// rjr - removed for hacking 			if ( (!r_dlightBacks->integer && DotProduct(normal,origin)-DotProduct(normal,posa) <= 0.0f) || // backface
			if ( DotProduct(normal,origin)-DotProduct(normal,posa) <= 0.0f || // backface
				DotProduct(normal,normal) < 1E-8f) // junk triangle
			{
				continue;
			}
			VectorNormalize(normal);
			fac=DotProduct(normal,origin)-DotProduct(normal,posa);
			if (fac >= radius)  // out of range
			{
				continue;
			}
			modulate = 1.0f-((fac*fac) / (radius*radius));
			fac = 0.5f/sqrtf(radius*radius - fac*fac);

			// save the verts
			VectorCopy(posa,vertCoordsArray[numIndexes]);
			VectorCopy(posb,vertCoordsArray[numIndexes+1]);
			VectorCopy(posc,vertCoordsArray[numIndexes+2]);

			// now we need e1 and e2 to be an orthonormal basis
			if (DotProduct(e1,e1) > DotProduct(e2,e2))
			{
				VectorNormalize(e1);
				CrossProduct(e1,normal,e2);
			}
			else
			{
				VectorNormalize(e2);
				CrossProduct(normal,e2,e1);
			}
			VectorScale(e1,fac,e1);
			VectorScale(e2,fac,e2);

			VectorSubtract( posa, origin,dist);
			texCoordsArray[numIndexes][0]=DotProduct(dist,e1)+0.5f;
			texCoordsArray[numIndexes][1]=DotProduct(dist,e2)+0.5f;

			VectorSubtract( posb, origin,dist);
			texCoordsArray[numIndexes+1][0]=DotProduct(dist,e1)+0.5f;
			texCoordsArray[numIndexes+1][1]=DotProduct(dist,e2)+0.5f;

			VectorSubtract( posc, origin,dist);
			texCoordsArray[numIndexes+2][0]=DotProduct(dist,e1)+0.5f;
			texCoordsArray[numIndexes+2][1]=DotProduct(dist,e2)+0.5f;

			if ((texCoordsArray[numIndexes][0] < 0.0f && texCoordsArray[numIndexes+1][0] < 0.0f && texCoordsArray[numIndexes+2][0] < 0.0f) ||
				(texCoordsArray[numIndexes][0] > 1.0f && texCoordsArray[numIndexes+1][0] > 1.0f && texCoordsArray[numIndexes+2][0] > 1.0f) ||
				(texCoordsArray[numIndexes][1] < 0.0f && texCoordsArray[numIndexes+1][1] < 0.0f && texCoordsArray[numIndexes+2][1] < 0.0f) ||
				(texCoordsArray[numIndexes][1] > 1.0f && texCoordsArray[numIndexes+1][1] > 1.0f && texCoordsArray[numIndexes+2][1] > 1.0f) )
			{
				continue; // didn't end up hitting this tri
			}

			// these are the old texture coordinates for the multitexture dlight

			/* old code, get from the svars = wrong
			oldTexCoordsArray[numIndexes][0]=tess.svars.texcoords[0][a][0];
			oldTexCoordsArray[numIndexes][1]=tess.svars.texcoords[0][a][1];
			oldTexCoordsArray[numIndexes+1][0]=tess.svars.texcoords[0][b][0];
			oldTexCoordsArray[numIndexes+1][1]=tess.svars.texcoords[0][b][1];
			oldTexCoordsArray[numIndexes+2][0]=tess.svars.texcoords[0][c][0];
			oldTexCoordsArray[numIndexes+2][1]=tess.svars.texcoords[0][c][1];
			*/
			oldTexCoordsArray[numIndexes][0]=tess.texCoords[a][0][0];
			oldTexCoordsArray[numIndexes][1]=tess.texCoords[a][0][1];
			oldTexCoordsArray[numIndexes+1][0]=tess.texCoords[b][0][0];
			oldTexCoordsArray[numIndexes+1][1]=tess.texCoords[b][0][1];
			oldTexCoordsArray[numIndexes+2][0]=tess.texCoords[c][0][0];
			oldTexCoordsArray[numIndexes+2][1]=tess.texCoords[c][0][1];

			colorTemp[0] = Q_ftol(floatColor[0] * modulate);
			colorTemp[1] = Q_ftol(floatColor[1] * modulate);
			colorTemp[2] = Q_ftol(floatColor[2] * modulate);
			colorTemp[3] = 255;

			byteAlias_t *ba = (byteAlias_t *)&colorTemp;
			colorArray[numIndexes + 0] = ba->ui;
			colorArray[numIndexes + 1] = ba->ui;
			colorArray[numIndexes + 2] = ba->ui;

			hitIndexes[numIndexes] = numIndexes;
			hitIndexes[numIndexes+1] = numIndexes+1;
			hitIndexes[numIndexes+2] = numIndexes+2;
			numIndexes += 3;

			if (numIndexes>=SHADER_MAX_VERTEXES-3)
			{
				break; // we are out of space, so we are done :)
			}
		}

		if ( !numIndexes ) {
			continue;
		}
#ifndef JK2_MODE
		//don't have fog enabled when we redraw with alpha test, or it will double over
		//and screw the tri up -rww
		if (r_drawfog->value == 2 &&
			tr.world &&
			(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs))
		{
			fogging = qglIsEnabled(GL_FOG);

			if (fogging)
			{
				qglDisable(GL_FOG);
			}
		}
		else
		{
			fogging = 0;
		}
#endif

		if (!needResetVerts)
		{
			needResetVerts=1;
			if (qglUnlockArraysEXT)
			{
				qglUnlockArraysEXT();
				GLimp_LogComment( "glUnlockArraysEXT\n" );
			}
		}

		dStage = NULL;
		if (tess.shader)
		{
			int i = 0;
			while (i < tess.shader->numUnfoggedPasses)
			{
				const int blendBits = (GLS_SRCBLEND_BITS+GLS_DSTBLEND_BITS);
				if (((tess.shader->stages[i].shaderData.bundle[0].cppData.image && !tess.shader->stages[i].shaderData.bundle[0].isLightmap && !tess.shader->stages[i].shaderData.bundle[0].numTexMods && tess.shader->stages[i].shaderData.bundle[0].tcGen != texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED && tess.shader->stages[i].shaderData.bundle[0].tcGen != texCoordGen_t::TCGEN_FOG) ||
					 (tess.shader->stages[i].shaderData.bundle[1].cppData.image && !tess.shader->stages[i].shaderData.bundle[1].isLightmap && !tess.shader->stages[i].shaderData.bundle[1].numTexMods && tess.shader->stages[i].shaderData.bundle[1].tcGen != texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED && tess.shader->stages[i].shaderData.bundle[1].tcGen != texCoordGen_t::TCGEN_FOG)) &&
					(tess.shader->stages[i].stateBits & blendBits) == 0 )
				{ //only use non-lightmap opaque stages
                    dStage = &tess.shader->stages[i];
					break;
				}
				i++;
			}
		}

		if (dStage)
		{
			if( dStage->shaderData.bundle[0].cppData.image && !dStage->shaderData.bundle[0].isLightmap && !dStage->shaderData.bundle[0].numTexMods && dStage->shaderData.bundle[0].tcGen != texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED && dStage->shaderData.bundle[0].tcGen != texCoordGen_t::TCGEN_FOG )
			{
				R_BindAnimatedImage( &dStage->shaderData.bundle[0], 0 );
			}
			else
			{
				R_BindAnimatedImage( &dStage->shaderData.bundle[1], 0 );
			}

			VK_BindImage( tr.dlightImage, 1 );
			GL_TexEnv( GL_MODULATE );

			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);// | GLS_ATEST_GT_0);

			vkCmdDrawIndexed( backEndData->cmdbuf, numIndexes, 1, 0, 0, 0 );
		}
		else
		{
			VK_BindImage( tr.dlightImage, 0 );
			// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
			// where they aren't rendered
			//if ( dl->additive ) {
			//	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			//}
			//else
			{
				GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			}

			vkCmdDrawIndexed( backEndData->cmdbuf, numIndexes, 1, 0, 0, 0 );
		}

#ifndef JK2_MODE
		if (fogging)
		{
			qglEnable(GL_FOG);
		}
#endif

		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
	if (needResetVerts)
	{
		qglVertexPointer (3, GL_FLOAT, 16, tess.xyz);	// padded for SIMD
		if (qglLockArraysEXT)
		{
			qglLockArraysEXT(0, tess.numVertexes);
			GLimp_LogComment( "glLockArraysEXT\n" );
		}
	}
}
static void ProjectDlightTexture( void ) {
	int		i, l;
	vec3_t	origin;
	float	*texCoords;
	byte	*colors;
	byte	clipBits[SHADER_MAX_VERTEXES];
	float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	byte	colorArray[SHADER_MAX_VERTEXES][4];
	uint32_t	hitIndexes[SHADER_MAX_INDEXES];
	int		numIndexes;
	float	scale;
	float	radius;
#ifndef JK2_MODE
	int		fogging;
#endif
	vec3_t	floatColor;
	shaderStage_t *dStage;

	if ( !backEnd.refdef.num_dlights ) {
		return;
	}

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}

		texCoords = texCoordsArray[0];
		colors = colorArray[0];

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;
		scale = 1.0f / radius;

		floatColor[0] = dl->color[0] * 255.0f;
		floatColor[1] = dl->color[1] * 255.0f;
		floatColor[2] = dl->color[2] * 255.0f;

		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords += 2, colors += 4 ) {
			vec3_t	dist;
			int		clip;
			float	modulate;

			backEnd.pc.c_dlightVertexes++;

			VectorSubtract( origin, tess.xyz[i], dist );

			int l = 1;
			int bestIndex = 0;
			float greatest = tess.normal[i][0];
			if (greatest < 0.0f)
			{
				greatest = -greatest;
			}

			if (VectorCompare(tess.normal[i], vec3_origin))
			{ //damn you terrain!
				bestIndex = 2;
			}
			else
			{
				while (l < 3)
				{
					if ((tess.normal[i][l] > greatest && tess.normal[i][l] > 0.0f) ||
						(tess.normal[i][l] < -greatest && tess.normal[i][l] < 0.0f))
					{
						greatest = tess.normal[i][l];
						if (greatest < 0.0f)
						{
							greatest = -greatest;
						}
						bestIndex = l;
					}
					l++;
				}
			}

			float dUse = 0.0f;
			const float maxScale = 1.5f;
			const float maxGroundScale = 1.4f;
			const float lightScaleTolerance = 0.1f;

			if (bestIndex == 2)
			{
				dUse = origin[2]-tess.xyz[i][2];
				if (dUse < 0.0f)
				{
					dUse = -dUse;
				}
				dUse = (radius*0.5f)/dUse;
				if (dUse > maxGroundScale)
				{
					dUse = maxGroundScale;
				}
				else if (dUse < 0.1f)
				{
					dUse = 0.1f;
				}

				if (VectorCompare(tess.normal[i], vec3_origin) ||
					tess.normal[i][0] > lightScaleTolerance ||
					tess.normal[i][0] < -lightScaleTolerance ||
					tess.normal[i][1] > lightScaleTolerance ||
					tess.normal[i][1] < -lightScaleTolerance)
				{ //if not perfectly flat, we must use a constant dist
					scale = 1.0f / radius;
				}
				else
				{
					scale = 1.0f / (radius*dUse);
				}

				texCoords[0] = 0.5f + dist[0] * scale;
				texCoords[1] = 0.5f + dist[1] * scale;
			}
			else if (bestIndex == 1)
			{
				dUse = origin[1]-tess.xyz[i][1];
				if (dUse < 0.0f)
				{
					dUse = -dUse;
				}
				dUse = (radius*0.5f)/dUse;
				if (dUse > maxScale)
				{
					dUse = maxScale;
				}
				else if (dUse < 0.1f)
				{
					dUse = 0.1f;
				}
				if (tess.normal[i][0] > lightScaleTolerance ||
					tess.normal[i][0] < -lightScaleTolerance ||
					tess.normal[i][2] > lightScaleTolerance ||
					tess.normal[i][2] < -lightScaleTolerance)
				{ //if not perfectly flat, we must use a constant dist
					scale = 1.0f / radius;
				}
				else
				{
					scale = 1.0f / (radius*dUse);
				}

				texCoords[0] = 0.5f + dist[0] * scale;
				texCoords[1] = 0.5f + dist[2] * scale;
			}
			else
			{
				dUse = origin[0]-tess.xyz[i][0];
				if (dUse < 0.0f)
				{
					dUse = -dUse;
				}
				dUse = (radius*0.5f)/dUse;
				if (dUse > maxScale)
				{
					dUse = maxScale;
				}
				else if (dUse < 0.1f)
				{
					dUse = 0.1f;
				}
				if (tess.normal[i][2] > lightScaleTolerance ||
					tess.normal[i][2] < -lightScaleTolerance ||
					tess.normal[i][1] > lightScaleTolerance ||
					tess.normal[i][1] < -lightScaleTolerance)
				{ //if not perfectly flat, we must use a constant dist
					scale = 1.0f / radius;
				}
				else
				{
					scale = 1.0f / (radius*dUse);
				}

				texCoords[0] = 0.5f + dist[1] * scale;
				texCoords[1] = 0.5f + dist[2] * scale;
			}

			clip = 0;
			if ( texCoords[0] < 0.0f ) {
				clip |= 1;
			} else if ( texCoords[0] > 1.0f ) {
				clip |= 2;
			}
			if ( texCoords[1] < 0.0f ) {
				clip |= 4;
			} else if ( texCoords[1] > 1.0f ) {
				clip |= 8;
			}
			// modulate the strength based on the height and color
			if ( dist[bestIndex] > radius ) {
				clip |= 16;
				modulate = 0.0f;
			} else if ( dist[bestIndex] < -radius ) {
				clip |= 32;
				modulate = 0.0f;
			} else {
				dist[bestIndex] = Q_fabs(dist[bestIndex]);
				if ( dist[bestIndex] < radius * 0.5f ) {
					modulate = 1.0f;
				} else {
					modulate = 2.0f * (radius - dist[bestIndex]) * scale;
				}
			}
			clipBits[i] = clip;

			colors[0] = Q_ftol(floatColor[0] * modulate);
			colors[1] = Q_ftol(floatColor[1] * modulate);
			colors[2] = Q_ftol(floatColor[2] * modulate);
			colors[3] = 255;
		}
		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			int		a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i+1];
			c = tess.indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}

#ifndef JK2_MODE
		//don't have fog enabled when we redraw with alpha test, or it will double over
		//and screw the tri up -rww
		if (r_drawfog->value == 2 &&
			tr.world &&
			(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs))
		{
			fogging = qglIsEnabled(GL_FOG);

			if (fogging)
			{
				qglDisable(GL_FOG);
			}
		}
		else
		{
			fogging = 0;
		}
#endif


		dStage = NULL;
		if (tess.shader)
		{
			int i = 0;
			while (i < tess.shader->numUnfoggedPasses)
			{
				const int blendBits = (GLS_SRCBLEND_BITS+GLS_DSTBLEND_BITS);
				if (((tess.shader->stages[i].shaderData.bundle[0].cppData.image && !tess.shader->stages[i].shaderData.bundle[0].isLightmap && !tess.shader->stages[i].shaderData.bundle[0].numTexMods && tess.shader->stages[i].shaderData.bundle[0].tcGen != texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED && tess.shader->stages[i].shaderData.bundle[0].tcGen != texCoordGen_t::TCGEN_FOG) ||
					 (tess.shader->stages[i].shaderData.bundle[1].cppData.image && !tess.shader->stages[i].shaderData.bundle[1].isLightmap && !tess.shader->stages[i].shaderData.bundle[1].numTexMods && tess.shader->stages[i].shaderData.bundle[1].tcGen != texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED && tess.shader->stages[i].shaderData.bundle[1].tcGen != texCoordGen_t::TCGEN_FOG)) &&
					(tess.shader->stages[i].stateBits & blendBits) == 0 )
				{ //only use non-lightmap opaque stages
					dStage = &tess.shader->stages[i];
					break;
				}
				i++;
			}
		}

		if (dStage)
		{
			if( dStage->shaderData.bundle[0].cppData.image && !dStage->shaderData.bundle[0].isLightmap && !dStage->shaderData.bundle[0].numTexMods && dStage->shaderData.bundle[0].tcGen != texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED && dStage->shaderData.bundle[0].tcGen != texCoordGen_t::TCGEN_FOG )
			{
				R_BindAnimatedImage( &dStage->shaderData.bundle[0], 0 );
			}
			else
			{
				R_BindAnimatedImage( &dStage->shaderData.bundle[1], 0 );
			}

			VK_BindImage( tr.dlightImage, 1 );
			GL_TexEnv( GL_MODULATE );

			GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL);// | GLS_ATEST_GT_0);

			vkCmdDrawIndexed( backEndData->cmdbuf, numIndexes, 1, 0, 0, 0 );
		}
		else
		{
			VK_BindImage( tr.dlightImage, 0 );
			// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
			// where they aren't rendered
			//if ( dl->additive ) {
			//	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			//}
			//else
			{
				GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
			}

			vkCmdDrawIndexed( backEndData->cmdbuf, numIndexes, 1, 0, 0, 0 );
		}

#ifndef JK2_MODE
		if (fogging)
		{
			qglEnable(GL_FOG);
		}
#endif

		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
}
#endif

/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass( void ) {
#if 0
	fog_t		*fog;
	int			i;

	fog = tr.world->fogs + tess.fogNum;

	for ( i = 0; i < tess.numVertexes; i++ ) {
		byteAlias_t *ba = (byteAlias_t *)&tess.svars.colors[i];
		ba->i = fog->colorInt;
	}

	RB_CalcFogTexCoords( ( float * ) tess.svars.texcoords[0] );

	VK_BindImage( tr.fogImage );

	if ( tess.shader->fogPass == FP_EQUAL ) {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	}

	R_DrawElements( &tess );
#endif
}


/*
** RB_IterateStagesGeneric
*/
#ifndef JK2_MODE
static vec4_t	GLFogOverrideColors[GLFOGOVERRIDE_MAX] =
{
	{ 0.0, 0.0, 0.0, 1.0 },	// GLFOGOVERRIDE_NONE
	{ 0.0, 0.0, 0.0, 1.0 },	// GLFOGOVERRIDE_BLACK
	{ 1.0, 1.0, 1.0, 1.0 }	// GLFOGOVERRIDE_WHITE
};

static const float logtestExp2 = (sqrt( -log( 1.0 / 255.0 ) ));
#endif
extern bool tr_stencilled; //tr_backend.cpp
static void RB_IterateStagesGeneric( shaderCommands_t *input ) {
	bool pipelineBound = ( backEndData->pipeline == input->shader->pipeline );
	int stage;
#ifndef JK2_MODE
	bool	UseGLFog = false;
	bool	FogColorChange = false;
	fog_t	*fog = NULL;

	if (tess.fogNum && tess.shader->fogPass && (tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs)
		&& r_drawfog->value == 2)
	{	// only gl fog global fog and the "special fog"
		fog = tr.world->fogs + tess.fogNum;

		if (tr.rangedFog)
		{ //ranged fog, used for sniper scope
			float fStart = fog->parms.depthForOpaque;
			float fEnd = tr.distanceCull;

			if (tr.rangedFog < 0.0f)
			{ //special designer override
				fStart = -tr.rangedFog;
				fEnd = fog->parms.depthForOpaque;

				if (fStart >= fEnd)
				{
					fStart = fEnd-1.0f;
				}
			}
			else
			{
				//the greater tr.rangedFog is, the more fog we will get between the view point and cull distance
				if ((tr.distanceCull-fStart) < tr.rangedFog)
				{ //assure a minimum range between fog beginning and cutoff distance
					fStart = tr.distanceCull-tr.rangedFog;

					if (fStart < 16.0f)
					{
						fStart = 16.0f;
					}
				}
			}

			qglFogi(GL_FOG_MODE, GL_LINEAR);
			qglFogf(GL_FOG_START, fStart);
			qglFogf(GL_FOG_END, fEnd);
		}
		else
		{
			qglFogi(GL_FOG_MODE, GL_EXP2);
			qglFogf(GL_FOG_DENSITY, logtestExp2 / fog->parms.depthForOpaque);
		}

		if ( g_bRenderGlowingObjects )
		{
			const float fogColor[3] = { 0.0f, 0.0f, 0.0f };
			qglFogfv(GL_FOG_COLOR, fogColor );
		}
		else
		{
			qglFogfv(GL_FOG_COLOR, fog->parms.color);
		}

		qglEnable(GL_FOG);
		UseGLFog = true;
	}
#endif

	for ( stage = 0; stage < input->shader->numUnfoggedPasses; stage++ )
	{
		shaderStage_t *pStage = &tess.xstages[stage];
		if ( !pStage->active )
		{
			break;
		}

		// Reject this stage if it's not a glow stage but we are doing a glow pass.
		if ( g_bRenderGlowingObjects && !pStage->glow )
		{
			continue;
		}

		int	stateBits = pStage->stateBits;
		alphaGen_t	forceAlphaGen = (alphaGen_t)0;
		colorGen_t	forceRGBGen = (colorGen_t)0;

		// allow skipping out to show just lightmaps during development
		if ( stage && r_lightmap->integer)
		{
			if ( !pStage->isLightmap )
			{
				continue;	// need to keep going in case the LM is in a later stage
			}
			else
			{
				stateBits = (GLS_DSTBLEND_ZERO | GLS_SRCBLEND_ONE);	//we want to replace the prior stages with this LM, not blend
			}
		}

		if ( backEnd.currentEntity )
		{
			if ( backEnd.currentEntity->e.renderfx & RF_DISINTEGRATE1 )
			{
				// we want to be able to rip a hole in the thing being disintegrated, and by doing the depth-testing it avoids some kinds of artefacts, but will probably introduce others?
				//	NOTE: adjusting the alphaFunc seems to help a bit
				stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHMASK_TRUE | GLS_ATEST_GE_C0;
			}

			if ( backEnd.currentEntity->e.renderfx & RF_ALPHA_FADE )
			{
				if ( backEnd.currentEntity->e.shaderRGBA[3] < 255 )
				{
					stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
					forceAlphaGen = alphaGen_t::AGEN_ENTITY;
				}
			}

			if ( backEnd.currentEntity->e.renderfx & RF_RGB_TINT )
			{//want to use RGBGen from ent
				forceRGBGen = colorGen_t::CGEN_ENTITY;
			}
		}

		if (pStage->ss && pStage->ss->surfaceSpriteType)
		{
			// We check for surfacesprites AFTER drawing everything else
			continue;
		}

#ifndef JK2_MODE
		if (UseGLFog)
		{
			if (pStage->mGLFogColorOverride)
			{
				qglFogfv(GL_FOG_COLOR, GLFogOverrideColors[pStage->mGLFogColorOverride]);
				FogColorChange = true;
			}
			else if (FogColorChange && fog)
			{
				FogColorChange = false;
				qglFogfv(GL_FOG_COLOR, fog->parms.color);
			}
		}
#endif

#if 0
		if (!input->fading)
		{ //this means ignore this, while we do a fade-out
			ComputeColors( pStage, forceAlphaGen, forceRGBGen );
		}
		ComputeTexCoords( pStage );

		if ( !setArraysOnce )
		{
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars.colors );
		}
#endif

		if (pStage->shaderData.bundle[0].isLightmap && r_debugStyle->integer >= 0)
		{
			if (pStage->lightmapStyle != r_debugStyle->integer)
			{
				if (pStage->lightmapStyle == 0)
				{
					//GL_State( GLS_DSTBLEND_ZERO | GLS_SRCBLEND_ZERO );
					R_DrawElements( input );
				}
				continue;
			}
		}

		{
			static bool lStencilled = false;

			//
			// set state
			//
			if ( (tess.shader == tr.distortionShader) ||
				 (backEnd.currentEntity && (backEnd.currentEntity->e.renderfx & RF_DISTORTION)) )
			{ //special distortion effect -rww
				//tr.screenImage should have been set for this specific entity before we got in here.
				VK_BindImage( tr.screenImage );
			}
			else if ( pStage->shaderData.bundle[0].vertexLightmap && ( r_vertexLight->integer ) && r_lightmap->integer )
			{
				VK_BindImage( tr.whiteImage );
			}
			else
				R_BindAnimatedImage( &pStage->shaderData.bundle[0] );

			if (tess.shader == tr.distortionShader &&
				glConfig.stencilBits >= 4)
			{ //draw it to the stencil buffer!
				tr_stencilled = true;
				lStencilled = true;
#if 0
				qglEnable(GL_STENCIL_TEST);
				qglStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF);
				qglStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
				qglColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

				//don't depthmask, don't blend.. don't do anything
				GL_State(0);
#endif
			}
			else if (backEnd.currentEntity && (backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA))
			{
				//ForceAlpha((unsigned char *) tess.svars.colors, backEnd.currentEntity->e.shaderRGBA[3]);
				//GL_State(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
			}
			else
			{
				//GL_State( stateBits );
			}

			// bind the pipeline
			if( !pipelineBound ) {
				RB_SetShader( input->shader );
				pipelineBound = true;
			}

			// draw
			R_DrawElements( input );
		}
	}
#ifndef JK2_MODE
	if (FogColorChange)
	{
		qglFogfv(GL_FOG_COLOR, fog->parms.color);
	}
#endif
}

/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input;
	int stage;

	input = &tess;

#if 0
	RB_DeformTessGeometry();
#endif

	//
	// log this call
	//
	if ( r_logFile->integer )
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name) );
	}

	RB_SetShader( input->shader );

	//
	// call shader function
	//
	RB_IterateStagesGeneric( input );

#if 0
	//
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE
		&& !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) ) {
		if (r_dlightStyle->integer>0)
		{
			ProjectDlightTexture2();
		}
		else
		{
			ProjectDlightTexture();
		}
	}
#endif

	//
	// now do fog
	//
#ifdef JK2_MODE
	if (tess.fogNum && tess.shader->fogPass && r_drawfog->value)
#else
	if (tr.world && (tess.fogNum != tr.world->globalFog || r_drawfog->value != 2) && r_drawfog->value && tess.fogNum && tess.shader->fogPass)
#endif
	{
		RB_FogPass();
	}

	// Now check for surfacesprites.
	if (r_surfaceSprites->integer)
	{
		for ( stage = 1; stage < tess.shader->numUnfoggedPasses; stage++ )
		{
			if (tess.xstages[stage].ss && tess.xstages[stage].ss->surfaceSpriteType)
			{	// Draw the surfacesprite
				RB_DrawSurfaceSprites( &tess.xstages[stage], input);
			}
		}
	}

#ifndef JK2_MODE
	//don't disable the hardware fog til after we do surface sprites
	if (r_drawfog->value == 2 &&
		tess.fogNum && tess.shader->fogPass &&
		(tess.fogNum == tr.world->globalFog || tess.fogNum == tr.world->numfogs))
	{
		qglDisable(GL_FOG);
	}
#endif
}


/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	shaderCommands_t *input;

	input = &tess;

	if (input->numDraws == 0) {
		return;
	}

	if ( tess.shader == tr.shadowShader ) {
		RB_ShadowTessEnd();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		return;
	}

	if ( skyboxportal )
	{
		// world
		if(!(backEnd.refdef.rdflags & RDF_SKYBOXPORTAL))
		{
			if(tess.currentStageIteratorFunc == RB_StageIteratorSky)
			{	// don't process these tris at all
				return;
			}
		}
		// portal sky
		else
		{
			if(!drawskyboxportal)
			{
				if( !(tess.currentStageIteratorFunc == RB_StageIteratorSky))
				{	// /only/ process sky tris
					return;
				}
			}
		}
	}

	//
	// update performance counters
	//
	if (!backEnd.projection2D)
	{
		backEnd.pc.c_shaders++;

		for( int i = 0; i < tess.numDraws; ++i ) {
			vertexBuffer_t *draw = tess.draws[i];

			backEnd.pc.c_vertexes += draw->numVertexes;
			backEnd.pc.c_indexes += draw->numIndexes;
			backEnd.pc.c_totalIndexes += draw->numIndexes * tess.numPasses;
#ifdef JK2_MODE
			if( tess.fogNum && tess.shader->fogPass && r_drawfog->value )
#else
			if( tess.fogNum && tess.shader->fogPass && r_drawfog->value == 1 )
#endif
			{ // Fogging adds an additional pass
				backEnd.pc.c_totalIndexes += draw->numIndexes;
			}
		}
	}

	//
	// call off to shader specific tess end function
	//
	tess.currentStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if ( r_showtris->integer )
	{
		DrawTris (input);
	}

#if 0
	if ( r_shownormals->integer ) {
		DrawNormals (input);
	}
#endif

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numDraws = 0;

	GLimp_LogComment( "----------\n" );
}
