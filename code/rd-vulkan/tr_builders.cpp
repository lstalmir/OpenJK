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

CDescriptorSetLayoutBuilder::CDescriptorSetLayoutBuilder() {
	reset();
}

void CDescriptorSetLayoutBuilder::reset() {
	bindingCount = 0;
}

void CDescriptorSetLayoutBuilder::addBinding( VkDescriptorType type ) {
	VkDescriptorSetLayoutBinding *binding;

	if( bindingCount == TR_MAX_DESCRIPTOR_SET_BINDING_COUNT ) {
		Com_Error( ERR_FATAL, "CDescriptorSetLayoutBuilder: max descriptor binding count limit reached\n" );
	}

	binding = &bindings[bindingCount];
	binding->binding = bindingCount;
	binding->descriptorType = type;
	binding->descriptorCount = 1;
	binding->stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	binding->pImmutableSamplers = NULL;
	bindingCount++;
}

void CDescriptorSetLayoutBuilder::addBinding( VkSampler &sampler ) {
	VkDescriptorSetLayoutBinding *binding;

	if( bindingCount == TR_MAX_DESCRIPTOR_SET_BINDING_COUNT ) {
		Com_Error( ERR_FATAL, "CDescriptorSetLayoutBuilder: max descriptor binding count limit reached\n" );
	}

	binding = &bindings[bindingCount];
	binding->binding = bindingCount;
	binding->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	binding->descriptorCount = 1;
	binding->stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	binding->pImmutableSamplers = &sampler;
	bindingCount++;
}

void CDescriptorSetLayoutBuilder::build( VkDescriptorSetLayout *layout ) {
	VkDescriptorSetLayoutCreateInfo createInfo;
	VkResult res;

	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.bindingCount = bindingCount;
	createInfo.pBindings = bindings;

	res = vkCreateDescriptorSetLayout( vkState.device, &createInfo, NULL, layout );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "CDescriptorSetLayoutBuilder: failed to create descriptor set layout (%d)\n", res );
	}
}

CPipelineLayoutBuilder::CPipelineLayoutBuilder() {
	reset();
}

void CPipelineLayoutBuilder::reset() {
	descriptorSetLayoutCount = 0;
	pushConstantRangeCount = 0;

	// add common descriptor set layouts
	addDescriptorSetLayout( vkState.commonDescriptorSetLayout );
	addDescriptorSetLayout( vkState.samplerDescriptorSetLayout );
	addDescriptorSetLayout( vkState.shaderDescriptorSetLayout );
	addDescriptorSetLayout( vkState.modelDescriptorSetLayout );
	addDescriptorSetLayout( vkState.textureDescriptorSetLayout );
	addDescriptorSetLayout( vkState.textureDescriptorSetLayout );
	addDescriptorSetLayout( vkState.viewDescriptorSetLayout );
}

void CPipelineLayoutBuilder::addDescriptorSetLayout( VkDescriptorSetLayout layout ) {
	if( descriptorSetLayoutCount == TR_MAX_DESCRIPTOR_SET_LAYOUT_COUNT ) {
		Com_Error( ERR_FATAL, "CPipelineLayoutBuilder: max descriptor set layout count limit reached\n" );
	}
	descriptorSetLayouts[descriptorSetLayoutCount++] = layout;
}

void CPipelineLayoutBuilder::addPushConstantRange( VkShaderStageFlags stages, int size, int offset ) {
	VkPushConstantRange *range;
	if( pushConstantRangeCount == TR_MAX_PUSH_CONSTANT_RANGE_COUNT ) {
		Com_Error( ERR_FATAL, "CPipelineLayoutBuilder: max push constant range count limit reached" );
	}
	range = &pushConstantRanges[pushConstantRangeCount++];
	range->stageFlags = stages;
	range->size = size;
	range->offset = offset;
}

