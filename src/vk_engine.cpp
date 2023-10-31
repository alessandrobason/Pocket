
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

#include "VkBootstrap.h"
#include "utils.h"

#define VK_CHECK(x) \
	do { \
		VkResult err = x; \
		if (err) { \
			fatal("Vulkan error: %d", err); \
		} \
	} while (0)

static uint32_t vulkan_print_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
	void *pUserData
);

void VulkanEngine::init() {
	debug("Initializing");

	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);
	
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
	
	m_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		m_windowExtent.width,
		m_windowExtent.height,
		window_flags
	);

	initVulkan();
	initSwapChain();
	initCommands();
	initDefaultRenderPass();
	initFramebuffers();
	initSyncStructures();
	initPipeline();
	
	//everything went fine
	m_isInitialized = true;
}
void VulkanEngine::cleanup()
{	
	debug("Cleaning up");

	VK_CHECK(vkWaitForFences(m_device, 1, &m_render_fence, true, 1000000000));
	VK_CHECK(vkResetCommandBuffer(m_main_cmdbuf, 0));

	if (m_isInitialized) {
		vkDestroySemaphore(m_device, m_render_sem, nullptr);
		vkDestroySemaphore(m_device, m_present_sem, nullptr);
		vkDestroyFence(m_device, m_render_fence, nullptr);

		vkDestroyCommandPool(m_device, m_cmdpool, nullptr);

		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

		for (size_t i = 0; i < m_framebuffers.size(); ++i) {
			vkDestroyFramebuffer(m_device, m_framebuffers[i], nullptr);

			vkDestroyImageView(m_device, m_swapchain_img_views[i], nullptr);
		}

		vkDestroyDevice(m_device, nullptr);
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		vkb::destroy_debug_utils_messenger(m_instance, m_debug_messenger);
		vkDestroyInstance(m_instance, nullptr);
		
		SDL_DestroyWindow(m_window);
	}
}

void VulkanEngine::draw() {
	VK_CHECK(vkWaitForFences(m_device, 1, &m_render_fence, true, 1000000000));
	VK_CHECK(vkResetFences(m_device, 1, &m_render_fence));

	uint32_t sc_img_index = 0;
	VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain, 1000000000, m_present_sem, nullptr, &sc_img_index));

	VK_CHECK(vkResetCommandBuffer(m_main_cmdbuf, 0));

	VkCommandBuffer cmd = m_main_cmdbuf;

	VkCommandBufferBeginInfo beg_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	VK_CHECK(vkBeginCommandBuffer(cmd, &beg_info));

	float flash = abs(sinf(m_frameNumber / 120.f));
	VkClearValue clear_col = {
		.color = { { 0.f, 0.f, flash, 1.f } }
	};

	VkRenderPassBeginInfo rp_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = m_render_pass,
		.framebuffer = m_framebuffers[sc_img_index],
		.renderArea = {
			.extent = m_windowExtent,
		},
		.clearValueCount = 1,
		.pClearValues = &clear_col
	};

	vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_tri_pipeline);
	vkCmdDraw(cmd, 3, 1, 0, 0);

	vkCmdEndRenderPass(cmd);
	VK_CHECK(vkEndCommandBuffer(cmd));

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_present_sem,
		.pWaitDstStageMask = &wait_stage,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_render_sem,
	};

	VK_CHECK(vkQueueSubmit(m_gfxqueue, 1, &submit, m_render_fence));

	VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_render_sem,
		.swapchainCount = 1,
		.pSwapchains = &m_swapchain,
		.pImageIndices = &sc_img_index,
	};

	VK_CHECK(vkQueuePresentKHR(m_gfxqueue, &present_info));

	++m_frameNumber;
}

void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	//main loop
	while (!bQuit)
	{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) bQuit = true;

		}

		draw();
	}
}

