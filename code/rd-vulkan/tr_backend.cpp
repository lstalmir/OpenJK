/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
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

#include "../server/exe_headers.h"

#define VMA_IMPLEMENTATION
#include "tr_local.h"
#include "tr_common.h"

backEndData_t *backEndData;
backEndState_t backEnd;

bool tr_stencilled = false;
extern qboolean tr_distortionPrePost;	   // tr_shadows.cpp
extern qboolean tr_distortionNegate;	   // tr_shadows.cpp
extern void RB_CaptureScreenImage( void ); // tr_shadows.cpp
extern void RB_DistortionFill( void );	   // tr_shadows.cpp
static void RB_DrawGlowOverlay();
static void RB_BlurGlowTexture();

// Whether we are currently rendering only glowing objects or not.
bool g_bRenderGlowingObjects = false;

// Whether the current hardware supports dynamic glows/flares.
bool g_bDynamicGlowSupported = false;

static const float s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};


static VkPipelineStageFlags VK_GetPipelineStageFlagsForAccess( VkAccessFlags access ) {
	VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	// Barrier on the host operations.
	if( ( access & VK_ACCESS_HOST_READ_BIT ) || ( access & VK_ACCESS_HOST_WRITE_BIT ) )
		stage |= VK_PIPELINE_STAGE_HOST_BIT;

	// Barrier on the transfer operations.
	if( ( access & VK_ACCESS_TRANSFER_READ_BIT ) || ( access & VK_ACCESS_TRANSFER_WRITE_BIT ) )
		stage |= VK_PIPELINE_STAGE_TRANSFER_BIT;

	// Barrier on indirect command read.
	if( ( access & VK_ACCESS_INDIRECT_COMMAND_READ_BIT ) )
		stage |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

	// Barrier on vertex input.
	if( ( access & VK_ACCESS_INDEX_READ_BIT ) || ( access & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT ) )
		stage |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

	// Barrier on the first shader stage in the pipeline.
	if( ( access & VK_ACCESS_SHADER_READ_BIT ) || ( access & VK_ACCESS_SHADER_WRITE_BIT ) )
		stage |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

	// Barrier on the first depth test.
	if( ( access & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT ) || ( access & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ) )
		stage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

	// Barrier on the fragment shader.
	if( ( access & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT ) )
		stage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	// Barrier on the color output.
	if( ( access & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT ) || ( access & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT ) )
		stage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	return stage;
}

/*
** VK_BeginFrame
*/
void VK_BeginFrame( void ) {
	VkResult res;

	if( vkState.imagenum == UINT32_MAX ) {
		// move to the next resource
		vkState.resnum = ( vkState.resnum + 1 ) % vkState.imgcount;
		backEndData->cmdbuf = vkState.cmdbuffers[2 * vkState.resnum];
		backEndData->uploadCmdbuf = vkState.cmdbuffers[2 * vkState.resnum + 1];
		backEndData->semaphore = vkState.semaphores[vkState.resnum];

		res = vkAcquireNextImageKHR(
			vkState.device, vkState.swapchain, UINT64_MAX, backEndData->semaphore, VK_NULL_HANDLE, &vkState.imagenum );

		if( res != VK_SUCCESS ) {
			Com_Error( ERR_FATAL, "VK_BeginFrame: failed to acquire next swapchain image (%d)\n", res );
		}

		backEndData->image = &vkState.images[vkState.imagenum];
		backEndData->imageArraySlice = 0; // todo: stereo

		backEndData->frameBuffer = NULL;
		backEndData->pipeline = VK_NULL_HANDLE;
		backEndData->pipelineLayout = VK_NULL_HANDLE;
		memset( backEndData->descriptorSets, 0, sizeof( backEndData->descriptorSets ) );

		// wait until the resources are available
		res = vkWaitForFences( vkState.device, 1, &vkState.fences[vkState.resnum], VK_FALSE, UINT64_MAX );
		if( res != VK_SUCCESS ) {
			Com_Error( ERR_FATAL, "VK_BeginFrame: failed to wait for resource availability (%d)\n", res );
		}

		vkResetFences( vkState.device, 1, &vkState.fences[vkState.resnum] );

		// free the upload buffers for this frame to the pool
		VK_PrepareUploadBuffers();

		// begin the command buffer
		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		res = vkBeginCommandBuffer( backEndData->cmdbuf, &commandBufferBeginInfo );
		if( res != VK_SUCCESS ) {
			Com_Error( ERR_FATAL, "VK_BeginFrame: failed to begin recording the next command buffer (%d)\n", res );
		}

		res = vkBeginCommandBuffer( backEndData->uploadCmdbuf, &commandBufferBeginInfo );
		if( res != VK_SUCCESS ) {
			Com_Error( ERR_FATAL, "VK_BeginFrame: failed to begin recording the next upload command buffer (%d)\n", res );
		}

		if( tr.registered ) {
			R_ClearFrameBuffer( tr.sceneFrameBuffer );

			// reset the dynamic vertex buffer for this frame
			backEndData->dynamicGeometryBuilder.reset();

			// upload the globals
			VK_UploadBuffer( tr.globalsBuffer, (byte *)&tr.globals, sizeof( tr.globals ), 0 );
		}
	}
}

/*
** VK_EndFrame
*/
void VK_EndFrame( void ) {
	VkCommandBuffer commandBuffers[2];
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkResult res;

	assert( vkState.imagenum != UINT32_MAX );

	// get the current frame buffer
	frameBuffer_t *frameBuffer = backEndData->frameBuffer;
	R_BindFrameBuffer( NULL );

	image_t *renderedImage = tr.screenImage;
	if( frameBuffer ) {
		renderedImage = frameBuffer->images[0].i;
	}

	// copy rendered image to swapchain image
	VK_SetImageLayout( renderedImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT );
	VK_SetImageLayout( tr.screenshotImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT );
	VK_SetImageLayout( backEndData->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT );

	VK_CopyImage( tr.screenshotImage, renderedImage );
	VK_CopyImage( backEndData->image, renderedImage );

	// transition the swapchain image to presentable layout
	VK_SetImageLayout( backEndData->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_TRANSFER_READ_BIT );

	// finish recording of the command buffer and submit it for execution
	vkEndCommandBuffer( backEndData->cmdbuf );
	vkEndCommandBuffer( backEndData->uploadCmdbuf );

	// execute upload commands first
	commandBuffers[0] = backEndData->uploadCmdbuf;
	commandBuffers[1] = backEndData->cmdbuf;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = ARRAY_LEN( commandBuffers );
	submitInfo.pCommandBuffers = commandBuffers;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &backEndData->semaphore;
	submitInfo.pWaitDstStageMask = &waitStage;

	res = vkQueueSubmit( vkState.queue, 1, &submitInfo, vkState.fences[vkState.resnum] );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "RB_SwapBuffers: failed to submit command buffer for execution (%d)\n", res );
	}

	// in Vulkan the buffers are managed by the swap chain
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &vkState.swapchain;
	presentInfo.pImageIndices = &vkState.imagenum;

	res = vkQueuePresentKHR( vkState.queue, &presentInfo );

	if( res == VK_ERROR_OUT_OF_DATE_KHR ) {
		// swap chain needs to be recreated

		// before we do that, we have to sync with gpu on all pending frames
		res = vkWaitForFences( vkState.device, vkState.imgcount, vkState.fences, VK_TRUE, UINT64_MAX );
		if( res != VK_SUCCESS ) {
			Com_Error( ERR_FATAL, "RB_SwapBuffers: failed to synchronize with the command queue (%d)\n", res );
		}

		// reinitialize the swapchain
		VK_InitSwapchain();
	}

	vkState.imagenum = UINT32_MAX;
}