void CPipelineLayoutBuilder::build( VkPipelineLayout *layout ) {
	VkPipelineLayoutCreateInfo createInfo;
	VkResult res;

	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.setLayoutCount = descriptorSetLayoutCount;
	createInfo.pSetLayouts = descriptorSetLayouts;
	createInfo.pushConstantRangeCount = pushConstantRangeCount;
	createInfo.pPushConstantRanges = pushConstantRanges;

	res = vkCreatePipelineLayout( vkState.device, &createInfo, NULL, layout );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "CPipelineLayoutBuilder: failed to create pipeline layout (%d)\n", res );
	}
}

CPipelineBuilder::CPipelineBuilder() {
	// these don't ever change
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.pVertexAttributeDescriptions = vertexAttributes;
	vertexInput.pVertexBindingDescriptions = vertexBindings;
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic.pDynamicStates = dynamicStates;

	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInput;
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pMultisampleState = &multisample;
	pipelineCreateInfo.pDepthStencilState = &depthStencil;
	pipelineCreateInfo.pRasterizationState = &rasterization;
	pipelineCreateInfo.pColorBlendState = &colorBlend;
	pipelineCreateInfo.pDynamicState = &dynamic;
	pipelineCreateInfo.pTessellationState = NULL;

	memset( shaderStages, 0, sizeof( shaderStages ) );

	// set default states
	reset( true );
}

CPipelineBuilder::~CPipelineBuilder() {
	reset( false );
}

void CPipelineBuilder::reset( bool setDefaults ) {
	int i;

	// destroy shader modules
	for( i = 0; i < shaderStageCount; ++i ) {
		if( shaderStages[i].module ) {
			vkDestroyShaderModule( vkState.device, shaderStages[i].module, NULL );
			shaderStages[i].module = VK_NULL_HANDLE;
		}
	}

	shaderStageCount = 0;
	dynamicStateCount = 0;
	vertexAttributeCount = 0;
	vertexBindingCount = 0;

	if( setDefaults ) {
		// vertex input
		vertexInput.pNext = NULL;
		vertexInput.flags = 0;

		// input assembly
		inputAssembly.pNext = NULL;
		inputAssembly.flags = 0;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// multisampling
		multisample.pNext = NULL;
		multisample.flags = 0;
		multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisample.sampleShadingEnable = VK_FALSE;
		multisample.minSampleShading = 0.f;
		multisample.pSampleMask = NULL;
		multisample.alphaToCoverageEnable = VK_FALSE;
		multisample.alphaToOneEnable = VK_FALSE;

		// depth-stencil
		depthStencil.pNext = NULL;
		depthStencil.flags = 0;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front.failOp = depthStencil.back.failOp = VK_STENCIL_OP_KEEP;
		depthStencil.front.passOp = depthStencil.back.passOp = VK_STENCIL_OP_KEEP;
		depthStencil.front.depthFailOp = depthStencil.back.depthFailOp = VK_STENCIL_OP_KEEP;
		depthStencil.front.compareOp = depthStencil.back.compareOp = VK_COMPARE_OP_ALWAYS;
		depthStencil.front.compareMask = depthStencil.back.compareMask = 0xff;
		depthStencil.front.writeMask = depthStencil.back.writeMask = 0xff;
		depthStencil.front.reference = depthStencil.back.reference = 0;

		// rasterization
		rasterization.pNext = NULL;
		rasterization.flags = 0;
		rasterization.depthClampEnable = VK_FALSE;
		rasterization.rasterizerDiscardEnable = VK_FALSE;
		rasterization.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterization.depthBiasEnable = VK_FALSE;
		rasterization.depthBiasConstantFactor = 0.f;
		rasterization.depthBiasClamp = 0.f;
		rasterization.depthBiasSlopeFactor = 0.f;
		rasterization.lineWidth = 1.f;

		// color blend
		colorBlend.pNext = NULL;
		colorBlend.flags = 0;
		colorBlend.logicOpEnable = VK_FALSE;
		colorBlend.logicOp = VK_LOGIC_OP_CLEAR;
		colorBlend.attachmentCount = 0;
		colorBlend.pAttachments = NULL;
		colorBlend.blendConstants[0] = 1.f;
		colorBlend.blendConstants[1] = 1.f;
		colorBlend.blendConstants[2] = 1.f;
		colorBlend.blendConstants[3] = 1.f;

		// dynamic states
		dynamic.pNext = NULL;
		dynamic.flags = 0;
		// dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
		// dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;

		// pipeline create info
		pipelineCreateInfo.pNext = NULL;
		pipelineCreateInfo.flags = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = 0;
		pipelineCreateInfo.subpass = 0;
	}
}

