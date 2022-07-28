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

// tr_draw.c
#include "../server/exe_headers.h"
#include "tr_common.h"
#include "tr_local.h"

#include "tr_dissolve_VS.h"
#include "tr_dissolve_PS.h"


static void RB_UploadCinematic( int cols, int rows, const byte *data, int &client, qboolean dirty ) {
	image_t *image = tr.scratchImage[client];

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if( cols != image->width || rows != image->height ) {

		// if the image has been used recently, it can't be destroyed yet
		// use any other available client
		if( image->iLastFrameUsedOn > tr.frameCount - vkState.imgcount ) {
			int i;

			image = NULL;
			for( i = 0; i < NUM_SCRATCH_IMAGES; ++i ) {
				if( ( cols == tr.scratchImage[i]->width ) &&
					( rows == tr.scratchImage[i]->height ) &&
					( tr.scratchImage[i]->iLastFrameUsedOn > tr.frameCount - vkState.imgcount ) ) {
					image = tr.scratchImage[i];
					client = i;
					break;
				}
			}
		}

		if( !image ) {
			Com_Error( ERR_DROP, "RE_UploadCinematic: out of scratch images\n" );
		}

		R_ResizeImage( image, cols, rows );
	}

	if( dirty ) {
		VK_UploadImage( image, data, cols, rows, 0 );
	}
}

void RE_UploadCinematic( int cols, int rows, const byte *data, int client, qboolean dirty ) {
	RB_UploadCinematic( cols, rows, data, client, dirty );
}

