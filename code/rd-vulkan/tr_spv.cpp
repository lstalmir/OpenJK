/*
===========================================================================
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

// tr_glow.c -- this file deals with the arb shaders for dynamic glow

#include "tr_local.h"

#include "tr_blur_combine_VS.h"
#include "tr_blur_combine_PS.h"
#include "tr_wireframe_VS.h"
#include "tr_wireframe_PS.h"
#include "tr_shade_VS.h"
#include "tr_shade_PS.h"
#include "tr_shade_md3_VS.h"
#include "tr_shade_ghoul2_VS.h"
#include "tr_skybox_VS.h"
#include "tr_skybox_PS.h"
#include "tr_skybox_fog_PS.h"
#include "tr_fxaa_VS.h"
#include "tr_fxaa_PS.h"

#include <unordered_map>

/*
** Pipeline cache
*/
std::unordered_map<int, pipelineState_t> CompiledPipelines;

/**
===============
SPV_FindShaderModuleFile

===============
*/
VkShaderModule SPV_FindShaderModuleFile( const char *name ) {
	VkShaderModule shaderModule;
	long shaderDataLength;
	void *shaderData;

	if( strlen( name ) >= MAX_QPATH ) {
		Com_Error( ERR_DROP, "SPV_FindShaderModuleFile: \"%s\" is too long\n", name );
	}

	shaderDataLength = ri.FS_ReadFile( name, &shaderData );
	if( shaderDataLength <= 0 ) {
		Com_Error( ERR_DROP, "SPV_FindShaderModuleFile: failed to read file \"%s\"\n", name );
	}

	// create the shader module object
	shaderModule = SPV_CreateShaderModule( (const uint32_t *)shaderData, shaderDataLength );

	ri.Z_Free( shaderData );

	return shaderModule;
}

/**
===============
SPV_CreateShaderModule

===============
*/
VkShaderModule SPV_CreateShaderModule( const uint32_t *code, int codeSize ) {
	VkResult res;
	VkShaderModule module;

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.pCode = code;
	shaderModuleCreateInfo.codeSize = codeSize;

	res = vkCreateShaderModule( vkState.device, &shaderModuleCreateInfo, NULL, &module );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_DROP, "SPV_CreateShaderModule: failed to create vertex shader module (%d)\n", res );
	}

	return module;
}

/***********************************************************************************************************/

#define SHADER_CACHE_NAME PRODUCT_NAME "_shaders.bin"

void SPV_InitPipelineCache( void ) {
	void *data;
	long dataSize;
	VkResult res;

	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

	// read the cache from disk
	dataSize = ri.FS_ReadFile( SHADER_CACHE_NAME, &data );
	if( dataSize == -1 ) {
		dataSize = 0;
	}

	pipelineCacheCreateInfo.initialDataSize = (size_t)dataSize;
	pipelineCacheCreateInfo.pInitialData = data;

	res = vkCreatePipelineCache( vkState.device, &pipelineCacheCreateInfo, NULL, &vkState.pipelineCache );

	if( data ) {
		ri.FS_FreeFile( data );
	}
	if( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "SPV_InitPipelineCache: failed to create pipeline cache object (%d)\n", res );
	}
}