/*
** VK_SetImageLayout
*/
void VK_SetImageLayout( image_t *im, VkImageLayout dstLayout, VkAccessFlags dstAccess ) {

	// Perform a transition only if layout of access flags change.
	if( dstLayout != im->layout || dstAccess != im->access ) {
		VkImageMemoryBarrier imageMemoryBarrier;
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.pNext = nullptr;
		imageMemoryBarrier.image = im->tex;
		imageMemoryBarrier.srcAccessMask = im->access;
		imageMemoryBarrier.dstAccessMask = dstAccess;
		imageMemoryBarrier.oldLayout = im->layout;
		imageMemoryBarrier.newLayout = dstLayout;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.subresourceRange.aspectMask = im->allAspectFlags;
		imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		imageMemoryBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		imageMemoryBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;

		vkCmdPipelineBarrier(
			backEndData->cmdbuf,
			VK_GetPipelineStageFlagsForAccess( imageMemoryBarrier.srcAccessMask ),
			VK_GetPipelineStageFlagsForAccess( imageMemoryBarrier.dstAccessMask ),
			VK_DEPENDENCY_BY_REGION_BIT,
			0, NULL,
			0, NULL,
			1, &imageMemoryBarrier );

		// Update the image state.
		im->layout = dstLayout;
		im->access = dstAccess;
	}
}

/*
** VK_AlignUniformBufferSize
*/
int VK_AlignUniformBufferSize( int structureSize ) {
	int alignment = vkState.physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;

	// return the next multiple of the alignemt that can fit the whole structure size
	return PAD( structureSize, alignment );
}

/*
** VK_CopyImage
*/
void VK_CopyImage( image_t *dst, image_t *src ) {
	VkImageCopy imageCopy = {};

	imageCopy.extent.width = dst->width;
	imageCopy.extent.height = dst->height;
	imageCopy.extent.depth = 1;
	imageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopy.srcSubresource.layerCount = 1;
	imageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopy.dstSubresource.layerCount = 1;

	vkCmdCopyImage( backEndData->cmdbuf, src->tex, src->layout, dst->tex, dst->layout, 1, &imageCopy );
}

/*
** VK_ClearColorImage
*/
void VK_ClearColorImage( image_t *image, const VkClearColorValue *value ) {
	assert( image->layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || image->layout == VK_IMAGE_LAYOUT_GENERAL );

	VkImageSubresourceRange range = {};
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.levelCount = VK_REMAINING_MIP_LEVELS;
	range.layerCount = VK_REMAINING_ARRAY_LAYERS;

	vkCmdClearColorImage( backEndData->cmdbuf, image->tex, image->layout, value, 1, &range );
}

/*
** VK_ClearDepthStencilImage
*/
void VK_ClearDepthStencilImage( image_t *image, const VkClearDepthStencilValue *value ) {
	assert( image->layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || image->layout == VK_IMAGE_LAYOUT_GENERAL );

	VkImageSubresourceRange range = {};
	range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	range.levelCount = VK_REMAINING_MIP_LEVELS;
	range.layerCount = VK_REMAINING_ARRAY_LAYERS;

	vkCmdClearDepthStencilImage( backEndData->cmdbuf, image->tex, image->layout, value, 1, &range );
}

/*
** VK_AllocateDescriptorSet
*/
void VK_AllocateDescriptorSet( VkDescriptorSetLayout layout, VkDescriptorSet *set ) {
	VkDescriptorSetAllocateInfo allocateInfo;
	VkResult res;

	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.pNext = NULL;
	allocateInfo.descriptorPool = vkState.descriptorPool;
	allocateInfo.descriptorSetCount = 1;
	allocateInfo.pSetLayouts = &layout;

	res = vkAllocateDescriptorSets( vkState.device, &allocateInfo, set );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "AllocVulkanDescriptorSet: failed to allocate descriptor set (%d)\n", res );
	}
}

/*
** VK_DeleteDescriptorSet
*/
void VK_DeleteDescriptorSet( VkDescriptorSet set ) {
	vkFreeDescriptorSets( vkState.device, vkState.descriptorPool, 1, &set );
}

/*
** VK_SetDebugObjectName
*/
void VK_SetDebugObjectName( uint64_t object, VkObjectType type, const char *name ) {
#if defined( _DEBUG )
	if( vkState.pfnSetDebugObjectName ) {
		VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
		objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		objectNameInfo.objectHandle = object;
		objectNameInfo.objectType = type;
		objectNameInfo.pObjectName = name;

		vkState.pfnSetDebugObjectName( vkState.device, &objectNameInfo );
	}
#endif
}


/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace( VkClearColorValue *clearColor ) {
	float c;

	if( !backEnd.isHyperspace ) {
		// do initialization shit
	}

	c = ( backEnd.refdef.time & 255 ) / 255.0f;
	clearColor->float32[0] = c;
	clearColor->float32[1] = c;
	clearColor->float32[2] = c;
	clearColor->float32[3] = 1;

	backEnd.isHyperspace = qtrue;
}


void SetViewportAndScissor( int depthRange ) {
	VkViewport viewport;
	VkRect2D scissor;
	float minDepth, maxDepth;

	switch( depthRange ) {
	default:
	case 0:
		minDepth = 0;
		maxDepth = 1;
		break;

	case 1:
		minDepth = 0;
		maxDepth = .3f;
		break;

	case 2:
		minDepth = 0;
		maxDepth = 0;
		break;
	}

	// set the window clipping
	viewport.x = backEnd.viewParms.viewportX;
	viewport.y = backEnd.viewParms.viewportY;
	viewport.width = backEnd.viewParms.viewportWidth;
	viewport.height = backEnd.viewParms.viewportHeight;
	viewport.minDepth = minDepth;
	viewport.maxDepth = maxDepth;
	vkCmdSetViewport( backEndData->cmdbuf, 0, 1, &viewport );

	// set the scissor rect
	scissor.offset.x = Q_max( 0, backEnd.viewParms.viewportX );
	scissor.offset.y = Q_max( 0, backEnd.viewParms.viewportY );
	scissor.extent.width = backEnd.viewParms.viewportWidth;
	scissor.extent.height = backEnd.viewParms.viewportHeight;
	vkCmdSetScissor( backEndData->cmdbuf, 0, 1, &scissor );
}