/*
=============
RE_StretchRaw

Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/

// param 'bDirty' should be true 99% of the time
void RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, const byte *data, int iClient, qboolean bDirty ) {
	image_t *scratchImage;
	frameBuffer_t *frameBuffer;
	float xscale, yscale;

	if( !tr.registered ) {
		return;
	}

	frameBuffer = backEndData->frameBuffer;

	R_IssuePendingRenderCommands();
	R_BindFrameBuffer( NULL );

	// make sure rows and cols are powers of 2
	if ( (cols&(cols-1)) || (rows&(rows-1)) ) {
		Com_Error( ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows );
	}

	RB_UploadCinematic( cols, rows, data, iClient, bDirty );
	scratchImage = tr.scratchImage[iClient];

	VK_SetImageLayout( scratchImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT );
	VK_SetImageLayout( tr.screenImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT );

	if( frameBuffer ) {
		image_t *src = frameBuffer->images[0].i;
		VK_SetImageLayout( src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT );

		VkImageCopy imageCopy = {};
		imageCopy.extent.width = tr.screenImage->width;
		imageCopy.extent.height = tr.screenImage->height;
		imageCopy.extent.depth = 1;
		imageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopy.srcSubresource.layerCount = 1;
		imageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopy.dstSubresource.layerCount = 1;

		vkCmdCopyImage( backEndData->cmdbuf, src->tex, src->layout, tr.screenImage->tex, tr.screenImage->layout, 1, &imageCopy );
	}

	// convert the screen coordinates. x,y,w,h are normalized to 640x480
	xscale = ( tr.screenImage->width / 640.f );
	yscale = ( tr.screenImage->height / 480.f );

	// blit the the uploaded image on the screen
	VkImageBlit imageBlit = {};
	imageBlit.srcOffsets[1].x = cols;
	imageBlit.srcOffsets[1].y = rows;
	imageBlit.srcOffsets[1].z = 1;
	imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBlit.srcSubresource.layerCount = 1;
	imageBlit.dstOffsets[0].x = (int)( x * xscale );
	imageBlit.dstOffsets[0].y = (int)( y * yscale );
	imageBlit.dstOffsets[1].x = (int)( (x + w) * xscale );
	imageBlit.dstOffsets[1].y = (int)( (y + h) * yscale );
	imageBlit.dstOffsets[1].z = 1;
	imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBlit.dstSubresource.layerCount = 1;

	vkCmdBlitImage( backEndData->cmdbuf, scratchImage->tex, scratchImage->layout, tr.screenImage->tex, tr.screenImage->layout, 1, &imageBlit, VK_FILTER_LINEAR );
}

extern byte *RB_ReadPixels(int x, int y, int width, int height, size_t *offset, int *padlen);
void RE_GetScreenShot(byte *buffer, int w, int h)
{
	byte		*source;
	byte		*src, *dst;
	size_t offset = 0, memcount;
	int padlen;

	int			x, y;
	int			r, g, b;
	float		xScale, yScale;
	int			xx, yy;


	source = RB_ReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, &offset, &padlen);
	memcount = (glConfig.vidWidth * 3 + padlen) * glConfig.vidHeight;

	// gamma correct
	if(glConfig.deviceSupportsGamma)
		R_GammaCorrect(source + offset, (int)memcount);

	// resample from source
	xScale = glConfig.vidWidth / (4.0*w);
	yScale = glConfig.vidHeight / (3.0*h);
	for ( y = 0 ; y < h ; y++ ) {
		for ( x = 0 ; x < w ; x++ ) {
			r = g = b = 0;
			for ( yy = 0 ; yy < 3 ; yy++ ) {
				for ( xx = 0 ; xx < 4 ; xx++ ) {
					src = source + offset + 3 * ( glConfig.vidWidth * (int)( (y*3+yy)*yScale ) + (int)( (x*4+xx)*xScale ) );
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 3 * ( y * w + x );
			dst[0] = r / 12;
			dst[1] = g / 12;
			dst[2] = b / 12;
		}
	}

	R_Free(source);
}

// this is just a chunk of code from RE_TempRawImage_ReadFromFile() below, subroutinised so I can call it
//	from the screen dissolve code as well...
//
static byte *RE_ReSample(byte *pbLoadedPic,			int iLoadedWidth,	int iLoadedHeight,
						 byte *pbReSampleBuffer,	int *piWidth,		int *piHeight
						)
{
	byte *pbReturn = NULL;

	// if not resampling, just return some values and return...
	//
	if ( pbReSampleBuffer == NULL || (iLoadedWidth == *piWidth && iLoadedHeight == *piHeight) )
	{
		// if not resampling, we're done, just return the loaded size...
		//
		*piWidth = iLoadedWidth;
		*piHeight= iLoadedHeight;
		pbReturn = pbLoadedPic;
	}
	else
	{
		// resample from pbLoadedPic to pbReSampledBuffer...
		//
		float	fXStep = (float)iLoadedWidth / (float)*piWidth;
		float	fYStep = (float)iLoadedHeight/ (float)*piHeight;
		int		iTotPixelsPerDownSample = (int)ceil(fXStep) * (int)ceil(fYStep);

		int 	r,g,b;

		byte	*pbDst = pbReSampleBuffer;

		for ( int y=0; y<*piHeight; y++ )
		{
			for ( int x=0; x<*piWidth; x++ )
			{
				r=g=b=0;

				for ( float yy = (float)y*fYStep; yy < (float)(y+1)*fYStep ; yy+=1 )
				{
					for ( float xx = (float)x*fXStep; xx < (float)(x+1)*fXStep ; xx+=1 )
					{
						byte *pbSrc = pbLoadedPic + 4 * ( ((int)yy * iLoadedWidth) + (int)xx );

						assert(pbSrc < pbLoadedPic + (iLoadedWidth * iLoadedHeight * 4) );

						r += pbSrc[0];
						g += pbSrc[1];
						b += pbSrc[2];
					}
				}

				assert(pbDst < pbReSampleBuffer + (*piWidth * *piHeight * 4));

				pbDst[0] = r / iTotPixelsPerDownSample;
				pbDst[1] = g / iTotPixelsPerDownSample;
				pbDst[2] = b / iTotPixelsPerDownSample;
				pbDst[3] = 255;
				pbDst += 4;
			}
		}

		// set return value...
		//
		pbReturn = pbReSampleBuffer;
	}

	return pbReturn;
}


// this is so the server (or anyone else) can get access to raw pixels if they really need to,
//	currently it's only used by the server so that savegames can embed a graphic in the auto-save files
//	(which can't do a screenshot since they're saved out before the level is drawn).
//
// by default, the pic will be returned as the original dims, but if pbReSampleBuffer != NULL then it's assumed to
//	be a big enough buffer to hold the resampled image, which also means that the width and height params are read as
//	inputs (as well as still being inherently outputs) and the pic is scaled to that size, and to that buffer.
//
// the return value is either NULL, or a pointer to the pixels to use (which may be either the pbReSampleBuffer param,
//	or the local ptr below).
//
// In either case, you MUST call the free-up function afterwards ( RE_TempRawImage_CleanUp() ) to get rid of any temp
//	memory after you've finished with the pic.
//
// Note: ALWAYS use the return value if != NULL, even if you passed in a declared resample buffer. This is because the
//	resample will get skipped if the values you want are the same size as the pic that it loaded, so it'll return a
//	different buffer.
//
// the vertflip param is used for those functions that expect things in OpenGL's upside-down pixel-read format (sigh)
//
// (not brilliantly fast, but it's only used for weird stuff anyway)
//
byte* pbLoadedPic = NULL;

byte* RE_TempRawImage_ReadFromFile(const char *psLocalFilename, int *piWidth, int *piHeight, byte *pbReSampleBuffer, qboolean qbVertFlip)
{
	RE_TempRawImage_CleanUp();	// jic

	byte *pbReturn = NULL;

	if (psLocalFilename && piWidth && piHeight)
	{
		int	 iLoadedWidth, iLoadedHeight;

		R_LoadImage( psLocalFilename, &pbLoadedPic, &iLoadedWidth, &iLoadedHeight);
		if ( pbLoadedPic )
		{
			pbReturn = RE_ReSample(	pbLoadedPic,		iLoadedWidth,	iLoadedHeight,
									pbReSampleBuffer,	piWidth,		piHeight);
		}
	}

	if (pbReturn && qbVertFlip)
	{
		unsigned int *pSrcLine = (unsigned int *) pbReturn;
		unsigned int *pDstLine = (unsigned int *) pbReturn + (*piHeight * *piWidth );	// *4 done by compiler (longs)
					   pDstLine-= *piWidth;	// point at start of last line, not first after buffer

		for (int iLineCount=0; iLineCount<*piHeight/2; iLineCount++)
		{
			for (int x=0; x<*piWidth; x++)
			{
				unsigned int l = pSrcLine[x];
				pSrcLine[x] = pDstLine[x];
				pDstLine[x] = l;
			}
			pSrcLine += *piWidth;
			pDstLine -= *piWidth;
		}
	}

	return pbReturn;
}

void RE_TempRawImage_CleanUp(void)
{
	if ( pbLoadedPic )
	{
		R_Free( pbLoadedPic );
		pbLoadedPic = NULL;
	}
}



typedef enum
{
	eDISSOLVE_RT_TO_LT = 0,
	eDISSOLVE_LT_TO_RT,
	eDISSOLVE_TP_TO_BT,
	eDISSOLVE_BT_TO_TP,
	eDISSOLVE_CIRCULAR_OUT,	// new image comes out from centre
	//
	eDISSOLVE_RAND_LIMIT,	// label only, not valid to select
	//
	// any others...
	//
	eDISSOLVE_CIRCULAR_IN,	// new image comes in from edges
	//
	eDISSOLVE_NUMBEROF,

	eDISSOLVE_FORCE_DWORD = 0x7fffffff
} Dissolve_e;

typedef struct {
	Dissolve_e	type;
	int			width;
	int			height;
	float		percentage;
} DissolveParms_t;

typedef struct
{
	image_t					*pImage; // old image screen
	image_t					*pDissolve;	// fuzzy thing

	int						iStartTime;	// 0 = not processing
	qboolean				bTouchNeeded;

	DissolveParms_t			dissolveParms;
	buffer_t				*dissolveParmsBuffer;

	// used for binding images to the dissolve pipelines
	VkDescriptorSetLayout	dissolvePipelineDescriptorSetLayout;
	VkDescriptorSet			dissolvePipelineDescriptorSet;

	VkPipelineLayout		dissolvePipelineLayout;
	VkPipeline				dissolvePipeline;

} Dissolve_t;

Dissolve_t Dissolve = { 0 };
#define fDISSOLVE_SECONDS 0.75f

static void RE_KillDissolve( qboolean deleteResources ) {

	Dissolve.iStartTime = 0;

	if( deleteResources ) {
		if( Dissolve.dissolveParmsBuffer ) {
			R_Buffers_DeleteBuffer( Dissolve.dissolveParmsBuffer );
			Dissolve.dissolveParmsBuffer = NULL;
		}
		if( Dissolve.dissolvePipelineDescriptorSetLayout ) {
			vkDestroyDescriptorSetLayout( vkState.device, Dissolve.dissolvePipelineDescriptorSetLayout, NULL );
			Dissolve.dissolvePipelineDescriptorSetLayout = VK_NULL_HANDLE;
		}
		if( Dissolve.dissolvePipelineDescriptorSet ) {
			vkFreeDescriptorSets( vkState.device, vkState.descriptorPool, 1, &Dissolve.dissolvePipelineDescriptorSet );
			Dissolve.dissolvePipelineDescriptorSet = VK_NULL_HANDLE;
		}
		if( Dissolve.dissolvePipelineLayout ) {
			vkDestroyPipelineLayout( vkState.device, Dissolve.dissolvePipelineLayout, NULL );
			Dissolve.dissolvePipelineLayout = VK_NULL_HANDLE;
		}
		if( Dissolve.dissolvePipeline ) {
			vkDestroyPipeline( vkState.device, Dissolve.dissolvePipeline, NULL );
			Dissolve.dissolvePipeline = VK_NULL_HANDLE;
		}
		if( Dissolve.pDissolve ) {
			R_Images_DeleteImage( Dissolve.pDissolve );
			Dissolve.pDissolve = NULL;
		}
		if( Dissolve.pImage ) {
			R_Images_DeleteImage( Dissolve.pImage );
			Dissolve.pImage = NULL;
		}
	}
}

// Draw the dissolve pic to the screen, over the top of what's already been rendered.
//
// return = qtrue while still processing, for those interested...
//
qboolean RE_ProcessDissolve( void )
{
	#if 0
	if (Dissolve.iStartTime)
	{
		if (Dissolve.bTouchNeeded)
		{
			// Stuff to avoid music stutter...
			//
			//	The problem is, that if I call RE_InitDissolve() then call RestartMusic, then by the time the music
			//	has loaded in if it took longer than one second the dissolve would think that it had finished,
			//	even if it had never actually drawn up. However, if I called RE_InitDissolve() AFTER the music had
			//	restarted, then the music would stutter on slow video cards or CPUs while I did the binding/resampling.
			//
			// This way, I restart the millisecond counter the first time we actually get as far as rendering, which
			//	should let things work properly...
			//
			Dissolve.bTouchNeeded = qfalse;
			Dissolve.iStartTime = ri.Milliseconds();
		}

		Dissolve.dissolveParms.percentage = (ri.Milliseconds() - Dissolve.iStartTime) / (1000.0f * fDISSOLVE_SECONDS);

		Dissolve.dissolveParms.width = backEndData->frameBuffer->width;
		Dissolve.dissolveParms.height = backEndData->frameBuffer->height;

//		ri.Printf(PRINT_ALL,"iDissolvePercentage %d\n",Dissolve.dissolveParms.percentage);

		if( Dissolve.dissolveParms.percentage <= 1.f )
		{
			VkBufferMemoryBarrier bufferMemoryBarrier = {};
			bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferMemoryBarrier.buffer = Dissolve.dissolveParmsBuffer->buf;
			bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			bufferMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferMemoryBarrier.offset = 0;
			bufferMemoryBarrier.size = sizeof( Dissolve.dissolveParms );

			vkCmdUpdateBuffer( backEndData->cmdbuf, Dissolve.dissolveParmsBuffer->buf, 0, sizeof( Dissolve.dissolveParms ), &Dissolve.dissolveParms );
			vkCmdPipelineBarrier( backEndData->cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
				0, NULL, 1, &bufferMemoryBarrier, 0, NULL );

			vkCmdBindPipeline( backEndData->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, Dissolve.dissolvePipeline );
			vkCmdBindDescriptorSets( backEndData->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, Dissolve.dissolvePipelineLayout, 4, 1, &Dissolve.dissolvePipelineDescriptorSet, 0, NULL );
			vkCmdDraw( backEndData->cmdbuf, 3, 1, 0, 0 );
		}

		if( Dissolve.dissolveParms.percentage > 1.f )
		{
			RE_KillDissolve( qfalse ); // resources can't be freed if the previous frames haven't been rendered yet
		}
	}
	#endif
	RE_KillDissolve( qfalse );

	return qfalse;
}

// return = qtrue(success) else fail, for those interested...
//
qboolean RE_InitDissolve( qboolean bForceCircularExtroWipe ) {
//	ri.Printf( PRINT_ALL, "RE_InitDissolve()\n");
	qboolean bReturn = qfalse;

#if 0
	if( tr.registered == qtrue ) { // ... stops it crashing during first cinematic before the menus... :-)

		RE_KillDissolve( qfalse );	// kill any that are already running

		if( Dissolve.dissolveParmsBuffer == NULL ) {
			Dissolve.dissolveParmsBuffer = R_CreateBuffer( sizeof( Dissolve.dissolveParms ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0 );
		}

		if( Dissolve.dissolvePipelineDescriptorSetLayout == VK_NULL_HANDLE ) {
			CDescriptorSetLayoutBuilder descriptorSetLayoutBuilder;

			// create the descriptor set layout
			descriptorSetLayoutBuilder.addBinding( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
			descriptorSetLayoutBuilder.addBinding( VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );
			descriptorSetLayoutBuilder.build( &Dissolve.dissolvePipelineDescriptorSetLayout );
		}

		if( Dissolve.dissolvePipelineDescriptorSet == VK_NULL_HANDLE ) {
			VK_AllocateDescriptorSet( Dissolve.dissolvePipelineDescriptorSetLayout, &Dissolve.dissolvePipelineDescriptorSet );
		}

		if( Dissolve.dissolvePipelineLayout == VK_NULL_HANDLE ) {
			CPipelineLayoutBuilder pipelineLayoutBuilder;

			// create the pipeline layout
			pipelineLayoutBuilder.addDescriptorSetLayout( Dissolve.dissolvePipelineDescriptorSetLayout );
			pipelineLayoutBuilder.build( &Dissolve.dissolvePipelineLayout );
		}

		if( Dissolve.dissolvePipeline == VK_NULL_HANDLE ) {
			CPipelineBuilder pipelineBuilder;
			pipelineBuilder.pipelineCreateInfo.layout = Dissolve.dissolvePipelineLayout;
			pipelineBuilder.pipelineCreateInfo.renderPass = tr.postProcessFrameBuffer->renderPass;

			// shaders
			pipelineBuilder.setShader( VK_SHADER_STAGE_VERTEX_BIT, tr_dissolve_VS );
			pipelineBuilder.setShader( VK_SHADER_STAGE_FRAGMENT_BIT, tr_dissolve_PS );

			// color blend
			VkPipelineColorBlendAttachmentState attachmentBlend = {};
			attachmentBlend.colorWriteMask = 0xf;
			attachmentBlend.blendEnable = VK_TRUE;
			attachmentBlend.colorBlendOp = VK_BLEND_OP_ADD;
			attachmentBlend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			attachmentBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			attachmentBlend.alphaBlendOp = VK_BLEND_OP_ADD;
			attachmentBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			attachmentBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

			pipelineBuilder.colorBlend.attachmentCount = 1;
			pipelineBuilder.colorBlend.pAttachments = &attachmentBlend;

			// disable depth test
			pipelineBuilder.depthStencil.depthTestEnable = VK_FALSE;
			pipelineBuilder.depthStencil.depthWriteEnable = VK_FALSE;

			pipelineBuilder.build( &Dissolve.dissolvePipeline );
		}

		// create a copy of the current screen image
		if( Dissolve.pImage == NULL ||
			Dissolve.pImage->width != glConfig.vidWidth ||
			Dissolve.pImage->height != glConfig.vidHeight ) {

			if( Dissolve.pImage ) {
				R_Images_DeleteImage( Dissolve.pImage );
			}

			Dissolve.pImage = R_CreateTransientImage(	"*DissolveImage",
														tr.screenshotImage->width,
														tr.screenshotImage->height,
														tr.screenshotImage->internalFormat,
														VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );

			VK_SetImageLayout( Dissolve.pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT );
			VK_CopyImage( Dissolve.pImage, tr.screenshotImage );

			// transition the image to read-only optimal layout
			VK_SetImageLayout( Dissolve.pImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT );
		}

		// pick dissolve type...
#if 0
		// cycles through every dissolve type, for testing...
		//
		static Dissolve_e eDissolve = (Dissolve_e) 0;
		Dissolve.dissolveParms.type =	eDissolve;
									eDissolve = (Dissolve_e) (eDissolve+1);
								if (eDissolve == eDISSOLVE_RAND_LIMIT)
									eDissolve = (Dissolve_e) (eDissolve+1);
								if (eDissolve >= eDISSOLVE_NUMBEROF)
									eDissolve = (Dissolve_e) 0;
#else
		// final (& random) version...
		//
		Dissolve.dissolveParms.type = (Dissolve_e)Q_irand( 0, eDISSOLVE_RAND_LIMIT - 1 );
#endif

		if( bForceCircularExtroWipe )
		{
			Dissolve.dissolveParms.type = eDISSOLVE_CIRCULAR_IN;
		}

		// ... and load appropriate graphics...
		//

		// special tweak, although this code is normally called just before client spawns into world (and
		//	is therefore pretty much immune to precache issues) I also need to make sure that the inverse
		//	iris graphic is loaded so for the special case of doing a circular wipe at the end of the last
		//	level doesn't stall on loading the image. So I'll load it here anyway - to prime the image -
		//	then allow the random wiper to overwrite the ptr if needed. This way the end of level call
		//	will be instant.  Downside: every level has one extra 256x256 texture.
	 	// Trying to decipher these comments - looks like no problem taking this out. I want the RAM.
		{
			Dissolve.pDissolve = R_FindImageFile(	"gfx/2d/iris_mono_rev",					// const char *name
													qfalse,									// qboolean mipmap
													qfalse,									// qboolean allowPicmip
													qfalse,									// qboolean allowTC
													VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE	// VkSamplerAdderssMode wrapClampMode
												);
		}

		extern cvar_t *com_buildScript;
		if (com_buildScript->integer)
		{
			// register any/all of the possible CASE statements below...
			//
			Dissolve.pDissolve = R_FindImageFile(	"gfx/2d/iris_mono",						// const char *name
													qfalse,									// qboolean mipmap
													qfalse,									// qboolean allowPicmip
													qfalse,									// qboolean allowTC
													VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE	// VkSamplerAdderssMode wrapClampMode
												);
			Dissolve.pDissolve = R_FindImageFile(	"textures/common/dissolve",				// const char *name
													qfalse,									// qboolean mipmap
													qfalse,									// qboolean allowPicmip
													qfalse,									// qboolean allowTC
													VK_SAMPLER_ADDRESS_MODE_REPEAT			// VkSamplerAdderssMode wrapClampMode
												);
		}

		switch( Dissolve.dissolveParms.type )
		{
			case eDISSOLVE_CIRCULAR_IN:
			{
				Dissolve.pDissolve = R_FindImageFile(	"gfx/2d/iris_mono_rev",					// const char *name
														qfalse,									// qboolean mipmap
														qfalse,									// qboolean allowPicmip
														qfalse,									// qboolean allowTC
														VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE	// VkSamplerAdderssMode wrapClampMode
													);
			}
			break;

			case eDISSOLVE_CIRCULAR_OUT:
			{
				Dissolve.pDissolve = R_FindImageFile(	"gfx/2d/iris_mono",						// const char *name
														qfalse,									// qboolean mipmap
														qfalse,									// qboolean allowPicmip
														qfalse,									// qboolean allowTC
														VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE	// VkSamplerAdderssMode wrapClampMode
													);
			}
			break;

			default:
			{
				Dissolve.pDissolve = R_FindImageFile(	"textures/common/dissolve",		// const char *name
														qfalse,							// qboolean mipmap
														qfalse,							// qboolean allowPicmip
														qfalse,							// qboolean allowTC
														VK_SAMPLER_ADDRESS_MODE_REPEAT	// VkSamplerAdderssMode wrapClampMode
													);
			}
			break;
		}

		// all good?...
		//
		if( Dissolve.pDissolve )	// test if image was found, if not, don't do dissolves
		{
			Dissolve.iStartTime = ri.Milliseconds();	// gets overwritten first time, but MUST be set to NZ
			Dissolve.bTouchNeeded = qtrue;
			bReturn = qtrue;
		}
		else
		{
			RE_KillDissolve( qtrue );
		}

		if( bReturn ) {
			// write immutable descriptors
			CDescriptorSetWriter writer( Dissolve.dissolvePipelineDescriptorSet );
			writer.writeBuffer( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Dissolve.dissolveParmsBuffer );
			writer.writeImage( 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, Dissolve.pDissolve );
			writer.writeImage( 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, Dissolve.pImage );
			writer.flush();
		}
	}
#endif

	return bReturn;
}