void SPV_InitGlowShaders( void ) {
	if( vkState.glowBlurPipeline.handle )
		return;

	CPipelineLayoutBuilder pipelineLayoutBuilder;
	CPipelineBuilder pipelineBuilder;

	// create the blur pipeline layout
	pipelineLayoutBuilder.addPushConstantRange( VK_SHADER_STAGE_VERTEX_BIT, 64 );
	pipelineLayoutBuilder.addPushConstantRange( VK_SHADER_STAGE_FRAGMENT_BIT, 16 );
	pipelineLayoutBuilder.build( &vkState.glowBlurPipelineLayout );

	// create the pipeline
	pipelineBuilder.layout = &vkState.glowBlurPipelineLayout;
	pipelineBuilder.pipelineCreateInfo.renderPass = tres.postProcessFrameBuffer->renderPass;
	pipelineBuilder.pipelineCreateInfo.subpass = 0;

#if 0
	// setup the pipeline shader stages
	pipelineBuilder.setShader( VK_SHADER_STAGE_VERTEX_BIT, g_spvGlowVShader );
	pipelineBuilder.setShader( VK_SHADER_STAGE_FRAGMENT_BIT, g_spvGlowPShader );
#endif

	// setup the depth state
	pipelineBuilder.depthStencil.depthTestEnable = VK_FALSE;
	pipelineBuilder.depthStencil.depthWriteEnable = VK_FALSE;

	// setup the color blend state
	VkPipelineColorBlendAttachmentState attachmentBlend = {};
	pipelineBuilder.colorBlend.attachmentCount = 1;
	pipelineBuilder.colorBlend.pAttachments = &attachmentBlend;
	attachmentBlend.blendEnable = VK_FALSE;
	attachmentBlend.colorWriteMask = 0xF;

#if 0
	// create the blur pipeline
	pipelineBuilder.build( &tr.glowBlurPipeline );
#endif

	// create the combine pipeline layout
	pipelineLayoutBuilder.reset();
	pipelineLayoutBuilder.build( &vkState.glowCombinePipelineLayout );

	// setup the blend state for combining the blur result
	attachmentBlend.blendEnable = VK_TRUE;
	attachmentBlend.alphaBlendOp = VK_BLEND_OP_ADD;
	attachmentBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	attachmentBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	attachmentBlend.colorBlendOp = VK_BLEND_OP_ADD;
	attachmentBlend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	attachmentBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;

	// One and Inverse Src Color give a very soft addition, while one one is a bit stronger. With one one we can
	// use additive blending through multitexture though.
	if( r_DynamicGlowSoft->integer ) {
		attachmentBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	}

	pipelineBuilder.shaderStageCount = 0;
	pipelineBuilder.setShader( VK_SHADER_STAGE_VERTEX_BIT, tr_blur_combine_VS );
	pipelineBuilder.setShader( VK_SHADER_STAGE_FRAGMENT_BIT, tr_blur_combine_PS );

	// create the combine pipeline
	pipelineBuilder.build( &vkState.glowCombinePipeline );
}

void SPV_InitWireframeShaders( void ) {
	if( vkState.wireframePipeline.handle )
		return;

	CPipelineLayoutBuilder pipelineLayoutBuilder;
	CPipelineBuilder pipelineBuilder;

	// create the pipeline layout
	pipelineLayoutBuilder.addPushConstantRange( VK_SHADER_STAGE_VERTEX_BIT, 12 );
	pipelineLayoutBuilder.build( &vkState.wireframePipelineLayout );
	VK_SetDebugObjectName( vkState.wireframePipelineLayout.handle, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "tr.wireframePipelineLayout" );

	// create the wireframe pipeline
	pipelineBuilder.layout = &vkState.wireframePipelineLayout;
	pipelineBuilder.pipelineCreateInfo.renderPass = tres.sceneFrameBuffer->renderPass;
	pipelineBuilder.pipelineCreateInfo.subpass = 0;

	// setup the vertex input
	pipelineBuilder.addVertexAttributesAndBinding<tr_shader::vertex_t>();

	// setup the pipeline shader stages
	pipelineBuilder.setShader( VK_SHADER_STAGE_VERTEX_BIT, tr_wireframe_VS );
	pipelineBuilder.setShader( VK_SHADER_STAGE_FRAGMENT_BIT, tr_wireframe_PS );

	// setup the rasterizer
	pipelineBuilder.rasterization.polygonMode = VK_POLYGON_MODE_LINE;

	// setup the depth state
	pipelineBuilder.depthStencil.depthWriteEnable = VK_FALSE;
	pipelineBuilder.depthStencil.depthCompareOp = VK_COMPARE_OP_EQUAL;

	// setup the color blend state
	VkPipelineColorBlendAttachmentState attachmentBlend = {};
	pipelineBuilder.colorBlend.attachmentCount = 1;
	pipelineBuilder.colorBlend.pAttachments = &attachmentBlend;
	attachmentBlend.blendEnable = VK_FALSE;
	attachmentBlend.colorWriteMask = 0xF;

	// create the wireframe pipeline
	pipelineBuilder.build( &vkState.wireframePipeline );

	// create the xray wireframe pipeline
	pipelineBuilder.depthStencil.depthTestEnable = VK_FALSE;
	pipelineBuilder.rasterization.cullMode = VK_CULL_MODE_NONE;

	pipelineBuilder.build( &vkState.wireframeXRayPipeline );
}