void VulkanEngine::initVulkan() {
	vkb::InstanceBuilder builder;

	auto inst_ret = builder.
		set_app_name("Vulkan Application")
		.request_validation_layers(true)
		.require_api_version(1, 1, 0)
		.set_debug_callback(vulkan_print_callback)
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	m_instance = vkb_inst.instance;
	m_debug_messenger = vkb_inst.debug_messenger;

	SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface);

	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physical_device = selector
		.set_minimum_version(1, 1)
		.set_surface(m_surface)
		.select()
		.value();

	vkb::DeviceBuilder device_builder{ physical_device };
	vkb::Device vkb_device = device_builder.build().value();

	m_device = vkb_device.device;
	m_chosen_gpu = physical_device.physical_device;

	m_gfxqueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
	m_gfxqueue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::initSwapChain() {
	vkb::SwapchainBuilder sc_builder{ m_chosen_gpu, m_device, m_surface };

	vkb::Swapchain vkb_sc = sc_builder
		.use_default_format_selection()
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(m_windowExtent.width, m_windowExtent.height)
		.build()
		.value();

	m_swapchain = vkb_sc.swapchain;
	m_swapchain_images = vkb_sc.get_images().value();
	m_swapchain_img_views = vkb_sc.get_image_views().value();
	m_swapchain_img_format = vkb_sc.image_format;
}

void VulkanEngine::initCommands() {
	VkCommandPoolCreateInfo cmdpool_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		// allow pool to reset individual command buffers
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_gfxqueue_family,
	};

	VK_CHECK(vkCreateCommandPool(m_device, &cmdpool_info, nullptr, &m_cmdpool));

	VkCommandBufferAllocateInfo cmdalloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = m_cmdpool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	VK_CHECK(vkAllocateCommandBuffers(m_device, &cmdalloc_info, &m_main_cmdbuf));
}

void VulkanEngine::initDefaultRenderPass() {
	VkAttachmentDescription colour_attachment = {
		.format = m_swapchain_img_format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};

	VkAttachmentReference colour_attachment_ref = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDescription subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colour_attachment_ref,
	};

	VkRenderPassCreateInfo render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &colour_attachment,
		.subpassCount = 1,
		.pSubpasses = &subpass,
	};

	VK_CHECK(vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass));
}

void VulkanEngine::initFramebuffers() {
	VkFramebufferCreateInfo fb_info = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = m_render_pass,
		.attachmentCount = 1,
		.width = m_windowExtent.width,
		.height = m_windowExtent.height,
		.layers = 1,
	};

	const size_t sc_image_count = m_swapchain_images.size();
	m_framebuffers = std::vector<VkFramebuffer>{ sc_image_count };

	for (size_t i = 0; i < sc_image_count; ++i) {
		fb_info.pAttachments = &m_swapchain_img_views[i];
		VK_CHECK(vkCreateFramebuffer(m_device, &fb_info, nullptr, &m_framebuffers[i]));
	}
}

void VulkanEngine::initSyncStructures() {
	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	VK_CHECK(vkCreateFence(m_device, &fence_info, nullptr, &m_render_fence));

	VkSemaphoreCreateInfo sem_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	VK_CHECK(vkCreateSemaphore(m_device, &sem_info, nullptr, &m_present_sem));
	VK_CHECK(vkCreateSemaphore(m_device, &sem_info, nullptr, &m_render_sem));
}