/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
static void RB_BeginDrawingView( void ) {
	frameBuffer_t *frameBuffer = tr.sceneFrameBuffer;
	frameBuffer->clearValues[1].depthStencil.depth = 1.f;
	frameBuffer->clearValues[1].depthStencil.stencil = 0;

	if( g_bRenderGlowingObjects ) {
		frameBuffer = tr.glowFrameBuffer;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	// clear relevant buffers
	if( r_measureOverdraw->integer || r_shadows->integer == 2 || tr_stencilled ) {
		tr_stencilled = false;
	}

	if( skyboxportal ) {
		if( backEnd.refdef.rdflags & RDF_SKYBOXPORTAL ) {								// portal scene, clear whatever is necessary
			if( r_fastsky->integer || ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) ) { // fastsky: clear color
				// try clearing first with the portal sky fog color, then the world fog color, then finally a default
				if( tr.world && tr.world->globalFog != -1 ) {
					const fog_t *fog = &tr.world->fogs[tr.world->globalFog];
					frameBuffer->clearValues[0].color.float32[0] = fog->parms.color[0];
					frameBuffer->clearValues[0].color.float32[1] = fog->parms.color[1];
					frameBuffer->clearValues[0].color.float32[2] = fog->parms.color[2];
					frameBuffer->clearValues[0].color.float32[3] = 1.0f;
				}
				else {
					frameBuffer->clearValues[0].color.float32[0] = 0.3f;
					frameBuffer->clearValues[0].color.float32[1] = 0.3f;
					frameBuffer->clearValues[0].color.float32[2] = 0.3f;
					frameBuffer->clearValues[0].color.float32[3] = 1.0f;
				}
			}
		}
	}
	else {
		if( r_fastsky->integer && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) && !g_bRenderGlowingObjects ) {
			if( tr.world && tr.world->globalFog != -1 ) {
				const fog_t *fog = &tr.world->fogs[tr.world->globalFog];
				frameBuffer->clearValues[0].color.float32[0] = fog->parms.color[0];
				frameBuffer->clearValues[0].color.float32[1] = fog->parms.color[1];
				frameBuffer->clearValues[0].color.float32[2] = fog->parms.color[2];
				frameBuffer->clearValues[0].color.float32[3] = 1.0f;
			}
			else {
				// FIXME: get color of sky
				frameBuffer->clearValues[0].color.float32[0] = 0.3f;
				frameBuffer->clearValues[0].color.float32[1] = 0.3f;
				frameBuffer->clearValues[0].color.float32[2] = 0.3f;
				frameBuffer->clearValues[0].color.float32[3] = 1.0f;
			}
		}
	}

	if( !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) && ( r_DynamicGlow->integer && !g_bRenderGlowingObjects ) ) {
		if( tr.world && tr.world->globalFog != -1 ) { // this is because of a bug in multiple scenes I think, it needs to clear for the second scene but it doesn't normally.
			const fog_t *fog = &tr.world->fogs[tr.world->globalFog];
			frameBuffer->clearValues[0].color.float32[0] = fog->parms.color[0];
			frameBuffer->clearValues[0].color.float32[1] = fog->parms.color[1];
			frameBuffer->clearValues[0].color.float32[2] = fog->parms.color[2];
			frameBuffer->clearValues[0].color.float32[3] = 1.0f;
		}
	}

	if( ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) ) {
		RB_Hyperspace( &frameBuffer->clearValues[0].color );
		return;
	}
	else {
		backEnd.isHyperspace = qfalse;
	}

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

#if 0
	// clip to the plane of the portal
	if( backEnd.viewParms.isPortal ) {
		float plane[4];
		double plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct( backEnd.viewParms.ori.axis[0], plane );
		plane2[1] = DotProduct( backEnd.viewParms.ori.axis[1], plane );
		plane2[2] = DotProduct( backEnd.viewParms.ori.axis[2], plane );
		plane2[3] = DotProduct( plane, backEnd.viewParms.ori.origin ) - plane[3];

		qglLoadMatrixf( s_flipMatrix );
		qglClipPlane( GL_CLIP_PLANE0, plane2 );
		qglEnable( GL_CLIP_PLANE0 );
	}
	else {
		qglDisable( GL_CLIP_PLANE0 );
	}
#endif

	R_BindFrameBuffer( frameBuffer );
}

// used by RF_DISTORTION
static inline bool R_WorldCoordToScreenCoordFloat( vec3_t worldCoord, float *x, float *y ) {
	int xcenter, ycenter;
	vec3_t local, transformed;
	vec3_t vfwd;
	vec3_t vright;
	vec3_t vup;
	float xzi;
	float yzi;

	xcenter = glConfig.vidWidth / 2;
	ycenter = glConfig.vidHeight / 2;

	// AngleVectors (tr.refdef.viewangles, vfwd, vright, vup);
	VectorCopy( tr.refdef.viewaxis[0], vfwd );
	VectorCopy( tr.refdef.viewaxis[1], vright );
	VectorCopy( tr.refdef.viewaxis[2], vup );

	VectorSubtract( worldCoord, tr.refdef.vieworg, local );

	transformed[0] = DotProduct( local, vright );
	transformed[1] = DotProduct( local, vup );
	transformed[2] = DotProduct( local, vfwd );

	// Make sure Z is not negative.
	if( transformed[2] < 0.01 ) {
		return false;
	}

	xzi = xcenter / transformed[2] * ( 90.0 / tr.refdef.fov_x );
	yzi = ycenter / transformed[2] * ( 90.0 / tr.refdef.fov_y );

	*x = xcenter + xzi * transformed[0];
	*y = ycenter - yzi * transformed[1];

	return true;
}

// used by RF_DISTORTION
static inline bool R_WorldCoordToScreenCoord( vec3_t worldCoord, int *x, int *y ) {
	float xF, yF;
	bool retVal = R_WorldCoordToScreenCoordFloat( worldCoord, &xF, &yF );
	*x = (int)xF;
	*y = (int)yF;
	return retVal;
}

/*
==================
RB_RenderDrawSurfList
==================
*/
// number of possible surfs we can postrender.
// note that postrenders lack much of the optimization that the standard sort-render crap does,
// so it's slower.
#define MAX_POST_RENDERS 128

typedef struct
{
	int fogNum;
	int entNum;
	int dlighted;
	int depthRange;
	drawSurf_t *drawSurf;
	shader_t *shader;
} postRender_t;

static postRender_t g_postRenders[MAX_POST_RENDERS];
static int g_numPostRenders = 0;