void SPV_InitSkyboxShaders( void ) {
	if( vkState.skyboxPipeline.handle )
		return;

	CPipelineLayoutBuilder pipelineLayoutBuilder;
	CPipelineBuilder pipelineBuilder;

	// create the pipeline layout
	pipelineLayoutBuilder.build( &vkState.skyboxPipelineLayout );
	VK_SetDebugObjectName( vkState.skyboxPipelineLayout.handle, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "tr.skyboxPipelineLayout" );

	// create the skybox pipeline
	pipelineBuilder.layout = &vkState.skyboxPipelineLayout;
	pipelineBuilder.pipelineCreateInfo.renderPass = tres.sceneFrameBuffer->renderPass;
	pipelineBuilder.pipelineCreateInfo.subpass = 0;

	// setup the vertex input
	pipelineBuilder.addVertexAttributesAndBinding<tr_shader::vertex_t>();

	// setup the pipeline shader stages
	pipelineBuilder.setShader( VK_SHADER_STAGE_VERTEX_BIT, tr_skybox_VS );
	pipelineBuilder.setShader( VK_SHADER_STAGE_FRAGMENT_BIT, tr_skybox_PS );

	// setup the rasterizer
	pipelineBuilder.rasterization.cullMode = VK_CULL_MODE_NONE;

	// setup the depth state
	pipelineBuilder.depthStencil.depthTestEnable = VK_TRUE;
	pipelineBuilder.depthStencil.depthWriteEnable = VK_FALSE;
	pipelineBuilder.depthStencil.depthCompareOp = VK_COMPARE_OP_EQUAL;

	// setup the color blend state
	VkPipelineColorBlendAttachmentState attachmentBlend = {};
	pipelineBuilder.colorBlend.attachmentCount = 1;
	pipelineBuilder.colorBlend.pAttachments = &attachmentBlend;
	attachmentBlend.blendEnable = VK_FALSE;
	attachmentBlend.colorWriteMask = 0xF;

	// create the skybox pipeline
	pipelineBuilder.build( &vkState.skyboxPipeline );

	// override pixel shader for skybox fog pipeline
	pipelineBuilder.setShader( VK_SHADER_STAGE_FRAGMENT_BIT, tr_skybox_fog_PS );
	// disable depth test
	pipelineBuilder.depthStencil.depthTestEnable = VK_FALSE;
	pipelineBuilder.depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
	// blend the fog with color output
	attachmentBlend.blendEnable = VK_TRUE;
	attachmentBlend.colorBlendOp = VK_BLEND_OP_ADD;
	attachmentBlend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	attachmentBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	attachmentBlend.alphaBlendOp = VK_BLEND_OP_ADD;
	attachmentBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	attachmentBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	// create the skybox fog pipeline
	pipelineBuilder.pipelineCreateInfo.renderPass = tres.skyFogFrameBuffer->renderPass;
	pipelineBuilder.build( &vkState.skyboxFogPipeline );
}