void CPipelineBuilder::setShader( VkShaderStageFlagBits stage, const uint32_t *code, int codeSize ) {
	VkPipelineShaderStageCreateInfo *stageCreateInfo = NULL;

	for( int i = 0; i < shaderStageCount; ++i ) {
		if( shaderStages[i].stage == stage ) {
			stageCreateInfo = &shaderStages[i];
			break;
		}
	}

	if( !stageCreateInfo ) {
		if( shaderStageCount == TR_MAX_SHADER_STAGE_COUNT ) {
			Com_Error( ERR_FATAL, "CPipelineBuilder: max shader stage count limit reached\n" );
		}
		stageCreateInfo = &shaderStages[shaderStageCount++];
	}

	if( stageCreateInfo->module ) {
		vkDestroyShaderModule( vkState.device, stageCreateInfo->module, NULL );
	}

	stageCreateInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageCreateInfo->pNext = NULL;
	stageCreateInfo->flags = 0;
	stageCreateInfo->stage = stage;
	stageCreateInfo->module = SPV_CreateShaderModule( code, codeSize );
	stageCreateInfo->pSpecializationInfo = NULL;

	switch( stage ) {
	case VK_SHADER_STAGE_VERTEX_BIT:
		stageCreateInfo->pName = "VS_Main";
		break;

	case VK_SHADER_STAGE_FRAGMENT_BIT:
		stageCreateInfo->pName = "PS_Main";
		break;

	default:
		Com_Error( ERR_FATAL, "CPipelineBuilder: unsupported shader stage\n" );
	}
}

void CPipelineBuilder::setDynamicState( VkDynamicState state ) {
	for( int i = 0; i < dynamicStateCount; ++i ) {
		if( dynamicStates[i] == state ) {
			return;
		}
	}
	if( dynamicStateCount == TR_MAX_DYNAMIC_STATE_COUNT ) {
		Com_Error( ERR_FATAL, "CPipelineBuilder: max dynamic state count limit reached\n" );
	}
	dynamicStates[dynamicStateCount++] = state;
}

void CPipelineBuilder::addVertexAttribute( VkFormat format, int offset, int binding ) {
	VkVertexInputAttributeDescription *attr;

	if( vertexAttributeCount == TR_MAX_VERTEX_INPUT_ATTRIBUTE_COUNT ) {
		Com_Error( ERR_FATAL, "CPipelineBuilder: max vertex attribute count limit reached\n" );
	}

	attr = &vertexAttributes[vertexAttributeCount];
	attr->binding = binding;
	attr->format = format;
	attr->location = vertexAttributeCount;
	attr->offset = offset;

	vertexAttributeCount++;
}

void CPipelineBuilder::addVertexBinding( int stride, VkVertexInputRate rate ) {
	VkVertexInputBindingDescription *binding;

	if( vertexBindingCount == TR_MAX_VERTEX_INPUT_BINDING_COUNT ) {
		Com_Error( ERR_FATAL, "CPipelineBuilder: max vertex binding count limit reached\n" );
	}

	binding = &vertexBindings[vertexBindingCount];
	binding->binding = vertexBindingCount;
	binding->stride = stride;
	binding->inputRate = rate;

	vertexBindingCount++;
}

