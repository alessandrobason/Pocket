#pragma once

#include <vulkan/vulkan.h>

#include "std/arr.h"
#include "std/slice.h"
#include "mesh.h"

struct PipelineBuilder {
	arr<VkPipelineShaderStageCreateInfo> m_shader_stages;
	VkPipelineVertexInputStateCreateInfo m_vtx_input;
	VkPipelineInputAssemblyStateCreateInfo m_input_assembly;
	VkViewport m_viewport;
	VkRect2D m_scissor;
	VkPipelineRasterizationStateCreateInfo m_rasterizer;
	VkPipelineColorBlendAttachmentState m_colour_blend;
	VkPipelineMultisampleStateCreateInfo m_multisampling;
	VkPipelineLayout m_layout;
	VkPipelineDepthStencilStateCreateInfo m_depth_stencil;
	VkPipelineDynamicStateCreateInfo m_dyn_create_info;

	PipelineBuilder &pushShader(VkShaderStageFlagBits stage, VkShaderModule shader, const char *entry = "main");
	PipelineBuilder &setVertexInput(const VertexInDesc &vtx_desc);
	PipelineBuilder &setVertexInput(const VkVertexInputBindingDescription *bindings, u32 bcount, const VkVertexInputAttributeDescription *attributes, u32 acount);
	PipelineBuilder &setInputAssembly(VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	PipelineBuilder &setViewport(float x, float y, float w, float h, float min_depth = 0, float max_depth = 1);
	PipelineBuilder &setScissor(VkExtent2D extent, VkOffset2D offset = { 0, 0 });
	PipelineBuilder &setRasterizer(VkCullModeFlags cull_mode, VkFrontFace front_face = VK_FRONT_FACE_CLOCKWISE, VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL);
	PipelineBuilder &setColourBlend(VkColorComponentFlags flags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
	PipelineBuilder &setMultisampling(VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
	PipelineBuilder &setLayout(VkPipelineLayout layout);
	PipelineBuilder &setDepthStencil(VkCompareOp operation = VK_COMPARE_OP_LESS_OR_EQUAL);
	PipelineBuilder &setDynamicState(Slice<VkDynamicState> dynamic_states);
	
	vkptr<VkPipeline> build(VkDevice device, VkRenderPass pass);
};