void SPV_InitAntialiasingShaders( void ) {
	if( r_antialiasing && !r_antialiasing->integer ) {
		// antialiasing disabled
		return;
	}

	if( vkState.antialiasingPipeline.handle ) {
		return;
	}

	CPipelineLayoutBuilder pipelineLayoutBuilder;
	CPipelineBuilder pipelineBuilder;

	// create the pipeline layout
	pipelineLayoutBuilder.build( &vkState.antialiasingPipelineLayout );
	VK_SetDebugObjectName( vkState.antialiasingPipelineLayout.handle, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "tr.antialiasingPipelineLayout" );

	// create the TAA pipeline
	pipelineBuilder.layout = &vkState.antialiasingPipelineLayout;
	pipelineBuilder.pipelineCreateInfo.renderPass = tres.antialiasingFrameBuffer->renderPass;
	pipelineBuilder.pipelineCreateInfo.subpass = 0;

	// setup the pipeline shader stages
	pipelineBuilder.setShader( VK_SHADER_STAGE_VERTEX_BIT, tr_fxaa_VS );
	pipelineBuilder.setShader( VK_SHADER_STAGE_FRAGMENT_BIT, tr_fxaa_PS );

	// setup the rasterizer
	pipelineBuilder.rasterization.cullMode = VK_CULL_MODE_NONE;

	// setup the depth state
	pipelineBuilder.depthStencil.depthTestEnable = VK_FALSE;
	pipelineBuilder.depthStencil.depthWriteEnable = VK_FALSE;
	pipelineBuilder.depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

	// setup the color blend state
	VkPipelineColorBlendAttachmentState attachmentBlend = {};
	attachmentBlend.blendEnable = VK_FALSE;
	attachmentBlend.colorWriteMask = 0xF;
	pipelineBuilder.colorBlend.attachmentCount = 1;
	pipelineBuilder.colorBlend.pAttachments = &attachmentBlend;

	// create the TAA pipeline
	pipelineBuilder.build( &vkState.antialiasingPipeline );
}

static void InitShadePipelineBuilder( CPipelineBuilder *builder, int spec ) {
	builder->layout = &vkState.shadePipelineLayout;
	builder->pipelineCreateInfo.renderPass = tres.sceneFrameBuffer->renderPass;
	builder->pipelineCreateInfo.subpass = 0;

	// set specialization constants
	builder->shaderSpec = spec;

	// setup the vertex input
	builder->addVertexAttributesAndBinding<tr_shader::vertex_t>();

	if( spec & TR_SHADER_SPEC_MD3 ) {
		// md3 shaders receive 2 vertex streams
		builder->addVertexAttributesAndBinding<tr_shader::oldVertex_t>();

		// use md3 pipeline shaders
		builder->setShader( VK_SHADER_STAGE_VERTEX_BIT, tr_shade_md3_VS );
	}
	else if( ( spec & TR_SHADER_SPEC_GLA ) || ( spec & TR_SHADER_SPEC_GLM ) ) {
		builder->layout = &vkState.ghoul2ShadePipelineLayout;

		// use vertex shader with bones
		builder->setShader( VK_SHADER_STAGE_VERTEX_BIT, tr_shade_ghoul2_VS );
	}
	else {
		// use default shade shader
		builder->setShader( VK_SHADER_STAGE_VERTEX_BIT, tr_shade_VS );
	}

	builder->setShader( VK_SHADER_STAGE_FRAGMENT_BIT, tr_shade_PS );
}

static VkBlendFactor GetBlendFactor( int blendStateBits ) {
	switch( blendStateBits ) {
	default:
	case GLS_SRCBLEND_ONE:
	case GLS_DSTBLEND_ONE:
		return VK_BLEND_FACTOR_ONE;
	case GLS_SRCBLEND_ZERO:
	case GLS_DSTBLEND_ZERO:
		return VK_BLEND_FACTOR_ZERO;
	case GLS_SRCBLEND_DST_COLOR:
		return VK_BLEND_FACTOR_DST_COLOR;
	case GLS_DSTBLEND_SRC_COLOR:
		return VK_BLEND_FACTOR_SRC_COLOR;
	case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case GLS_SRCBLEND_SRC_ALPHA:
	case GLS_DSTBLEND_SRC_ALPHA:
		return VK_BLEND_FACTOR_SRC_ALPHA;
	case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
	case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case GLS_SRCBLEND_DST_ALPHA:
	case GLS_DSTBLEND_DST_ALPHA:
		return VK_BLEND_FACTOR_DST_ALPHA;
	case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
	case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	case GLS_SRCBLEND_ALPHA_SATURATE:
		return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
	}
}