void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t *shader, *oldShader;
	int fogNum, oldFogNum;
	int entityNum, oldEntityNum;
	int dlighted, oldDlighted;
	int depthRange, oldDepthRange;
	int i;
	drawSurf_t *drawSurf;
	unsigned int oldSort;
	float originalTime;
	trRefEntity_t *curEnt;
	postRender_t *pRender;
	frameBuffer_t *frameBuffer;
	image_t *frameBufferImage;
	bool didShadowPass = false;

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView();

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDepthRange = qfalse;
	oldDlighted = qfalse;
	oldSort = (unsigned int)-1;
	depthRange = qfalse;

	// set default viewport and scissor
	SetViewportAndScissor( depthRange );

	backEnd.pc.c_surfaces += numDrawSurfs;

	for( i = 0, drawSurf = drawSurfs; i < numDrawSurfs; i++, drawSurf++ ) {
		if( drawSurf->sort == oldSort ) {
			// fast path, same as previous sort
			rb_surfaceTable[*drawSurf->surface]( drawSurf->surface );
			continue;
		}
		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );

		// If we're rendering glowing objects, but this shader has no stages with glow, skip it!
		if( g_bRenderGlowingObjects && !shader->hasGlow ) {
			shader = oldShader;
			entityNum = oldEntityNum;
			fogNum = oldFogNum;
			dlighted = oldDlighted;
			continue;
		}

		oldSort = drawSurf->sort;

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if( entityNum != REFENTITYNUM_WORLD &&
			g_numPostRenders < MAX_POST_RENDERS ) {
			if( ( backEnd.refdef.entities[entityNum].e.renderfx & RF_DISTORTION ) ||
				( backEnd.refdef.entities[entityNum].e.renderfx & RF_FORCE_ENT_ALPHA ) ) { // must render last
				curEnt = &backEnd.refdef.entities[entityNum];
				pRender = &g_postRenders[g_numPostRenders];

				g_numPostRenders++;

				depthRange = 0;
				// figure this stuff out now and store it
				if( curEnt->e.renderfx & RF_NODEPTH ) {
					depthRange = 2;
				}
				else if( curEnt->e.renderfx & RF_DEPTHHACK ) {
					depthRange = 1;
				}
				pRender->depthRange = depthRange;

				// It is not necessary to update the old* values because
				// we are not updating now with the current values.
				depthRange = oldDepthRange;

				// store off the ent num
				pRender->entNum = entityNum;

				// remember the other values necessary for rendering this surf
				pRender->drawSurf = drawSurf;
				pRender->dlighted = dlighted;
				pRender->fogNum = fogNum;
				pRender->shader = shader;

				// assure the info is back to the last set state
				shader = oldShader;
				entityNum = oldEntityNum;
				fogNum = oldFogNum;
				dlighted = oldDlighted;

				oldSort = (unsigned int)-1; // invalidate this thing, cause we may want to postrender more surfs of the same sort

				// continue without bothering to begin a draw surf
				continue;
			}
		}

		if( shader != oldShader || fogNum != oldFogNum || dlighted != oldDlighted || ( entityNum != oldEntityNum && !shader->entityMergable ) ) {
			if( oldShader != NULL ) {
				RB_EndSurface();

				if( !didShadowPass && shader && shader->sort > SS_BANNER ) {
					RB_ShadowFinish();
					didShadowPass = true;
				}
			}

			RB_BeginSurface( shader, fogNum );

			oldShader = shader;
			oldFogNum = fogNum;
			oldDlighted = dlighted;
		}

		//
		// change the modelview matrix if needed
		//
		if( entityNum != oldEntityNum ) {
			depthRange = qfalse;

			if( entityNum != REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori );

				// set up the dynamic lighting if needed
				if( backEnd.currentEntity->needDlights ) {
					R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori );
				}

				if( backEnd.currentEntity->e.renderfx & RF_NODEPTH ) {
					// No depth at all, very rare but some things for seeing through walls
					depthRange = 2;
				}
				else if( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
				}
			}
			else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.ori = backEnd.viewParms.world;
				R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori );
			}

			//
			// change depthrange if needed
			//
			if( oldDepthRange != depthRange ) {
				SetViewportAndScissor( depthRange );
				oldDepthRange = depthRange;
			}

			// update the uniform buffer
			memcpy( &backEnd.currentEntity->model.ori.modelMatrix, backEnd.ori.modelMatrix, sizeof( backEnd.ori.modelMatrix ) );

			backEnd.currentEntity->model.ori.origin.x = backEnd.ori.origin[0];
			backEnd.currentEntity->model.ori.origin.y = backEnd.ori.origin[1];
			backEnd.currentEntity->model.ori.origin.z = backEnd.ori.origin[2];

			VK_UploadBuffer( backEnd.currentEntity->modelBuffer, (byte *)&backEnd.currentEntity->model, sizeof( backEnd.currentEntity->model ), 0 );

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[*drawSurf->surface]( drawSurf->surface );
	}

	if( tr_stencilled && tr_distortionPrePost ) { // ok, cap it now
		RB_CaptureScreenImage();
		RB_DistortionFill();
	}

	// get the frame buffer
	frameBuffer = backEndData->frameBuffer;
	frameBufferImage = frameBuffer->images[0].i;

	// render distortion surfs (or anything else that needs to be post-rendered)
	if( g_numPostRenders > 0 ) {
		int lastPostEnt = -1;

		while( g_numPostRenders > 0 ) {
			g_numPostRenders--;
			pRender = &g_postRenders[g_numPostRenders];

			RB_BeginSurface( pRender->shader, pRender->fogNum );

			backEnd.currentEntity = &backEnd.refdef.entities[pRender->entNum];
			backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;

			// set up the transformation matrix
			R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori );

			// set up the dynamic lighting if needed
			if( backEnd.currentEntity->needDlights ) {
				R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori );
			}

			SetViewportAndScissor( pRender->depthRange );

			if( ( backEnd.currentEntity->e.renderfx & RF_DISTORTION ) &&
				lastPostEnt != pRender->entNum ) { // do the capture now, we only need to do it once per ent
				int x, y;
				int rad = backEnd.currentEntity->e.radius;

				if( R_WorldCoordToScreenCoord( backEnd.currentEntity->e.origin, &x, &y ) ) {
					VkImageCopy imageCopy = {};
					int cX, cY;
					cX = glConfig.vidWidth - x - ( rad / 2 );
					cY = glConfig.vidHeight - y - ( rad / 2 );

					if( cX + rad > glConfig.vidWidth ) { // would it go off screen?
						cX = glConfig.vidWidth - rad;
					}
					else if( cX < 0 ) { // cap it off at 0
						cX = 0;
					}

					if( cY + rad > glConfig.vidHeight ) { // would it go off screen?
						cY = glConfig.vidHeight - rad;
					}
					else if( cY < 0 ) { // cap it off at 0
						cY = 0;
					}

					// unbind the frame buffer to copy it to screenImage
					R_BindFrameBuffer( NULL );

					VK_SetImageLayout( frameBufferImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT );
					VK_SetImageLayout( tr.screenImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT );

					// now copy a portion of the screen to this texture
					imageCopy.extent.width = rad;
					imageCopy.extent.height = rad;
					imageCopy.extent.depth = 1;
					imageCopy.srcOffset.x = cX;
					imageCopy.srcOffset.y = cY;
					imageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					imageCopy.srcSubresource.layerCount = 1;
					imageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					imageCopy.dstSubresource.layerCount = 1;

					vkCmdCopyImage( backEndData->cmdbuf, frameBufferImage->tex, frameBufferImage->layout, tr.screenImage->tex, tr.screenImage->layout, 1, &imageCopy );

					// rebind the frame buffer
					R_BindFrameBuffer( frameBuffer );

					lastPostEnt = pRender->entNum;
				}
			}

			rb_surfaceTable[*pRender->drawSurf->surface]( pRender->drawSurf->surface );
			RB_EndSurface();
		}
	}

	R_BindFrameBuffer( NULL );