void CPipelineBuilder::build( VkPipeline *pipeline ) {
	VkResult res;
	int i;

	// update number of defined shader stages
	pipelineCreateInfo.stageCount = shaderStageCount;

	// update number enabled dynamic states
	dynamic.dynamicStateCount = dynamicStateCount;

	// update number of vertex input attributes and bindings
	vertexInput.vertexAttributeDescriptionCount = vertexAttributeCount;
	vertexInput.vertexBindingDescriptionCount = vertexBindingCount;

	// setup the default viewport state
	// it won't be used because the dynamic viewport and scissor rects are always enabled,
	// but the spec requires the default state to be set anyway
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

	VkViewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = glConfig.vidWidth;
	viewport.height = glConfig.vidHeight;
	viewport.minDepth = 0;
	viewport.maxDepth = 1;

	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = glConfig.vidWidth;
	scissor.extent.height = glConfig.vidHeight;

	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	pipelineCreateInfo.pViewportState = &viewportState;

	// setup the specialization constants
	VkSpecializationInfo specializationInfo;
	VkSpecializationMapEntry specializationMapEntry;
	if( shaderSpec != 0 ) {
		specializationMapEntry.constantID = 0;
		specializationMapEntry.offset = 0;
		specializationMapEntry.size = sizeof( shaderSpec );

		specializationInfo.mapEntryCount = 1;
		specializationInfo.pMapEntries = &specializationMapEntry;
		specializationInfo.dataSize = sizeof( shaderSpec );
		specializationInfo.pData = &shaderSpec;

		for( i = 0; i < shaderStageCount; ++i ) {
			shaderStages[i].pSpecializationInfo = &specializationInfo;
		}
	}
	else {
		for( i = 0; i < shaderStageCount; ++i ) {
			shaderStages[i].pSpecializationInfo = NULL;
		}
	}

	// create the pipeline
	res = vkCreateGraphicsPipelines( vkState.device, vkState.pipelineCache, 1, &pipelineCreateInfo, NULL, pipeline );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "CPipelineBuilder: failed to create graphics pipeline (%d)\n", res );
	}
}

CFrameBufferBuilder::CFrameBufferBuilder() {
	reset();
}

void CFrameBufferBuilder::reset() {
	width = 0;
	height = 0;
	attachmentCount = 0;
	depthBufferIndex = -1;
}

void CFrameBufferBuilder::addColorAttachment( VkFormat format, bool clear, const VkClearColorValue &clearValue ) {
	VkAttachmentDescription *att = attachmentDescriptions + attachmentCount;

	if( attachmentCount == TR_MAX_FRAMEBUFFER_IMAGES ) {
		Com_Error( ERR_FATAL, "CFrameBufferBuilder: max frame buffer image count limit reached\n" );
	}

	att->flags = 0;
	att->format = format;
	att->samples = VK_SAMPLE_COUNT_1_BIT;
	att->loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	att->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	att->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	att->initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	att->finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	externalImages[attachmentCount] = NULL;
	clearValues[attachmentCount].color = clearValue;

	attachmentCount++;
}

void CFrameBufferBuilder::addColorAttachment( image_t *image, bool clear, const VkClearColorValue &clearValue ) {
	VkAttachmentDescription *att = attachmentDescriptions + attachmentCount;

	if( attachmentCount == TR_MAX_FRAMEBUFFER_IMAGES ) {
		Com_Error( ERR_FATAL, "CFrameBufferBuilder: max frame buffer image count limit reached\n" );
	}
	att->flags = 0;
	att->format = image->internalFormat;
	att->samples = VK_SAMPLE_COUNT_1_BIT;
	att->loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	att->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	att->stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	att->initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	att->finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	externalImages[attachmentCount] = image;
	clearValues[attachmentCount].color = clearValue;

	attachmentCount++;
}