pipelineState_t *SPV_GetShadePipeline( int stateBits ) {
	pipelineState_t &pipeline = CompiledPipelines[stateBits];

	if( !pipeline.handle ) {
		CPipelineBuilder pipelineBuilder;

		int spec = 0;
		int input = stateBits & GLS_INPUT_BITS;
		switch( input ) {
		case GLS_INPUT_MD3:
			spec |= TR_SHADER_SPEC_MD3;
			break;
		case GLS_INPUT_GLM:
			spec |= TR_SHADER_SPEC_GLM;
			break;
		case GLS_INPUT_GLA:
			spec |= TR_SHADER_SPEC_GLA;
			break;
		}

		int alphaTest = stateBits & GLS_ATEST_BITS;
		switch( alphaTest ) {
		case GLS_ATEST_GT_0:
			spec |= TR_SHADER_SPEC_ATEST_GT_0;
			break;
		case GLS_ATEST_LT_80:
			spec |= TR_SHADER_SPEC_ATEST_LT_80;
			break;
		case GLS_ATEST_GE_80:
			spec |= TR_SHADER_SPEC_ATEST_GE_80;
			break;
		case GLS_ATEST_GE_C0:
			spec |= TR_SHADER_SPEC_ATEST_GE_C0;
			break;
		}

		InitShadePipelineBuilder( &pipelineBuilder, spec );

		// depth test
		if( stateBits & GLS_DEPTHTEST_DISABLE ) {
			pipelineBuilder.depthStencil.depthTestEnable = VK_FALSE;
		}
		else {
			pipelineBuilder.depthStencil.depthTestEnable = VK_TRUE;
		}

		if( stateBits & GLS_DEPTHMASK_TRUE ) {
			pipelineBuilder.depthStencil.depthWriteEnable = VK_TRUE;
		}
		else {
			pipelineBuilder.depthStencil.depthWriteEnable = VK_FALSE;
		}

		if( stateBits & GLS_DEPTHFUNC_EQUAL ) {
			pipelineBuilder.depthStencil.depthCompareOp = VK_COMPARE_OP_EQUAL;
		}
		else {
			pipelineBuilder.depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
		}

		// rasterization
		if( stateBits & GLS_POLYMODE_LINE ) {
			pipelineBuilder.rasterization.polygonMode = VK_POLYGON_MODE_LINE;
		}
		else {
			pipelineBuilder.rasterization.polygonMode = VK_POLYGON_MODE_FILL;
		}

		if( stateBits & GLS_CULL_NONE ) {
			pipelineBuilder.rasterization.cullMode = VK_CULL_MODE_NONE;
		}
		else {
			pipelineBuilder.rasterization.cullMode = VK_CULL_MODE_NONE; // todo
		}

		// color blend
		VkPipelineColorBlendAttachmentState colorBlend = {};
		colorBlend.blendEnable = VK_FALSE;
		colorBlend.colorWriteMask = 0xF;

		int srcBlendState = ( stateBits & GLS_SRCBLEND_BITS );
		int dstBlendState = ( stateBits & GLS_DSTBLEND_BITS );
		if( srcBlendState || dstBlendState ) {
			colorBlend.blendEnable = VK_TRUE;
			colorBlend.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlend.srcColorBlendFactor = GetBlendFactor( srcBlendState );
			colorBlend.dstColorBlendFactor = GetBlendFactor( dstBlendState );
			colorBlend.alphaBlendOp = VK_BLEND_OP_ADD;
			colorBlend.srcAlphaBlendFactor = colorBlend.srcColorBlendFactor;
			colorBlend.dstAlphaBlendFactor = colorBlend.dstColorBlendFactor;
		}

		pipelineBuilder.colorBlend.attachmentCount = 1;
		pipelineBuilder.colorBlend.pAttachments = &colorBlend;

		// create the pipeline
		pipelineBuilder.build( &pipeline );
		pipeline.stateBits = stateBits;

		char pipelineName[MAX_QPATH];
		sprintf( pipelineName, "shadePipeline (%08x)", stateBits );
		VK_SetDebugObjectName( pipeline.handle, VK_OBJECT_TYPE_PIPELINE, pipelineName );
	}

	return &pipeline;
}