#if 0
	RB_DrawSun();
#endif
	if( tr_stencilled && !tr_distortionPrePost ) { // draw in the stencil buffer's cutout
		RB_DistortionFill();
	}
	if( !didShadowPass ) {
		// darken down any stencil shadows
		RB_ShadowFinish();
		didShadowPass = true;
	}

	// add light flares on lights that aren't obscured
	//	RB_RenderFlares();
}


/*
============================================================================

RENDER BACK END FUNCTIONS

============================================================================
*/

/*
=============
RB_SetColor

=============
*/
const void *RB_SetColor( const void *data ) {
	const setColorCommand_t *cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D.r = (byte)( cmd->color[2] * 255 );
	backEnd.color2D.g = (byte)( cmd->color[1] * 255 );
	backEnd.color2D.b = (byte)( cmd->color[0] * 255 );
	backEnd.color2D.a = (byte)( cmd->color[3] * 255 );

	return (const void *)( cmd + 1 );
}

/*
=============
RB_StretchPic
=============
*/
const void *RB_StretchPic( const void *data ) {
	tr_shader::vertex_t *vertex;
	const stretchPicCommand_t *cmd;
	shader_t *shader;
	drawCommand_t *draw;

	cmd = (const stretchPicCommand_t *)data;

	shader = cmd->shader;
	if( shader != tess.shader ) {
		RB_EndSurface();
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

	backEndData->dynamicGeometryBuilder.checkOverflow( 4, 6 );

	int a = backEndData->dynamicGeometryBuilder.addVertex();
	vertex = &backEndData->dynamicGeometryBuilder.vertexes[a];
	vertex->position.x = cmd->x;
	vertex->position.y = cmd->y;
	vertex->position.z = 0;
	vertex->texCoord0.x = cmd->s1;
	vertex->texCoord0.y = cmd->t1;
	vertex->vertexColor = backEnd.color2D;

	int b = backEndData->dynamicGeometryBuilder.addVertex();
	vertex = &backEndData->dynamicGeometryBuilder.vertexes[b];
	vertex->position.x = cmd->x + cmd->w;
	vertex->position.y = cmd->y;
	vertex->position.z = 0;
	vertex->texCoord0.x = cmd->s2;
	vertex->texCoord0.y = cmd->t1;
	vertex->vertexColor = backEnd.color2D;

	int c = backEndData->dynamicGeometryBuilder.addVertex();
	vertex = &backEndData->dynamicGeometryBuilder.vertexes[c];
	vertex->position.x = cmd->x + cmd->w;
	vertex->position.y = cmd->y + cmd->h;
	vertex->position.z = 0;
	vertex->texCoord0.x = cmd->s2;
	vertex->texCoord0.y = cmd->t2;
	vertex->vertexColor = backEnd.color2D;

	int d = backEndData->dynamicGeometryBuilder.addVertex();
	vertex = &backEndData->dynamicGeometryBuilder.vertexes[d];
	vertex->position.x = cmd->x;
	vertex->position.y = cmd->y + cmd->h;
	vertex->position.z = 0;
	vertex->texCoord0.x = cmd->s1;
	vertex->texCoord0.y = cmd->t2;
	vertex->vertexColor = backEnd.color2D;

	backEndData->dynamicGeometryBuilder.addTriangle( a, c, b );
	backEndData->dynamicGeometryBuilder.addTriangle( a, d, c );

	return (const void *)( cmd + 1 );
}


/*
=============
RB_RotatePic
=============
*/
const void *RB_RotatePic( const void *data ) {
	tr_shader::vertex_t *vertex;
	const rotatePicCommand_t *cmd;
	shader_t *shader;
	drawCommand_t *draw;

	cmd = (const rotatePicCommand_t *)data;

	shader = cmd->shader;
	if( shader != tess.shader ) {
		RB_EndSurface();
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

	float angle = DEG2RAD( cmd->a );
	float sina = sinf( angle );
	float cosa = cosf( angle );

	matrix3_t m = {
		{ cosa, sina, 0.0f },
		{ -sina, cosa, 0.0f },
		{ cmd->x + cmd->w, cmd->y, 1.0f }
	};

	backEndData->dynamicGeometryBuilder.checkOverflow( 4, 6 );

	int a = backEndData->dynamicGeometryBuilder.addVertex();
	vertex = &backEndData->dynamicGeometryBuilder.vertexes[a];
	vertex->position.x = m[0][0] * ( -cmd->w ) + m[2][0];
	vertex->position.y = m[0][1] * ( -cmd->w ) + m[2][1];
	vertex->position.z = 0;
	vertex->texCoord0.x = cmd->s1;
	vertex->texCoord0.y = cmd->t1;
	vertex->vertexColor = backEnd.color2D;

	int b = backEndData->dynamicGeometryBuilder.addVertex();
	vertex = &backEndData->dynamicGeometryBuilder.vertexes[b];
	vertex->position.x = m[2][0];
	vertex->position.y = m[2][1];
	vertex->position.z = 0;
	vertex->texCoord0.x = cmd->s2;
	vertex->texCoord0.y = cmd->t1;
	vertex->vertexColor = backEnd.color2D;

	int c = backEndData->dynamicGeometryBuilder.addVertex();
	vertex = &backEndData->dynamicGeometryBuilder.vertexes[c];
	vertex->position.x = m[1][0] * ( cmd->h ) + m[2][0];
	vertex->position.y = m[1][1] * ( cmd->h ) + m[2][1];
	vertex->position.z = 0;
	vertex->texCoord0.x = cmd->s2;
	vertex->texCoord0.y = cmd->t2;
	vertex->vertexColor = backEnd.color2D;

	int d = backEndData->dynamicGeometryBuilder.addVertex();
	vertex = &backEndData->dynamicGeometryBuilder.vertexes[d];
	vertex->position.x = m[0][0] * ( -cmd->w ) + m[1][0] * ( cmd->h ) + m[2][0];
	vertex->position.y = m[0][1] * ( -cmd->w ) + m[1][1] * ( cmd->h ) + m[2][1];
	vertex->position.z = 0;
	vertex->texCoord0.x = cmd->s1;
	vertex->texCoord0.y = cmd->t2;
	vertex->vertexColor = backEnd.color2D;

	backEndData->dynamicGeometryBuilder.addTriangle( a, c, b );
	backEndData->dynamicGeometryBuilder.addTriangle( a, d, c );

	return (const void *)( cmd + 1 );
}

/*
=============
RB_RotatePic2
=============
*/
const void *RB_RotatePic2( const void *data ) {
	tr_shader::vertex_t *vertex;
	const rotatePicCommand_t *cmd;
	shader_t *shader;
	drawCommand_t *draw;

	cmd = (const rotatePicCommand_t *)data;
	shader = cmd->shader;

	// FIXME is this needed
	if( shader->numUnfoggedPasses ) {
		if( shader != tess.shader ) {
			RB_EndSurface();
			backEnd.currentEntity = &backEnd.entity2D;
			RB_BeginSurface( shader, 0 );
		}

		backEndData->dynamicGeometryBuilder.checkOverflow( 4, 6 );

		float angle = DEG2RAD( cmd->a );
		float sina = sinf( angle );
		float cosa = cosf( angle );

		matrix3_t m = {
			{ cosa, sina, 0.0f },
			{ -sina, cosa, 0.0f },
			{ cmd->x, cmd->y, 1.0f }
		};

		int a = backEndData->dynamicGeometryBuilder.addVertex();
		vertex = &backEndData->dynamicGeometryBuilder.vertexes[a];
		vertex->position.x = m[0][0] * ( -cmd->w ) + m[2][0];
		vertex->position.y = m[0][1] * ( -cmd->w ) + m[2][1];
		vertex->position.z = 0;
		vertex->texCoord0.x = cmd->s1;
		vertex->texCoord0.y = cmd->t1;
		vertex->vertexColor = backEnd.color2D;

		int b = backEndData->dynamicGeometryBuilder.addVertex();
		vertex = &backEndData->dynamicGeometryBuilder.vertexes[b];
		vertex->position.x = m[2][0];
		vertex->position.y = m[2][1];
		vertex->position.z = 0;
		vertex->texCoord0.x = cmd->s2;
		vertex->texCoord0.y = cmd->t1;
		vertex->vertexColor = backEnd.color2D;

		int c = backEndData->dynamicGeometryBuilder.addVertex();
		vertex = &backEndData->dynamicGeometryBuilder.vertexes[c];
		vertex->position.x = m[1][0] * ( cmd->h ) + m[2][0];
		vertex->position.y = m[1][1] * ( cmd->h ) + m[2][1];
		vertex->position.z = 0;
		vertex->texCoord0.x = cmd->s2;
		vertex->texCoord0.y = cmd->t2;
		vertex->vertexColor = backEnd.color2D;

		int d = backEndData->dynamicGeometryBuilder.addVertex();
		vertex = &backEndData->dynamicGeometryBuilder.vertexes[d];
		vertex->position.x = m[0][0] * ( -cmd->w ) + m[1][0] * ( cmd->h ) + m[2][0];
		vertex->position.y = m[0][1] * ( -cmd->w ) + m[1][1] * ( cmd->h ) + m[2][1];
		vertex->position.z = 0;
		vertex->texCoord0.x = cmd->s1;
		vertex->texCoord0.y = cmd->t2;
		vertex->vertexColor = backEnd.color2D;

		backEndData->dynamicGeometryBuilder.addTriangle( a, c, b );
		backEndData->dynamicGeometryBuilder.addTriangle( a, d, c );
	}

	return (const void *)( cmd + 1 );
}

/*
=============
RB_ScissorPic
=============
*/
const void *RB_Scissor( const void *data ) {
	const scissorCommand_t *cmd;
	VkRect2D scissorRect;

	cmd = (const scissorCommand_t *)data;

	if( cmd->x >= 0 ) {
		scissorRect.offset.x = cmd->x;
		scissorRect.offset.y = glConfig.vidHeight - cmd->y - cmd->h;
		scissorRect.extent.width = cmd->w;
		scissorRect.extent.height = cmd->h;
	}
	else {
		scissorRect.offset.x = 0;
		scissorRect.offset.y = 0;
		scissorRect.extent.width = glConfig.vidWidth;
		scissorRect.extent.height = glConfig.vidHeight;
	}

	vkCmdSetScissor( backEndData->cmdbuf, 0, 1, &scissorRect );

	return (const void *)( cmd + 1 );
}

/*
=============
RB_DrawSurfs

=============
*/
const void *RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t *cmd;

	// finish any 2D drawing if needed
	RB_EndSurface();

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	// update the viewParms buffer
	memcpy( &backEnd.viewParms.shaderData.projectionMatrix, backEnd.viewParms.projectionMatrix, sizeof( backEnd.viewParms.projectionMatrix ) );
	VK_UploadBuffer( backEnd.viewParms.buffer, (byte *)&backEnd.viewParms.shaderData, sizeof( backEnd.viewParms.shaderData ), 0 );

	R_BindDescriptorSet( TR_VIEW_SPACE, backEnd.viewParms.descriptorSet );

	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

	// Dynamic Glow/Flares:
	/*
		The basic idea is to render the glowing parts of the scene to an offscreen buffer, then take
		that buffer and blur it. After it is sufficiently blurred, re-apply that image back to
		the normal screen using a additive blending. To blur the scene I use a vertex program to supply
		four texture coordinate offsets that allow 'peeking' into adjacent pixels. In the register
		combiner (pixel shader), I combine the adjacent pixels using a weighting factor. - Aurelio
	*/

	// Render dynamic glowing/flaring objects.
	if( !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) && g_bDynamicGlowSupported && r_DynamicGlow->integer ) {

		// Render the glowing objects.
		g_bRenderGlowingObjects = true;
		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
		g_bRenderGlowingObjects = false;

		// Resize the viewport to the blur texture size.
		const int oldViewWidth = backEnd.viewParms.viewportWidth;
		const int oldViewHeight = backEnd.viewParms.viewportHeight;
		backEnd.viewParms.viewportWidth = r_DynamicGlowWidth->integer;
		backEnd.viewParms.viewportHeight = r_DynamicGlowHeight->integer;
		SetViewportAndScissor( 0 );

		// Blur the scene.
		RB_BlurGlowTexture();

		// Draw the glow additively over the screen.
		RB_DrawGlowOverlay();
	}

	return (const void *)( cmd + 1 );
}