void CFrameBufferBuilder::addDepthStencilAttachment( VkFormat format, bool clear, const VkClearDepthStencilValue &clearValue ) {
	VkAttachmentDescription *att = attachmentDescriptions + attachmentCount;

	if( attachmentCount == TR_MAX_FRAMEBUFFER_IMAGES ) {
		Com_Error( ERR_FATAL, "CFrameBufferBuilder: max frame buffer image count limit reached\n" );
	}
	if( depthBufferIndex != -1 ) {
		Com_Error( ERR_FATAL, "CFrameBufferBuilder: frame buffer already contains a depth-stencil buffer at index %d\n", depthBufferIndex );
	}

	att->flags = 0;
	att->format = format;
	att->samples = VK_SAMPLE_COUNT_1_BIT;
	att->loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	att->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att->stencilLoadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	att->stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	att->initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	att->finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	externalImages[attachmentCount] = NULL;
	clearValues[attachmentCount].depthStencil = clearValue;
	depthBufferIndex = attachmentCount;

	attachmentCount++;
}

void CFrameBufferBuilder::addDepthStencilAttachment( image_t *image, bool clear, const VkClearDepthStencilValue &clearValue ) {
	VkAttachmentDescription *att = attachmentDescriptions + attachmentCount;

	if( attachmentCount == TR_MAX_FRAMEBUFFER_IMAGES ) {
		Com_Error( ERR_FATAL, "CFrameBufferBuilder: max frame buffer image count limit reached\n" );
	}
	if( depthBufferIndex != -1 ) {
		Com_Error( ERR_FATAL, "CFrameBufferBuilder: frame buffer already contains a depth-stencil buffer at index %d\n", depthBufferIndex );
	}

	att->flags = 0;
	att->format = image->internalFormat;
	att->samples = VK_SAMPLE_COUNT_1_BIT;
	att->loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	att->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att->stencilLoadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	att->stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	att->initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	att->finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	externalImages[attachmentCount] = image;
	clearValues[attachmentCount].depthStencil = clearValue;
	depthBufferIndex = attachmentCount;

	attachmentCount++;
}

void CFrameBufferBuilder::build( frameBuffer_t **frameBuffer ) {
	VkSubpassDescription subpassDescription = {};
	VkAttachmentReference attachmentReferences[TR_MAX_FRAMEBUFFER_IMAGES];
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	VkFramebufferCreateInfo framebufferCreateInfo = {};
	VkImageView framebufferAttachments[TR_MAX_FRAMEBUFFER_IMAGES];
	VkResult res;

	// allocate the frame buffer
	*frameBuffer = (frameBuffer_t *)R_Malloc( sizeof( frameBuffer_t ), TAG_HUNKALLOC );
	frameBuffer_t *fb = *frameBuffer;

	// find all color attachments
	int colorAttachmentCount = 0;
	for( int i = 0; i < attachmentCount; ++i ) {
		if( i == depthBufferIndex )
			continue;
		attachmentReferences[colorAttachmentCount].attachment = i;
		attachmentReferences[colorAttachmentCount].layout = attachmentDescriptions[i].initialLayout;
		++colorAttachmentCount;
	}

	subpassDescription.colorAttachmentCount = colorAttachmentCount;
	subpassDescription.pColorAttachments = attachmentReferences;

	if( depthBufferIndex != -1 ) {
		attachmentReferences[colorAttachmentCount].attachment = depthBufferIndex;
		attachmentReferences[colorAttachmentCount].layout = attachmentDescriptions[depthBufferIndex].initialLayout;
		subpassDescription.pDepthStencilAttachment = &attachmentReferences[colorAttachmentCount];
	}

	// create the render pass
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = attachmentCount;
	renderPassCreateInfo.pAttachments = attachmentDescriptions;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDescription;

	res = vkCreateRenderPass( vkState.device, &renderPassCreateInfo, NULL, &fb->renderPass );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "CFrameBufferBuilder: failed to create VkRenderPass (%d)\n", res );
	}

	// create the frame buffer images
	fb->width = width;
	fb->height = height;
	fb->numImages = attachmentCount;

	for( int i = 0; i < attachmentCount; ++i ) {
		if( externalImages[i] ) {
			fb->images[i].external = true;
			fb->images[i].i = externalImages[i];
		}
		else {
			fb->images[i].external = false;
			fb->images[i].i = R_CreateTransientImage( "*frameBufferImage", width, height, attachmentDescriptions[i].format, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
		}
		fb->images[i].layout = attachmentDescriptions[i].initialLayout;
		framebufferAttachments[i] = fb->images[i].i->texview;
	}

	fb->depthBufferIndex = depthBufferIndex;

	memcpy( fb->clearValues, clearValues, sizeof( clearValues ) );

	// create the VkFramebuffer object
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.renderPass = fb->renderPass;
	framebufferCreateInfo.width = width;
	framebufferCreateInfo.height = height;
	framebufferCreateInfo.layers = 1;
	framebufferCreateInfo.attachmentCount = attachmentCount;
	framebufferCreateInfo.pAttachments = framebufferAttachments;

	res = vkCreateFramebuffer( vkState.device, &framebufferCreateInfo, NULL, &fb->buf );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "CFrameBufferBuilder: failed to create VkFramebuffer (%d)\n", res );
	}
}

