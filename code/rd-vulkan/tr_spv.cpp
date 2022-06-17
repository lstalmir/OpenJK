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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Vertex and Pixel Shader definitions.	- AReis
/***********************************************************************************************************/
// This vertex shader basically passes through most values and calculates no lighting. The only
// unusual thing it does is add the inputed texel offsets to all four texture units (this allows
// nearest neighbor pixel peeking).
const unsigned char g_strGlowVShaderARB[] =
{
	"!!ARBvp1.0\
	\
	# Input.\n\
	ATTRIB	iPos		= vertex.position;\
	ATTRIB	iColor		= vertex.color;\
	ATTRIB	iTex0		= vertex.texcoord[0];\
	ATTRIB	iTex1		= vertex.texcoord[1];\
	ATTRIB	iTex2		= vertex.texcoord[2];\
	ATTRIB	iTex3		= vertex.texcoord[3];\
	\
	# Output.\n\
	OUTPUT	oPos		= result.position;\
	OUTPUT	oColor		= result.color;\
	OUTPUT	oTex0		= result.texcoord[0];\
	OUTPUT	oTex1		= result.texcoord[1];\
	OUTPUT	oTex2		= result.texcoord[2];\
	OUTPUT	oTex3		= result.texcoord[3];\
	\
	# Constants.\n\
	PARAM	ModelViewProj[4]= { state.matrix.mvp };\
	PARAM	TexelOffset0	= program.env[0];\
	PARAM	TexelOffset1	= program.env[1];\
	PARAM	TexelOffset2	= program.env[2];\
	PARAM	TexelOffset3	= program.env[3];\
	\
	# Main.\n\
	DP4		oPos.x, ModelViewProj[0], iPos;\
	DP4		oPos.y, ModelViewProj[1], iPos;\
	DP4		oPos.z, ModelViewProj[2], iPos;\
	DP4		oPos.w, ModelViewProj[3], iPos;\
	MOV		oColor, iColor;\
	# Notice the optimization of using one texture coord instead of all four.\n\
	ADD		oTex0, iTex0, TexelOffset0;\
	ADD		oTex1, iTex0, TexelOffset1;\
	ADD		oTex2, iTex0, TexelOffset2;\
	ADD		oTex3, iTex0, TexelOffset3;\
	\
	END"
};

// This Pixel Shader loads four texture units and adds them all together (with a modifier
// multiplied to each in the process). The final output is r0 = t0 + t1 + t2 + t3.
const unsigned char g_strGlowPShaderARB[] =
{
	"!!ARBfp1.0\
	\
	# Input.\n\
	ATTRIB	iColor	= fragment.color.primary;\
	\
	# Output.\n\
	OUTPUT	oColor	= result.color;\
	\
	# Constants.\n\
	PARAM	Weight	= program.env[0];\
	TEMP	t0;\
	TEMP	t1;\
	TEMP	t2;\
	TEMP	t3;\
	TEMP	r0;\
	\
	# Main.\n\
	TEX		t0, fragment.texcoord[0], texture[0], RECT;\
	TEX		t1, fragment.texcoord[1], texture[1], RECT;\
	TEX		t2, fragment.texcoord[2], texture[2], RECT;\
	TEX		t3, fragment.texcoord[3], texture[3], RECT;\
	\
    MUL		r0, t0, Weight;\
	MAD		r0, t1, Weight, r0;\
	MAD		r0, t2, Weight, r0;\
	MAD		r0, t3, Weight, r0;\
	\
	MOV		oColor, r0;\
	\
	END"
};
/***********************************************************************************************************/


static VkShaderModule SPV_CreateShaderModule(const uint32_t* code, int codeSize) {
	VkResult res;
	VkShaderModule module;

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.pCode = code;
	shaderModuleCreateInfo.codeSize = codeSize;

	res = vkCreateShaderModule(vkState.device, &shaderModuleCreateInfo, NULL, &module);
	if (res != VK_SUCCESS) {
		Com_Error(ERR_FATAL, "SPV_InitGlowShaders: failed to create vertex shader module (%d)\n", res);
	}

	return module;
}