/*
=============
RB_DrawBuffer

=============
*/
const void *RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t *cmd;
	VkClearColorValue colorClearValue = { 0, 0, 0, 1 };

	cmd = (const drawBufferCommand_t *)data;

	// Stereo rendering not supported.
	assert( cmd->buffer == 0 );
	backEndData->imageArraySlice = cmd->buffer;

	// clear screen for debugging
	if( !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) && tr.world && tr.refdef.rdflags & RDF_doLAGoggles ) {
		const fog_t *fog = &tr.world->fogs[tr.world->numfogs];

		colorClearValue.float32[0] = fog->parms.color[0];
		colorClearValue.float32[1] = fog->parms.color[1];
		colorClearValue.float32[2] = fog->parms.color[2];
		colorClearValue.float32[3] = 1.0f;
	}
	else if( !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) && tr.world && tr.world->globalFog != -1 && tr.sceneCount ) // don't clear during menus, wait for real scene
	{
		const fog_t *fog = &tr.world->fogs[tr.world->globalFog];

		colorClearValue.float32[0] = fog->parms.color[0];
		colorClearValue.float32[1] = fog->parms.color[1];
		colorClearValue.float32[2] = fog->parms.color[2];
		colorClearValue.float32[3] = 1.0f;
	}
	else if( r_clear->integer ) { // clear screen for debugging
		int i = r_clear->integer;
		if( i == 42 ) {
			i = Q_irand( 0, 8 );
		}
		switch( i ) {
		default:
			colorClearValue.float32[0] = 1;
			colorClearValue.float32[1] = 0;
			colorClearValue.float32[2] = 0.5f;
			colorClearValue.float32[3] = 1;
			break;
		case 1: // red
			colorClearValue.float32[0] = 1;
			colorClearValue.float32[1] = 0;
			colorClearValue.float32[2] = 0;
			colorClearValue.float32[3] = 1;
			break;
		case 2: // green
			colorClearValue.float32[0] = 0;
			colorClearValue.float32[1] = 1;
			colorClearValue.float32[2] = 0;
			colorClearValue.float32[3] = 1;
			break;
		case 3: // yellow
			colorClearValue.float32[0] = 1;
			colorClearValue.float32[1] = 1;
			colorClearValue.float32[2] = 0;
			colorClearValue.float32[3] = 1;
			break;
		case 4: // blue
			colorClearValue.float32[0] = 0;
			colorClearValue.float32[1] = 0;
			colorClearValue.float32[2] = 1;
			colorClearValue.float32[3] = 1;
			break;
		case 5: // cyan
			colorClearValue.float32[0] = 0;
			colorClearValue.float32[1] = 1;
			colorClearValue.float32[2] = 1;
			colorClearValue.float32[3] = 1;
			break;
		case 6: // magenta
			colorClearValue.float32[0] = 1;
			colorClearValue.float32[1] = 0;
			colorClearValue.float32[2] = 1;
			colorClearValue.float32[3] = 1;
			break;
		case 7: // white
			colorClearValue.float32[0] = 1;
			colorClearValue.float32[1] = 1;
			colorClearValue.float32[2] = 1;
			colorClearValue.float32[3] = 1;
			break;
		case 8: // black
			colorClearValue.float32[0] = 0;
			colorClearValue.float32[1] = 0;
			colorClearValue.float32[2] = 0;
			colorClearValue.float32[3] = 1;
			break;
		}
	}

	// the buffer will be cleared at the beginning of the render pass
	backEndData->defaultClearValue = colorClearValue;

	return (const void *)( cmd + 1 );
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages( void ) {
	image_t *image;
	float x, y, w, h;
	// int		start, end;

	// start = ri.Milliseconds();

	VK_SetImageLayout( backEndData->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT );

	int i = 0;
	//	int iNumImages =
	R_Images_StartIteration();
	while( ( image = R_Images_GetNextIteration() ) != NULL ) {
		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if( r_showImages->integer == 2 ) {
			w *= image->width / 512.0;
			h *= image->height / 512.0;
		}

		VkImageBlit blitRegion = {};

		blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blitRegion.srcSubresource.layerCount = 1;
		blitRegion.srcOffsets[1].x = image->width;
		blitRegion.srcOffsets[1].y = image->height;
		blitRegion.srcOffsets[1].z = 1;

		blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blitRegion.dstSubresource.layerCount = 1;
		blitRegion.dstOffsets[0].x = x;
		blitRegion.dstOffsets[0].y = y;
		blitRegion.dstOffsets[1].x = x + w;
		blitRegion.dstOffsets[1].y = y + h;
		blitRegion.dstOffsets[1].z = 1;

		VK_SetImageLayout( image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT );
		vkCmdBlitImage( backEndData->cmdbuf,
			image->tex,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			backEndData->image->tex,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blitRegion,
			VK_FILTER_LINEAR );

		i++;
	}

	// end = ri.Milliseconds();
	// ri.Printf( PRINT_ALL, "%i msec to draw all images\n", end - start );
}