CDescriptorSetWriter::CDescriptorSetWriter( VkDescriptorSet dstSet ) {
	reset( dstSet );
}

void CDescriptorSetWriter::reset( VkDescriptorSet dstSet ) {
	if( dstSet != VK_NULL_HANDLE ) {
		descriptorSet = dstSet;
	}
	writeCount = 0;
	bufferCount = 0;
	imageCount = 0;
}

void CDescriptorSetWriter::writeBuffer( int binding, VkDescriptorType type, buffer_t *buffer, VkDeviceSize offset, VkDeviceSize range ) {
	if( writeCount == TR_MAX_DESCRIPTOR_SET_UPDATE_SIZE ) {
		flush();
	}

	VkWriteDescriptorSet *write = &writes[writeCount++];
	VkDescriptorBufferInfo *bdesc = &buffers[bufferCount++];

	bdesc->buffer = buffer->buf;
	bdesc->offset = offset;
	bdesc->range = range;

	write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write->pNext = NULL;
	write->dstSet = descriptorSet;
	write->dstBinding = binding;
	write->dstArrayElement = 0;
	write->descriptorCount = 1;
	write->descriptorType = type;
	write->pImageInfo = NULL;
	write->pBufferInfo = bdesc;
	write->pTexelBufferView = NULL;
}

void CDescriptorSetWriter::writeImage( int binding, VkDescriptorType type, image_t *image, VkImageLayout layout ) {
	if( writeCount == TR_MAX_DESCRIPTOR_SET_UPDATE_SIZE ) {
		flush();
	}

	VkWriteDescriptorSet *write = &writes[writeCount++];
	VkDescriptorImageInfo *idesc = &images[imageCount++];

	idesc->imageView = image->texview;
	idesc->imageLayout = layout;
	idesc->sampler = VK_NULL_HANDLE;

	write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write->pNext = NULL;
	write->dstSet = descriptorSet;
	write->dstBinding = binding;
	write->dstArrayElement = 0;
	write->descriptorCount = 1;
	write->descriptorType = type;
	write->pImageInfo = idesc;
	write->pBufferInfo = NULL;
	write->pTexelBufferView = NULL;
}

void CDescriptorSetWriter::writeSampler( int binding, VkSampler sampler ) {
	if( writeCount == TR_MAX_DESCRIPTOR_SET_UPDATE_SIZE ) {
		flush();
	}

	VkWriteDescriptorSet *write = &writes[writeCount++];
	VkDescriptorImageInfo *idesc = &images[imageCount++];

	idesc->imageView = VK_NULL_HANDLE;
	idesc->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	idesc->sampler = sampler;

	write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write->pNext = NULL;
	write->dstSet = descriptorSet;
	write->dstBinding = binding;
	write->dstArrayElement = 0;
	write->descriptorCount = 1;
	write->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	write->pImageInfo = idesc;
	write->pBufferInfo = NULL;
	write->pTexelBufferView = NULL;
}

