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

#define TR_SHADERS_BEGIN                                                                      \
	/* enforce member alignment to 4 bytes to safely cast from unsigned char* to uint32_t* */ \
	__pragma( pack( push, 4 ) );                                                              \
	static struct alignas( alignof( uint32_t ) ) {
#define TR_SHADERS_END \
	}                  \
	g_scShaders;       \
	__pragma( pack( pop ) )
#define TR_INCLUDE_SHADER inline static

#include "tr_blur_combine_VS.h"
#include "tr_blur_combine_PS.h"
#include "tr_wireframe_VS.h"
#include "tr_wireframe_PS.h"
#include "tr_shade_VS.h"
#include "tr_shade_PS.h"

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

void SPV_InitDescriptorSetLayouts( void ) {
	CDescriptorSetLayoutBuilder descriptorSetLayoutBuilder;

	// space 0
	descriptorSetLayoutBuilder.addBinding( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ); // tr
	descriptorSetLayoutBuilder.addBinding( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ); // tr_funcs
	descriptorSetLayoutBuilder.addBinding( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ); // tr_lightGridData
	descriptorSetLayoutBuilder.addBinding( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ); // tr_lightGridArray
	descriptorSetLayoutBuilder.addBinding( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ); // tr_fogs
	descriptorSetLayoutBuilder.addBinding( VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );	// tr_noise
	descriptorSetLayoutBuilder.build( &tr.commonDescriptorSetLayout );

	// space 1
	descriptorSetLayoutBuilder.reset();
	descriptorSetLayoutBuilder.addBinding( tr.pointClampSampler );
	descriptorSetLayoutBuilder.addBinding( tr.pointWrapSampler );
	descriptorSetLayoutBuilder.addBinding( tr.linearClampSampler );
	descriptorSetLayoutBuilder.addBinding( tr.linearWrapSampler );
	descriptorSetLayoutBuilder.build( &tr.samplerDescriptorSetLayout );

	// space 2
	descriptorSetLayoutBuilder.reset();
	descriptorSetLayoutBuilder.addBinding( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	descriptorSetLayoutBuilder.build( &tr.shaderDescriptorSetLayout );

	// space 3
	descriptorSetLayoutBuilder.build( &tr.modelDescriptorSetLayout );
}

void SPV_InitGlowShaders( void ) {
	CPipelineLayoutBuilder pipelineLayoutBuilder;
	CPipelineBuilder pipelineBuilder;

	// create the blur pipeline layout
	pipelineLayoutBuilder.addPushConstantRange( VK_SHADER_STAGE_VERTEX_BIT, 64 );
	pipelineLayoutBuilder.addPushConstantRange( VK_SHADER_STAGE_FRAGMENT_BIT, 16 );
	pipelineLayoutBuilder.build( &tr.glowBlurPipelineLayout );

	// create the pipeline
	pipelineBuilder.pipelineCreateInfo.layout = tr.glowBlurPipelineLayout;
	pipelineBuilder.pipelineCreateInfo.renderPass = tr.postProcessFrameBuffer->renderPass;
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
	pipelineLayoutBuilder.build( &tr.glowCombinePipelineLayout );

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
	pipelineBuilder.build( &tr.glowCombinePipeline );
}

void SPV_InitWireframeShaders( void ) {
	CPipelineLayoutBuilder pipelineLayoutBuilder;
	CPipelineBuilder pipelineBuilder;

	// create the pipeline layout
	pipelineLayoutBuilder.addPushConstantRange( VK_SHADER_STAGE_VERTEX_BIT, 12 );
	pipelineLayoutBuilder.build( &tr.wireframePipelineLayout );

	// create the wireframe pipeline
	pipelineBuilder.pipelineCreateInfo.layout = tr.wireframePipelineLayout;
	pipelineBuilder.pipelineCreateInfo.renderPass = tr.sceneFrameBuffer->renderPass;
	pipelineBuilder.pipelineCreateInfo.subpass = 0;

	// setup the vertex input
	pipelineBuilder.vertexBinding.binding = 0;
	pipelineBuilder.vertexBinding.stride = sizeof( tr_shader::vertex_t );
	pipelineBuilder.vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	pipelineBuilder.addVertexAttributes<tr_shader::vertex_t>();

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
	pipelineBuilder.build( &tr.wireframePipeline );

	// create the xray wireframe pipeline
	pipelineBuilder.depthStencil.depthTestEnable = VK_FALSE;
	pipelineBuilder.rasterization.cullMode = VK_CULL_MODE_NONE;

	pipelineBuilder.build( &tr.wireframeXRayPipeline );
}

void SPV_InitShadeShaders( void ) {
	CPipelineLayoutBuilder pipelineLayoutBuilder;
	CPipelineBuilder pipelineBuilder;

	// use the default pipeline layout
	pipelineLayoutBuilder.build( &tr.shadePipelineLayout );

	pipelineBuilder.pipelineCreateInfo.layout = tr.shadePipelineLayout;
	pipelineBuilder.pipelineCreateInfo.renderPass = tr.sceneFrameBuffer->renderPass;
	pipelineBuilder.pipelineCreateInfo.subpass = 0;

	// setup the pipeline shader stages
	pipelineBuilder.setShader( VK_SHADER_STAGE_VERTEX_BIT, tr_shade_VS );
	pipelineBuilder.setShader( VK_SHADER_STAGE_FRAGMENT_BIT, tr_shade_PS );

	// setup the vertex input
	pipelineBuilder.vertexBinding.binding = 0;
	pipelineBuilder.vertexBinding.stride = sizeof( tr_shader::vertex_t );
	pipelineBuilder.vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	pipelineBuilder.addVertexAttributes<tr_shader::vertex_t>();

	// setup the color blend state
	VkPipelineColorBlendAttachmentState attachmentBlend = {};
	pipelineBuilder.colorBlend.attachmentCount = 1;
	pipelineBuilder.colorBlend.pAttachments = &attachmentBlend;
	attachmentBlend.blendEnable = VK_FALSE;
	attachmentBlend.colorWriteMask = 0xF;

	// create the shade pipeline
	pipelineBuilder.build( &tr.shadePipeline );
}