void SPV_InitGlowShaders(void) {
	VkResult res;

	// create the pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &tr.glowBlurDescriptorSetLayout;

	VkPushConstantRange pushConstants[2] = {};
	pipelineLayoutCreateInfo.pushConstantRangeCount = ARRAY_LEN(pushConstants);
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstants;

	VkPushConstantRange* vertexConstants = &pushConstants[0];
	vertexConstants->stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexConstants->offset = 0;
	vertexConstants->size = 64;

	VkPushConstantRange* fragmentConstants = &pushConstants[1];
	fragmentConstants->stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentConstants->offset = 0;
	fragmentConstants->size = 16;

	res = vkCreatePipelineLayout(vkState.device, &pipelineLayoutCreateInfo, NULL, &tr.glowBlurPipelineLayout);
	if (res != VK_SUCCESS) {
		Com_Error(ERR_FATAL, "SPV_InitGlowShaders: failed to create glow blur pipeline layout (%d)\n", res);
	}

	// create the glow pipeline
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.layout = tr.glowBlurPipelineLayout;
	pipelineCreateInfo.renderPass = tr.postProcessPass;
	pipelineCreateInfo.subpass = 0;

	// setup the pipeline shader stages
	VkPipelineShaderStageCreateInfo pipelineStages[2] = {};
	pipelineCreateInfo.stageCount = ARRAY_LEN(pipelineStages);
	pipelineCreateInfo.pStages = pipelineStages;

	// vertex shader
	VkPipelineShaderStageCreateInfo* vertexShaderStage = &pipelineStages[0];
	vertexShaderStage->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStage->stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStage->module = SPV_CreateShaderModule(g_strGlowVShaderARB, sizeof(g_strGlowVShaderARB));
	vertexShaderStage->pName = "main";

	// fragment shader
	VkPipelineShaderStageCreateInfo* fragmentShaderStage = &pipelineStages[1];
	fragmentShaderStage->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStage->module = SPV_CreateShaderModule(g_strGlowPShaderARB, sizeof(g_strGlowPShaderARB));
	fragmentShaderStage->pName = "main";

	// setup the rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	pipelineCreateInfo.pRasterizationState = &rasterizer;
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.lineWidth = 1.f;

	// setup the multisampling
	VkPipelineMultisampleStateCreateInfo multisample = {};
	pipelineCreateInfo.pMultisampleState = &multisample;
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// setup the depth state
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	pipelineCreateInfo.pDepthStencilState = &depthStencil;
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	// setup the color blend state
	VkPipelineColorBlendStateCreateInfo colorBlend = {};
	pipelineCreateInfo.pColorBlendState = &colorBlend;
	colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

	VkPipelineColorBlendAttachmentState attachmentBlend = {};
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments = &attachmentBlend;
	attachmentBlend.blendEnable = VK_FALSE;
	attachmentBlend.colorWriteMask = 0xF;

	// create the blur pipeline
	res = vkCreateGraphicsPipelines(vkState.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &tr.glowBlurPipeline);

	vkDestroyShaderModule(vkState.device, vertexShaderStage->module, NULL);
	vkDestroyShaderModule(vkState.device, fragmentShaderStage->module, NULL);

	if (res != VK_SUCCESS) {
		Com_Error(ERR_FATAL, "SPV_InitGlowShaders: failed to create glow blur graphics pipeline (%d)\n", res);
	}

	// create the combine pipeline
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;

	res = vkCreatePipelineLayout(vkState.device, &pipelineLayoutCreateInfo, NULL, &tr.glowCombinePipelineLayout);
	if (res != VK_SUCCESS) {
		Com_Error(ERR_FATAL, "SPV_InitGlowShaders: failed to create glow combine pipeline layout (%d)\n", res);
	}

	pipelineCreateInfo.layout = tr.glowCombinePipelineLayout;

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
	if (r_DynamicGlowSoft->integer) {
		attachmentBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	}

	vertexShaderStage->module = SPV_CreateShaderModule(g_strGlowVShaderARB, sizeof(g_strGlowVShaderARB));
	fragmentShaderStage->module = SPV_CreateShaderModule(g_strGlowPShaderARB, sizeof(g_strGlowPShaderARB));

	// create the combine pipeline
	res = vkCreateGraphicsPipelines(vkState.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &tr.glowCombinePipeline);

	vkDestroyShaderModule(vkState.device, vertexShaderStage->module, NULL);
	vkDestroyShaderModule(vkState.device, fragmentShaderStage->module, NULL);

	if (res != VK_SUCCESS) {
		Com_Error(ERR_FATAL, "SPV_InitGlowShaders: failed to create glow combine graphics pipeline (%d)\n", res);
	}
}