void VulkanEngine::initPipeline() {
	VkShaderModule frag = loadShaderModule("../../shaders/triangle.frag.spv");
	VkShaderModule vert = loadShaderModule("../../shaders/triangle.vert.spv");

	if (!frag) err("could not compile fragment shader");
	else info("compiled fragment shader");

	if (!vert) err("could not compile vertex shader");
	else info("compiled vertex shader");

	VkPipelineLayoutCreateInfo pipeline_layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	};

	VK_CHECK(vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_tri_pipeline_layout));

	PipelineBuilder pipeline_builder = {
		.m_vtx_input = {
			.vertexBindingDescriptionCount = 0,
			.vertexAttributeDescriptionCount = 0
		},
		.m_input_assembly = {
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		},
		.m_viewport = {
			.x = 0,
			.y = 0,
			.width = (float)m_windowExtent.width,
			.height = (float)m_windowExtent.height,
			.minDepth = 0.f,
			.maxDepth = 1.f,
		},
		.m_scissor = {
			.offset = { 0, 0 },
			.extent = m_windowExtent,
		},
		.m_rasterizer = {
			.depthClampEnable = false,
			// discards all primitives before the rasterization stage if enabled which we don't want
			.rasterizerDiscardEnable = false,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasClamp = false,
			.lineWidth = 1.f,
		},
		.m_colour_blend = {
			.blendEnable = false,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		},
		.m_multisampling = {
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.minSampleShading = 1.f,
		},
		.m_layout = m_tri_pipeline_layout
	};

	pipeline_builder.m_shader_stages.push_back(VkPipelineShaderStageCreateInfo{
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vert,
		.pName = "main",
	});

	pipeline_builder.m_shader_stages.push_back(VkPipelineShaderStageCreateInfo{
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = frag,
		.pName = "main",
	});

	m_tri_pipeline = pipeline_builder.build(m_device, m_render_pass);
}

VkShaderModule VulkanEngine::loadShaderModule(const char *path) {
	FILE *fp = nullptr;
	errno_t error = fopen_s(&fp, path, "rb");
	if (error) {
		err("couldn't open file %s: %d", path, error);
		return nullptr;
	}

	fseek(fp, 0, SEEK_END);
	size_t file_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	uint8_t *buf = (uint8_t *)calloc(file_len, 1);
	assert(buf);

	fread(buf, 1, file_len, fp);
	fclose(fp);

	VkShaderModuleCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = file_len,
		.pCode = (uint32_t *)buf,
	};

	VkShaderModule module;
	if (vkCreateShaderModule(m_device, &info, nullptr, &module)) {
		module = nullptr;
	}

	free(buf);
	return module;
}

////////////////////////////////////////////////////////////

static uint32_t vulkan_print_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void *pUserData
) {
	LogLevel level = LogLevel::None;
	switch (messageSeverity) {
		case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			level = LogLevel::Debug;
			break;
		case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			level = LogLevel::Info;
			break;
		case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			level = LogLevel::Warn;
			break;
		case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			level = LogLevel::Error;
			break;
	}

	const char *type = "Unknown";
	switch (messageTypes) {
		case 1: type = "General"; break;
		case 2: type = "Validation"; break;
		case 3: type = "General | Validation"; break;
		case 4: type = "Performance"; break;
		case 5: type = "General | Performance"; break;
		case 6: type = "Validation | Performance"; break;
		case 7: type = "General | Validation | Performance"; break;
	}

	logMessage(level, "(Vulkan / %s): %s", type, pCallbackData->pMessage);

	return VK_FALSE;
}

VkPipeline PipelineBuilder::build(VkDevice device, VkRenderPass pass) {
	for (auto &stage : m_shader_stages) {
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	}

	m_vtx_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	m_input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	m_rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	
	//make viewport state from our stored viewport and scissor.
	//at the moment we won't support multiple viewports or scissors
	VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &m_viewport,
		.scissorCount = 1,
		.pScissors = &m_scissor,
	};

	//setup dummy color blending. We aren't using transparent objects yet
	//the blending is just "no blend", but we do write to the color attachment
	VkPipelineColorBlendStateCreateInfo colour_blending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &m_colour_blend,
	};

	VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = (uint32_t)m_shader_stages.size(),
		.pStages = m_shader_stages.data(),
		.pVertexInputState = &m_vtx_input,
		.pInputAssemblyState = &m_input_assembly,
		.pViewportState = &viewport_state,
		.pRasterizationState = &m_rasterizer,
		.pMultisampleState = &m_multisampling,
		.pColorBlendState = &colour_blending,
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