/*
=============
RB_SwapBuffers

=============
*/
extern void RB_RenderWorldEffects( void );
const void *RB_SwapBuffers( const void *data ) {
	const swapBuffersCommand_t *cmd;

	// finish any 2D drawing if needed
	RB_EndSurface();

	// texture swapping test
	if( r_showImages->integer ) {
		RB_ShowImages();
	}

	cmd = (const swapBuffersCommand_t *)data;

#if 0
	// we measure overdraw by reading back the stencil buffer and
	// counting up the number of increments that have happened
	if( r_measureOverdraw->integer ) {
		int i;
		long sum = 0;
		unsigned char *stencilReadback;

		stencilReadback = (unsigned char *)R_Malloc( glConfig.vidWidth * glConfig.vidHeight, TAG_TEMP_WORKSPACE, qfalse );
		qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback );

		for( i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++ ) {
			sum += stencilReadback[i];
		}

		backEnd.pc.c_overDraw += sum;
		R_Free( stencilReadback );
	}
#endif

	GLimp_LogComment( "***************** RB_SwapBuffers *****************\n\n\n" );

	VK_EndFrame();

	ri.WIN_Present( &window );

	backEnd.projection2D = qfalse;

	return (const void *)( cmd + 1 );
}

const void *RB_WorldEffects( const void *data ) {
	const setModeCommand_t *cmd;

	cmd = (const setModeCommand_t *)data;

	// Always flush the tess buffer
	if( tess.shader && tess.numDraws ) {
		RB_EndSurface();
	}
	RB_RenderWorldEffects();

	if( tess.shader ) {
		RB_BeginSurface( tess.shader, tess.fogNum );
	}

	return (const void *)( cmd + 1 );
}

