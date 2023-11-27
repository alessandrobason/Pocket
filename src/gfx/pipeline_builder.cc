#include "pipeline_builder.h"

#include <vulkan/vulkan.h>

#include "std/logging.h"

#include "shader.h"

PipelineBuilder PipelineBuilder::begin() {
	return PipelineBuilder();
}

PipelineBuilder &PipelineBuilder::pushShader(
	VkShaderStageFlagBits stage, 
	VkShaderModule shader, 
	const char *entry
) {
	m_shader_stages.push(VkPipelineShaderStageCreateInfo{
		.stage = stage,
		.module = shader,
		.pName = entry,
	});
	return *this;
}

PipelineBuilder &PipelineBuilder::pushShaders(ShaderCompiler &compiler) {
	m_layout = compiler.pipeline_layout;

	for (ShaderCompiler::StageInfo &stage : compiler.stages) {
		m_shader_stages.push({
			.stage = stage.stage,
			.module = stage.module,
			.pName = "main",
		});
	}
	
	return *this;
}

PipelineBuilder &PipelineBuilder::setVertexInput(const VertexInDesc &vtx_desc) {
	return setVertexInput(
		vtx_desc.bindings.data(), 
		(u32)vtx_desc.bindings.size(), 
		vtx_desc.attributes.data(), 
		(u32)vtx_desc.attributes.size()
	);
}

PipelineBuilder &PipelineBuilder::setVertexInput(
	const VkVertexInputBindingDescription *bindings, 
	u32 bcount, 
	const VkVertexInputAttributeDescription *attributes, 
	u32 acount
) {
	m_vtx_input = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = bcount,
		.pVertexBindingDescriptions = bindings,
		.vertexAttributeDescriptionCount = acount,
		.pVertexAttributeDescriptions = attributes,
	};
	return *this;
}

PipelineBuilder &PipelineBuilder::setInputAssembly(VkPrimitiveTopology topology) {
	m_input_assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = topology,
	};
	return *this;
}

PipelineBuilder &PipelineBuilder::setViewport(
	float x, 
	float y, 
	float w, 
	float h, 
	float min_depth, 
	float max_depth
) {
	m_viewport = {
		.x = 0,
		.y = 0,
		.width = w,
		.height = h,
		.minDepth = min_depth,
		.maxDepth = max_depth,
	};
	return *this;
}

PipelineBuilder &PipelineBuilder::setScissor(VkExtent2D extent, VkOffset2D offset) {
	m_scissor = {
		.offset = offset,
		.extent = extent,
	};
	return *this;
}

PipelineBuilder &PipelineBuilder::setRasterizer(
	VkCullModeFlags cull_mode, 
	VkFrontFace front_face, 
	VkPolygonMode polygon_mode
) {
	m_rasterizer = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = false,
		// discards all primitives before the rasterization stage if enabled which we don't want
		.rasterizerDiscardEnable = false,
		.polygonMode = polygon_mode,
		.cullMode = cull_mode,
		.frontFace = front_face,
		.depthBiasClamp = false,
		.lineWidth = 1.f,
	};
	return *this;
}

PipelineBuilder &PipelineBuilder::setColourBlend(VkColorComponentFlags flags) {
	m_colour_blend = {
		.blendEnable = false,
		.colorWriteMask = flags,
	};
	return *this;
}

PipelineBuilder &PipelineBuilder::setMultisampling(VkSampleCountFlagBits samples) {
	m_multisampling = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = samples,
		.minSampleShading = 1.f,
	};
	return *this;
}

PipelineBuilder &PipelineBuilder::setLayout(VkPipelineLayout layout) {
	m_layout = layout;
	return *this;
}

PipelineBuilder &PipelineBuilder::setDepthStencil(VkCompareOp operation) {
	m_depth_stencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = true,
		.depthWriteEnable = true,
		.depthCompareOp = operation,
		.depthBoundsTestEnable = false,
		.minDepthBounds = 0.f,
		.maxDepthBounds = 1.f,
	};
	return *this;
}

PipelineBuilder &PipelineBuilder::setDynamicState(Slice<VkDynamicState> dynamic_states) {
	m_dyn_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = (u32)dynamic_states.len,
		.pDynamicStates = dynamic_states.buf,
	};
	return *this;
}

vkptr<VkPipeline> PipelineBuilder::build(VkDevice device, VkRenderPass pass) {
	for (auto &stage : m_shader_stages) {
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	}

	Slice<VkDynamicState> dyn_state = { m_dyn_create_info.pDynamicStates, m_dyn_create_info.dynamicStateCount };
	bool dynamic_viewport = dyn_state.contains(VK_DYNAMIC_STATE_VIEWPORT);
	bool dynamic_scissor = dyn_state.contains(VK_DYNAMIC_STATE_SCISSOR);

	// make viewport state from our stored viewport and scissor.
	// at the moment we won't support multiple viewports or scissors
	VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = dynamic_viewport ? nullptr : &m_viewport,
		.scissorCount = 1,
		.pScissors = dynamic_scissor ? nullptr : &m_scissor,
	};

	// setup dummy color blending. We aren't using transparent objects yet
	// the blending is just "no blend", but we do write to the color attachment
	VkPipelineColorBlendStateCreateInfo colour_blending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &m_colour_blend,
	};

	VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = (u32)m_shader_stages.size(),
		.pStages = m_shader_stages.data(),
		.pVertexInputState = &m_vtx_input,
		.pInputAssemblyState = &m_input_assembly,
		.pViewportState = &viewport_state,
		.pRasterizationState = &m_rasterizer,
		.pMultisampleState = &m_multisampling,
		.pDepthStencilState = &m_depth_stencil,
		.pColorBlendState = &colour_blending,
		.pDynamicState = &m_dyn_create_info,
		.layout = m_layout,
		.renderPass = pass,
	};

	VkPipeline pipeline;
	if (vkCreateGraphicsPipelines(
			device,
			nullptr,
			1, &pipeline_info,
			nullptr,
			&pipeline
		)
	) {
		err("failed to create pipeline");
		return nullptr;
	}
	
	return pipeline;
}