void CDescriptorSetWriter::flush() {
	vkUpdateDescriptorSets( vkState.device, writeCount, writes, 0, NULL );
	reset();
}

void CDynamicGeometryBuilder::init() {
	root = (vertexBufferList_t *)R_Malloc( sizeof( vertexBufferList_t ), TAG_HUNKALLOC );
	root->vertexBuffer = R_CreateVertexBuffer( ARRAY_LEN( vertexes ), ARRAY_LEN( indexes ) );
	root->next = NULL;
	reset();
}

void CDynamicGeometryBuilder::reset() {
	curr = root;
	triangleStrip = false;
	vertexOffset = 0;
	vertexCount = 0;
	indexOffset = 0;
	indexCount = 0;
	drawStateBits = 0;
}

void CDynamicGeometryBuilder::checkOverflow( int numVertexes, int numIndexes ) {
	if( ( vertexOffset + vertexCount + numVertexes > ARRAY_LEN( vertexes ) ) ||
		( indexOffset + indexCount + numIndexes > ARRAY_LEN( indexes ) ) ) {
		endGeometry();
		// move to the next vertex buffer
		if( !curr->next ) {
			curr->next = (vertexBufferList_t *)R_Malloc( sizeof( vertexBufferList_t ), TAG_HUNKALLOC );
			curr->next->vertexBuffer = R_CreateVertexBuffer( ARRAY_LEN( vertexes ), ARRAY_LEN( indexes ) );
			curr->next->next = NULL;
		}
		curr = curr->next;
		vertexOffset = 0;
		indexOffset = 0;
	}
}

void CDynamicGeometryBuilder::beginGeometry() {
	vertexCount = 0;
	indexCount = 0;
}

void CDynamicGeometryBuilder::beginTriangleStrip() {
	triangleStripVertexCount = 0;
	triangleStrip = true;
}

void CDynamicGeometryBuilder::endTriangleStrip() {
	triangleStrip = false;
}

int CDynamicGeometryBuilder::addVertex() {
	int vertex = vertexCount++;
	if( triangleStrip ) {
		if( triangleStripVertexCount == 2 ) {
			// next vertex will form a triangle
			addTriangle( triangleStripVertexes[0], triangleStripVertexes[1], vertex );
			triangleStripVertexCount = 0;
		}
		triangleStripVertexes[triangleStripVertexCount] = vertex;
		triangleStripVertexCount++;
	}
	return vertexCount++;
}

void CDynamicGeometryBuilder::addTriangle( int a, int b, int c ) {
	indexes[indexCount++] = a;
	indexes[indexCount++] = b;
	indexes[indexCount++] = c;
}

void CDynamicGeometryBuilder::endGeometry() {
	drawCommand_t *draw;

	if( indexCount > 0 ) {
		int idxoff = indexOffset * sizeof( *indexes );
		int vertoff = vertexOffset * sizeof( *vertexes ) + curr->vertexBuffer->vertexOffset;

		// update the vertex buffer
		VK_UploadBuffer( &curr->vertexBuffer->b, (const byte *)indexes, indexCount * sizeof( *indexes ), idxoff );
		VK_UploadBuffer( &curr->vertexBuffer->b, (const byte *)vertexes, vertexCount * sizeof( *vertexes ), vertoff );

		// send the draw command
		RB_CHECKOVERFLOW();
		assert( tess.shader );

		draw = RB_DrawSurface();

		draw->numVertexBuffers = 1;
		draw->vertexBuffers[0] = curr->vertexBuffer;

		draw->vertexOffsets[0] = vertoff;
		draw->vertexCount = vertexCount;

		draw->indexOffset = idxoff;
		draw->indexCount = indexCount;

		draw->stateBits = drawStateBits;

		vertexOffset += vertexCount;
		indexOffset += indexCount;

		// begin new geometry
		beginGeometry();
	}
}

void CDynamicGeometryBuilder::setDrawStateBits( int stateBits ) {
	if( drawStateBits != stateBits ) {
		endGeometry();
	}
	drawStateBits = stateBits;
}