/*
====================
RB_ExecuteRenderCommands
====================
*/
void RB_ExecuteRenderCommands( const void *data ) {
	int t1, t2;

	t1 = ri.Milliseconds();

	while( 1 ) {
		data = PADP( data, sizeof( void * ) );

		switch( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_ROTATE_PIC:
			data = RB_RotatePic( data );
			break;
		case RC_ROTATE_PIC2:
			data = RB_RotatePic2( data );
			break;
		case RC_SCISSOR:
			data = RB_Scissor( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			break;
		case RC_WORLD_EFFECTS:
			data = RB_WorldEffects( data );
			break;
		case RC_END_OF_LIST:
		default:
			// stop rendering
			t2 = ri.Milliseconds();
			backEnd.pc.msec = t2 - t1;
			return;
		}
	}
}

// Hack variable for deciding which kind of texture rectangle thing to do (for some
// reason it acts different on radeon! It's against the spec!).
extern bool g_bTextureRectangleHack;

static inline void RB_BlurGlowTexture() {
	image_t *glow = tr.glowFrameBuffer->images[0].i;

	/////////////////////////////////////////////////////////
	// Setup vertex and pixel programs.
	/////////////////////////////////////////////////////////

	// NOTE: The 0.25 is because we're blending 4 textures (so = 1.0) and we want a relatively normalized pixel
	// intensity distribution, but this won't happen anyways if intensity is higher than 1.0.
	float fBlurDistribution = r_DynamicGlowIntensity->value * 0.25f;
	float fBlurWeight[4] = { fBlurDistribution, fBlurDistribution, fBlurDistribution, 1.0f };

	// end the glow render pass before transitioning the image to shader read-only layout
	R_BindFrameBuffer( NULL );
	VK_SetImageLayout( glow, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT );

	// begin the post-process render pass
	R_BindFrameBuffer( tr.glowBlurFrameBuffer );

	vkCmdBindPipeline( backEndData->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, tr.glowBlurPipeline );
	backEndData->pipeline = tr.glowBlurPipeline;
	backEndData->pipelineLayout = tr.glowBlurPipelineLayout;

	VK_BindImage( glow );

	vkCmdPushConstants( backEndData->cmdbuf, tr.glowBlurPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( fBlurWeight ), fBlurWeight );

	/////////////////////////////////////////////////////////
	// Draw the blur passes (each pass blurs it more, increasing the blur radius ).
	/////////////////////////////////////////////////////////

	// How much to offset each texel by.
	float fTexelWidthOffset = 0.1f, fTexelHeightOffset = 0.1f;

	// int iTexWidth = backEnd.viewParms.viewportWidth, iTexHeight = backEnd.viewParms.viewportHeight;
	int iTexWidth = glConfig.vidWidth, iTexHeight = glConfig.vidHeight;

	for( int iNumBlurPasses = 0; iNumBlurPasses < r_DynamicGlowPasses->integer; iNumBlurPasses++ ) {
		// Load the Texel Offsets into the Vertex Program.
		float fTexelOffsets[16] = {
			-fTexelWidthOffset, -fTexelWidthOffset, 0.0f, 0.0f,
			-fTexelWidthOffset, fTexelWidthOffset, 0.0f, 0.0f,
			fTexelWidthOffset, -fTexelWidthOffset, 0.0f, 0.0f,
			fTexelWidthOffset, fTexelWidthOffset, 0.0f, 0.0f
		};

		vkCmdPushConstants( backEndData->cmdbuf, tr.glowBlurPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( fTexelOffsets ), fTexelOffsets );

		// After first pass put the tex coords to the viewport size.
		if( iNumBlurPasses == 1 ) {
#if 0
			uiTex = tr.blurImage;
			qglActiveTextureARB( GL_TEXTURE3_ARB );
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_ARB );
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );
			qglActiveTextureARB( GL_TEXTURE2_ARB );
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_ARB );
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );
			qglActiveTextureARB( GL_TEXTURE1_ARB );
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_ARB );
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );
			qglActiveTextureARB( GL_TEXTURE0_ARB );
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_ARB );
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );

			// Copy the current image over.
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );
			qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
#endif
		}

		// Draw the fullscreen quad.
		vkCmdDraw( backEndData->cmdbuf, 3, 1, 0, 0 );

#if 0
		qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, tr.blurImage );
		qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
#endif

		// Increase the texel offsets.
		// NOTE: This is possibly the most important input to the effect. Even by using an exponential function I've been able to
		// make it look better (at a much higher cost of course). This is cheap though and still looks pretty great. In the future
		// I might want to use an actual gaussian equation to correctly calculate the pixel coefficients and attenuates, texel
		// offsets, gaussian amplitude and radius...
		fTexelWidthOffset += r_DynamicGlowDelta->value;
		fTexelHeightOffset += r_DynamicGlowDelta->value;
	}
}

// Draw the glow blur over the screen additively.
static inline void RB_DrawGlowOverlay() {
	image_t *glow = tr.glowBlurFrameBuffer->images[0].i;

	// end the glow blur render pass before transitioning the blurred image to shader read-only layout
	R_BindFrameBuffer( NULL );
	VK_SetImageLayout( glow, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT );

	R_BindFrameBuffer( tr.postProcessFrameBuffer );

	// additively render the glow texture
	vkCmdBindPipeline( backEndData->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, tr.glowCombinePipeline );
	backEndData->pipeline = tr.glowCombinePipeline;
	backEndData->pipelineLayout = tr.glowCombinePipelineLayout;

	VK_BindImage( glow );

	vkCmdDraw( backEndData->cmdbuf, 3, 1, 0, 0 );
}

/*
====================
R_BindFrameBuffer
====================
*/
void R_BindFrameBuffer( frameBuffer_t *frameBuffer ) {
	if( backEndData->frameBuffer == frameBuffer ) {
		return;
	}

	if( backEndData->frameBuffer ) {
		vkCmdEndRenderPass( backEndData->cmdbuf );
	}

	if( frameBuffer ) {
		for( int i = 0; i < frameBuffer->numImages; ++i ) {
			VK_SetImageLayout( frameBuffer->images[i].i, frameBuffer->images[i].layout, 0 );
		}

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = frameBuffer->renderPass;
		renderPassBeginInfo.framebuffer = frameBuffer->buf;
		renderPassBeginInfo.renderArea.extent.width = frameBuffer->width;
		renderPassBeginInfo.renderArea.extent.height = frameBuffer->height;
		renderPassBeginInfo.clearValueCount = frameBuffer->numImages;
		renderPassBeginInfo.pClearValues = frameBuffer->clearValues;

		vkCmdBeginRenderPass( backEndData->cmdbuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );
	}

	// update the frame buffer
	backEndData->frameBuffer = frameBuffer;
}

void R_DeleteFrameBuffer( frameBuffer_t *frameBuffer ) {
	assert( frameBuffer );

	if( frameBuffer->buf ) {
		vkDestroyFramebuffer( vkState.device, frameBuffer->buf, NULL );
	}
	if( frameBuffer->renderPass ) {
		vkDestroyRenderPass( vkState.device, frameBuffer->renderPass, NULL );
	}
	for( int i = 0; i < frameBuffer->numImages; ++i ) {
		if( !frameBuffer->images[i].external ) {
			R_Images_DeleteImage( frameBuffer->images[i].i );
		}
	}
	R_Free( frameBuffer );
}

void R_ClearFrameBuffer( frameBuffer_t *frameBuffer ) {
	image_t *image;
	VkClearValue *clearValue;
	int i;

	for( i = 0; i < frameBuffer->numImages; ++i ) {
		image = frameBuffer->images[i].i;
		clearValue = &frameBuffer->clearValues[i];

		// transition the image to transfer dst optimal layout
		VK_SetImageLayout( image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT );

		if( i == frameBuffer->depthBufferIndex ) {
			VK_ClearDepthStencilImage( image, &clearValue->depthStencil );
		}
		else {
			VK_ClearColorImage( image, &clearValue->color );
		}
	}
}
