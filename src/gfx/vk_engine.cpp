
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <glm/gtx/transform.hpp>

#include <vk_types.h>
#include <vk_initializers.h>

#include "VkBootstrap.h"
#include "vk_textures.h"
#include "std/logging.h"

#define VK_CHECK(x) \
	do { \
		VkResult err = x; \
		if (err) { \
			fatal("%s: Vulkan error: %d", #x, err); \
		} \
	} while (0)

static u32 vulkan_print_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void									   *pUserData
);

void VulkanEngine::init() {
	info("Initializing");

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
	initDescriptors();
	initPipeline();
	loadImages();
	loadMeshes();
	initScene();

	m_main_delete_queue.push(vmaDestroyAllocator, m_allocator);
	m_main_delete_queue.push(vkDestroyDevice, m_device, nullptr);
	m_main_delete_queue.push(vkDestroySurfaceKHR, m_instance, m_surface, nullptr);
	m_main_delete_queue.push(vkb::destroy_debug_utils_messenger, m_instance, m_debug_messenger, nullptr);
	m_main_delete_queue.push(vkDestroyInstance, m_instance, nullptr);
	m_main_delete_queue.push(SDL_DestroyWindow, m_window);
	
	//everything went fine
	m_isInitialized = true;
}
void VulkanEngine::cleanup()
{	
	info("Cleaning up");

	
	if (m_isInitialized) {
		for (FrameData &frame : m_frames) {
			VK_CHECK(vkWaitForFences(m_device, 1, &frame.render_fence, true, 1000000000));
		}

		m_main_delete_queue.flush(m_device);
	}
}

void VulkanEngine::draw() {
	FrameData &frame = getFrame();

	VK_CHECK(vkWaitForFences(m_device, 1, &frame.render_fence, true, 1000000000));
	VK_CHECK(vkResetFences(m_device, 1, &frame.render_fence));
	
	VK_CHECK(vkResetCommandBuffer(frame.cmd_buf, 0));

	u32 sc_img_index = 0;
	VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain, 1000000000, frame.present_sem, nullptr, &sc_img_index));

	VkCommandBuffer cmd = frame.cmd_buf;

	VkCommandBufferBeginInfo beg_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	VK_CHECK(vkBeginCommandBuffer(cmd, &beg_info));
	
	VkClearValue clear_col = {
		.color = { { 0.01f, 0.01f, 0.01f, 1.f } }
	};

	VkClearValue clear_depth = {
		.depthStencil{
			.depth = 1,
		},
	};

	VkClearValue clear_values[] = {
		clear_col,
		clear_depth
	};

	VkRenderPassBeginInfo rp_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = m_render_pass,
		.framebuffer = m_framebuffers[sc_img_index],
		.renderArea = {
			.extent = m_windowExtent,
		},
		.clearValueCount = pk_arrlen(clear_values),
		.pClearValues = clear_values
	};

	vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

	drawObjects(cmd, m_drawable.data(), (int)m_drawable.size());

	vkCmdEndRenderPass(cmd);
	VK_CHECK(vkEndCommandBuffer(cmd));

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &frame.present_sem,
		.pWaitDstStageMask = &wait_stage,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &frame.render_sem,
	};

	VK_CHECK(vkQueueSubmit(m_gfxqueue, 1, &submit, frame.render_fence));

	VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &frame.render_sem,
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

	VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_params_feature = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
		.shaderDrawParameters = true,
	};

	vkb::DeviceBuilder device_builder{ physical_device };
	vkb::Device vkb_device = device_builder
		.add_pNext(&shader_draw_params_feature)
		.build()
		.value();

	m_device = vkb_device.device;
	m_chosen_gpu = physical_device.physical_device;
	m_gpu_properties = physical_device.properties;

	m_gfxqueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
	m_gfxqueue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo alloc_info = {
		.physicalDevice = m_chosen_gpu,
		.device = m_device,
		.instance = m_instance,
	};
	vmaCreateAllocator(&alloc_info, &m_allocator);

	info("The GPU has a minimum buffer aligment of %zu", m_gpu_properties.limits.minUniformBufferOffsetAlignment);
}

void VulkanEngine::initSwapChain() {
	vkb::SwapchainBuilder sc_builder{ m_chosen_gpu, m_device, m_surface };

	vkb::Swapchain vkb_sc = sc_builder
		.use_default_format_selection()
		//.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
		.set_desired_extent(m_windowExtent.width, m_windowExtent.height)
		.build()
		.value();

	m_swapchain = vkb_sc.swapchain;
	
	auto images = vkb_sc.get_images().value();
	m_swapchain_images.reserve(images.size());
	for (auto i : images) m_swapchain_images.push(i);
	
	auto views = vkb_sc.get_image_views().value();
	m_swapchain_img_views.reserve(images.size());
	for (auto v : views) m_swapchain_img_views.push(v);

	m_swapchain_img_format = vkb_sc.image_format;

	m_main_delete_queue.push(vkDestroySwapchainKHR, m_device, m_swapchain, nullptr);

	VkExtent3D depth_img_extent = {
		.width = m_windowExtent.width,
		.height = m_windowExtent.height,
		.depth = 1,
	};

	m_depth_format = VK_FORMAT_D32_SFLOAT;

	VkImageCreateInfo depth_img_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = m_depth_format,
		.extent = depth_img_extent,
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
	};

	VmaAllocationCreateInfo depth_img_alloc_info = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
		.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	};

	vmaCreateImage(
		m_allocator,
		&depth_img_info,
		&depth_img_alloc_info,
		&m_depth_img.image,
		&m_depth_img.allocation,
		nullptr
	);

	VkImageViewCreateInfo depth_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = m_depth_img.image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = m_depth_format,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	VK_CHECK(vkCreateImageView(m_device, &depth_view_info, nullptr, &m_depth_view));

	m_main_delete_queue.push(vkDestroyImageView, m_device, m_depth_view, nullptr);
	m_main_delete_queue.push(vmaDestroyImage, m_allocator, m_depth_img.image, m_depth_img.allocation);
}

void VulkanEngine::initCommands() {
	VkCommandPoolCreateInfo cmdpool_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		// allow pool to reset individual command buffers
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_gfxqueue_family,
	};

	VkCommandBufferAllocateInfo cmdalloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	for (FrameData &frame : m_frames) {
		VK_CHECK(vkCreateCommandPool(m_device, &cmdpool_info, nullptr, &frame.cmd_pool));
		m_main_delete_queue.push(vkDestroyCommandPool, m_device, frame.cmd_pool, nullptr);

		cmdalloc_info.commandPool = frame.cmd_pool;
		VK_CHECK(vkAllocateCommandBuffers(m_device, &cmdalloc_info, &frame.cmd_buf));
	}

	VK_CHECK(vkCreateCommandPool(m_device, &cmdpool_info, nullptr, &m_upload_ctx.pool));
	m_main_delete_queue.push(vkDestroyCommandPool, m_device, m_upload_ctx.pool, nullptr);

	cmdalloc_info.commandPool = m_upload_ctx.pool;
	VK_CHECK(vkAllocateCommandBuffers(m_device, &cmdalloc_info, &m_upload_ctx.buffer));
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

	VkAttachmentDescription depth_attachment = {
		.flags = 0,
		.format = m_depth_format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	VkAttachmentReference depth_attachment_ref = {
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	VkSubpassDependency colour_dep = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	VkSubpassDependency depth_dep = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
	};
	
	VkSubpassDescription subpass = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colour_attachment_ref,
		.pDepthStencilAttachment = &depth_attachment_ref,
	};

	VkAttachmentDescription attachments[] = {
		colour_attachment,
		depth_attachment,
	};

	VkSubpassDependency dependencies[] = {
		colour_dep,
		depth_dep,
	};

	VkRenderPassCreateInfo render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = pk_arrlen(attachments), 
		.pAttachments = attachments,
		.subpassCount = 1,
		.pSubpasses = &subpass,
		.dependencyCount = pk_arrlen(dependencies),
		.pDependencies = dependencies,
	};

	VK_CHECK(vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass));
	m_main_delete_queue.push(vkDestroyRenderPass, m_device, m_render_pass, nullptr);
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
	m_framebuffers.resize(sc_image_count);

	for (size_t i = 0; i < sc_image_count; ++i) {
		VkImageView attachments[] = {
			m_swapchain_img_views[i],
			m_depth_view
		};

		fb_info.pAttachments = attachments;
		fb_info.attachmentCount = pk_arrlen(attachments);
		
		//fb_info.pAttachments = &m_swapchain_img_views[i];
		VK_CHECK(vkCreateFramebuffer(m_device, &fb_info, nullptr, &m_framebuffers[i]));
		
		m_main_delete_queue.push(vkDestroyFramebuffer, m_device, m_framebuffers[i], nullptr);
		m_main_delete_queue.push(vkDestroyImageView, m_device, m_swapchain_img_views[i], nullptr);
	}
}

void VulkanEngine::initSyncStructures() {
	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	VkSemaphoreCreateInfo sem_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	for (FrameData &frame : m_frames) {
		VK_CHECK(vkCreateFence(m_device, &fence_info, nullptr, &frame.render_fence));
		m_main_delete_queue.push(vkDestroyFence, m_device, frame.render_fence, nullptr);

		VK_CHECK(vkCreateSemaphore(m_device, &sem_info, nullptr, &frame.present_sem));
		VK_CHECK(vkCreateSemaphore(m_device, &sem_info, nullptr, &frame.render_sem));
		m_main_delete_queue.push(vkDestroySemaphore, m_device, frame.present_sem, nullptr);
		m_main_delete_queue.push(vkDestroySemaphore, m_device, frame.render_sem, nullptr);
	}

	fence_info.flags = 0;
	VK_CHECK(vkCreateFence(m_device, &fence_info, nullptr, &m_upload_ctx.fence));
	m_main_delete_queue.push(vkDestroyFence, m_device, m_upload_ctx.fence, nullptr);
}

void VulkanEngine::initPipeline() {
	VkShaderModule vert = loadShaderModule("../../shaders/mesh.vert.spv");
	VkShaderModule frag = loadShaderModule("../../shaders/triangle.frag.spv");

	if (!vert) err("could not compile vertex shader");
	else info("compiled vertex shader");

	if (!frag) err("could not compile fragment shader");
	else info("compiled fragment shader");

	VkPushConstantRange push_constat = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(MeshPushConstants),
	};

	VkDescriptorSetLayout set_layouts[] = {
		m_global_set_layout,
		m_object_set_layout,
		m_single_texture_set_layout,
	};

	VkPipelineLayoutCreateInfo mesh_pip_layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = pk_arrlen(set_layouts),
		.pSetLayouts = set_layouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push_constat,
	};

	VkPipelineLayout pipeline_layout;

	VK_CHECK(vkCreatePipelineLayout(m_device, &mesh_pip_layout_info, nullptr, &pipeline_layout));
	m_main_delete_queue.push(vkDestroyPipelineLayout, m_device, pipeline_layout, nullptr);

	PipelineBuilder pipeline_builder;
	
	VkPipeline mesh_pip = pipeline_builder
		.setVertexInput(Vertex::getVertexDesc())
		.setInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.setViewport(0, 0, (float)m_windowExtent.width, (float)m_windowExtent.height, 0, 1)
		.setScissor(m_windowExtent)
		.setRasterizer(VK_CULL_MODE_NONE)
		.setColourBlend()
		.setMultisampling(VK_SAMPLE_COUNT_1_BIT)
		.setLayout(pipeline_layout)
		.setDepthStencil(VK_COMPARE_OP_LESS_OR_EQUAL)
		.pushShader(VK_SHADER_STAGE_VERTEX_BIT, vert)
		.pushShader(VK_SHADER_STAGE_FRAGMENT_BIT, frag)
		.build(m_device, m_render_pass);

	makeMaterial(mesh_pip, pipeline_layout, "default");
	m_main_delete_queue.push(vkDestroyPipeline, m_device, mesh_pip, nullptr);

	makeMaterial(mesh_pip, pipeline_layout, "texturedmesh");

	vkDestroyShaderModule(m_device, frag, nullptr);
	vkDestroyShaderModule(m_device, vert, nullptr);
}

void VulkanEngine::initScene() {
#if 0
	RenderObject monkey = {
		.mesh = getMesh("monkey"),
		.material = getMaterial("default"),
	};

	monkey.matrix = glm::translate(glm::mat4(1), glm::vec3(0, 6, 0));
	m_drawable.push(monkey);

	monkey.matrix = glm::translate(glm::mat4(1), glm::vec3(5, 6, 0));
	m_drawable.push(monkey);

	monkey.matrix = glm::translate(glm::mat4(1), glm::vec3(-5, 6, 0));
	m_drawable.push(monkey);

	monkey.matrix = glm::translate(glm::mat4(1), glm::vec3(0, 10, 0));
	m_drawable.push(monkey);

	monkey.matrix = glm::translate(glm::mat4(1), glm::vec3(0, 2, 0));
	m_drawable.push(monkey);
#else
	RenderObject map = {
		.mesh = getMesh("lost_empire"),
		.material = getMaterial("texturedmesh"),
};
	m_drawable.push(map);

	VkSamplerCreateInfo sampler_info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
	};

	VkSampler blocky_sampler;
	vkCreateSampler(m_device, &sampler_info, nullptr, &blocky_sampler);
	m_main_delete_queue.push(vkDestroySampler, m_device, blocky_sampler, nullptr);

	Material* textured_mat = getMaterial("texturedmesh");

	VkDescriptorSetAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = m_descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &m_single_texture_set_layout,
	};

	vkAllocateDescriptorSets(m_device, &alloc_info, &textured_mat->texture_set);

	VkDescriptorImageInfo image_info = {
		.sampler = blocky_sampler,
		.imageView = m_textures.get("empire_diffuse")->view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkWriteDescriptorSet texture = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = textured_mat->texture_set,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &image_info,
	};

	vkUpdateDescriptorSets(m_device, 1, &texture, 0, nullptr);
#endif
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

	u8 *buf = (u8 *)calloc(file_len, 1);
	assert(buf);

	fread(buf, 1, file_len, fp);
	fclose(fp);

	VkShaderModuleCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = file_len,
		.pCode = (u32 *)buf,
	};

	VkShaderModule module;
	if (vkCreateShaderModule(m_device, &info, nullptr, &module)) {
		module = nullptr;
	}

	free(buf);
	return module;
}

void VulkanEngine::loadMeshes() {
	Mesh monkey;
	monkey.loadFromObj("../../assets/monkey_smooth.obj");
	uploadMesh(monkey);
	m_meshes.push("monkey", monkey);

	Mesh lost_empire;
	lost_empire.loadFromObj("../../assets/lost_empire.obj");
	uploadMesh(lost_empire);
	m_meshes.push("lost_empire", lost_empire);
}

void VulkanEngine::uploadMesh(Mesh &mesh) {
	const size_t buffer_size = mesh.m_verts.size() * sizeof(Vertex);

	VkBufferCreateInfo buf_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = (u32)buffer_size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	};

	VmaAllocationCreateInfo alloc_info = {
		.usage = VMA_MEMORY_USAGE_CPU_ONLY,
	};

	Buffer staging_buffer;

	VK_CHECK(vmaCreateBuffer(
		m_allocator,
		&buf_info,
		&alloc_info,
		&staging_buffer.buffer,
		&staging_buffer.allocation,
		nullptr
	));

	writeGpuMemory(staging_buffer, mesh.m_verts.data(), mesh.m_verts.size());

	buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VK_CHECK(vmaCreateBuffer(
		m_allocator,
		&buf_info,
		&alloc_info,
		&mesh.m_buf.buffer,
		&mesh.m_buf.allocation,
		nullptr
	));
	m_main_delete_queue.push(vmaDestroyBuffer, m_allocator, mesh.m_buf.buffer, mesh.m_buf.allocation);

	immediateSubmit(
		[&buffer_size, &staging_buffer, &mesh]
		(VkCommandBuffer cmd) {
			VkBufferCopy copy = {
				.size = buffer_size,
			};
			vkCmdCopyBuffer(cmd, staging_buffer.buffer, mesh.m_buf.buffer, 1, &copy);
		}
	);

	vmaDestroyBuffer(m_allocator, staging_buffer.buffer, staging_buffer.allocation);
}

Material *VulkanEngine::makeMaterial(VkPipeline pipeline, VkPipelineLayout layout, StrView name) {
	Material mat = {
		.pipeline = pipeline,
		.layout = layout,
	};
	return m_materials.push(name, mat);
	//m_materials[name] = mat;
	//return &m_materials[name];
}

Material *VulkanEngine::getMaterial(StrView name) {
	return m_materials.get(name);
	// auto it = m_materials.find(name);
	// if (it == m_materials.end()) {
	// 	return nullptr;
	// }
	// return &it->second;
}

Mesh *VulkanEngine::getMesh(StrView name) {
	return m_meshes.get(name);
	// auto it = m_meshes.find(name);
	// if (it == m_meshes.end()) {
	// 	return nullptr;
	// }
	// return &it->second;
}

void VulkanEngine::drawObjects(VkCommandBuffer cmd, RenderObject *first, int count) {
	glm::vec3 cam_pos = { 0, -20, -10 };
	glm::mat4 view = glm::translate(glm::mat4(1), cam_pos);
	glm::mat4 proj = glm::perspective(
		glm::radians(70.f),
		(float)(m_windowExtent.width) / (float)(m_windowExtent.height),
		0.1f,
		200.f
	);
	proj[1][1] *= -1;

	FrameData &frame = getFrame();

	GPUCameraData cam_data = {
		.view = view,
		.proj = proj,
		.viewproj = proj * view,
	};

	writeGpuMemory(frame.camera_buf, &cam_data);

	uint64_t frame_index = m_frameNumber % frame_overlap;
	float framed = m_frameNumber / 120.f;
	m_scene_params.ambient_colour = { sin(framed), 0, cos(framed), 1 };

	void *data;
	vmaMapMemory(m_allocator, m_scene_params_buf.allocation, &data);
	
	uint8_t *scene_data = (uint8_t *)data;
	scene_data += padUniformBufferSize(sizeof(GPUSceneData)) * frame_index;
	memcpy(scene_data, &m_scene_params, sizeof(m_scene_params));

	vmaUnmapMemory(m_allocator, m_scene_params_buf.allocation);

	void *obj_data;
	vmaMapMemory(m_allocator, frame.object_buf.allocation, &obj_data);

	GPUObjectData *objects = (GPUObjectData *)obj_data;

	for (int i = 0; i < count; ++i) {
		objects[i].model = first[i].matrix;
	}

	vmaUnmapMemory(m_allocator, frame.object_buf.allocation);


	Mesh *last_mesh = nullptr;
	Material *last_material = nullptr;
	for (int i = 0; i < count; ++i) {
		RenderObject &obj = first[i];

		if (obj.material != last_material) {
			last_material = obj.material;
			uint32_t uniform_offset = (uint32_t)(padUniformBufferSize(sizeof(GPUSceneData)) * frame_index);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, obj.material->pipeline);
			vkCmdBindDescriptorSets(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				obj.material->layout,
				0,
				1,
				&frame.global_descriptor,
				1,
				&uniform_offset
			);

			vkCmdBindDescriptorSets(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				obj.material->layout,
				1,
				1,
				&frame.object_descriptor,
				0,
				nullptr
			);

			if (obj.material->texture_set != VK_NULL_HANDLE) {
				vkCmdBindDescriptorSets(
					cmd,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					obj.material->layout,
					2,
					1,
					&obj.material->texture_set,
					0,
					nullptr
				);
			}
		}

		float time_passed = (float)SDL_GetTicks64() / 1000.f;
		MeshPushConstants constants = {
			.data = { 0, 0, 0, time_passed },
			.render_matrix = obj.matrix,
		};

		vkCmdPushConstants(
			cmd,
			obj.material->layout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(MeshPushConstants),
			&constants
		);

		if (obj.mesh != last_mesh) {
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(
				cmd,
				0, 1,
				&obj.mesh->m_buf.buffer,
				&offset
			);
			last_mesh = obj.mesh;
		}

		vkCmdDraw(cmd, (u32)obj.mesh->m_verts.size(), 1, 0, i);
	}
}

FrameData &VulkanEngine::getFrame() {
	return m_frames[m_frameNumber % frame_overlap];
}

Buffer VulkanEngine::makeBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage) {
	VkBufferCreateInfo buf_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage
	};

	VmaAllocationCreateInfo alloc_info = {
		.usage = memory_usage,
	};

	Buffer buffer = {};

	VK_CHECK(vmaCreateBuffer(
		m_allocator,
		&buf_info,
		&alloc_info,
		&buffer.buffer,
		&buffer.allocation,
		nullptr
	));
	
	return buffer;
}

void VulkanEngine::initDescriptors() {
	VkDescriptorSetLayoutBinding cam_bindings = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	};

	VkDescriptorSetLayoutBinding scene_bindings = {
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	VkDescriptorSetLayoutBinding bindings[] = {
		cam_bindings, 
		scene_bindings
	};

	VkDescriptorSetLayoutCreateInfo set_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = pk_arrlen(bindings),
		.pBindings = bindings,
	};

	vkCreateDescriptorSetLayout(m_device, &set_info, nullptr, &m_global_set_layout);
	m_main_delete_queue.push(vkDestroyDescriptorSetLayout, m_device, m_global_set_layout, nullptr);

	VkDescriptorSetLayoutBinding object_bindings = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	};

	set_info.bindingCount = 1;
	set_info.pBindings = &object_bindings;

	vkCreateDescriptorSetLayout(m_device, &set_info, nullptr, &m_object_set_layout);
	m_main_delete_queue.push(vkDestroyDescriptorSetLayout, m_device, m_object_set_layout, nullptr);

	VkDescriptorSetLayoutBinding texture_bindings = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	set_info.bindingCount = 1;
	set_info.pBindings = &texture_bindings;

	vkCreateDescriptorSetLayout(m_device, &set_info, nullptr, &m_single_texture_set_layout);
	m_main_delete_queue.push(vkDestroyDescriptorSetLayout, m_device, m_single_texture_set_layout, nullptr);

	// create a descriptor pool that will hold 10 uniform buffers
	VkDescriptorPoolSize sizes[] = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
	};

	VkDescriptorPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 10,
		.poolSizeCount = pk_arrlen(sizes),
		.pPoolSizes = sizes
	};

	vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptor_pool);
	m_main_delete_queue.push(vkDestroyDescriptorPool, m_device, m_descriptor_pool, nullptr);

	uint64_t scene_buf_size = frame_overlap * padUniformBufferSize(sizeof(GPUSceneData));
	m_scene_params_buf = makeBuffer(scene_buf_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	m_main_delete_queue.push(vmaDestroyBuffer, m_allocator, m_scene_params_buf.buffer, m_scene_params_buf.allocation);

	for (size_t i = 0; i < pk_arrlen(m_frames); ++i) {
		FrameData &frame = m_frames[i];

		constexpr int max_objects = 10'000;
		frame.object_buf = makeBuffer(sizeof(GPUObjectData) * max_objects, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		m_main_delete_queue.push(vmaDestroyBuffer, m_allocator, frame.object_buf.buffer, frame.object_buf.allocation);

		frame.camera_buf = makeBuffer(
			sizeof(GPUCameraData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);
		m_main_delete_queue.push(vmaDestroyBuffer, m_allocator, frame.camera_buf.buffer, frame.camera_buf.allocation);

		VkDescriptorSetAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = m_descriptor_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &m_global_set_layout,
		};

		VK_CHECK(vkAllocateDescriptorSets(m_device, &alloc_info, &frame.global_descriptor));

		alloc_info.pSetLayouts = &m_object_set_layout;

		VK_CHECK(vkAllocateDescriptorSets(m_device, &alloc_info, &frame.object_descriptor));

		VkDescriptorBufferInfo cam_info = {
			.buffer = frame.camera_buf.buffer,
			.offset = 0,
			.range = sizeof(GPUCameraData),
		};

		VkDescriptorBufferInfo scene_info = {
			.buffer = m_scene_params_buf.buffer,
			.offset = 0,
			.range = sizeof(GPUSceneData),
		};

		VkDescriptorBufferInfo object_info = {
			.buffer = frame.object_buf.buffer,
			.offset = 0,
			.range = sizeof(GPUObjectData) * max_objects,
		};

		VkWriteDescriptorSet cam_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = frame.global_descriptor,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &cam_info,
		};

		VkWriteDescriptorSet scene_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = frame.global_descriptor,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.pBufferInfo = &scene_info,
		};

		VkWriteDescriptorSet object_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = frame.object_descriptor,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &object_info,
		};

		VkWriteDescriptorSet set_writes[] = {
			cam_write, 
			scene_write,
			object_write,
		};

		vkUpdateDescriptorSets(m_device, pk_arrlen(set_writes), set_writes, 0, nullptr);
	}
}

size_t VulkanEngine::padUniformBufferSize(size_t size) {
	size_t min_ubo_alignment = m_gpu_properties.limits.minUniformBufferOffsetAlignment;
	return min_ubo_alignment > 0 ? alignTo(size, min_ubo_alignment) : size;
}

void VulkanEngine::writeGpuMemory(Buffer &buffer, void *data, size_t len) {
	void *gpu_data;
	vmaMapMemory(m_allocator, buffer.allocation, &gpu_data);
	memcpy(gpu_data, data, len);
	vmaUnmapMemory(m_allocator, buffer.allocation);
}

void VulkanEngine::immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&fun) {
	VkCommandBuffer cmd = m_upload_ctx.buffer;

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

	fun(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
	};

	VK_CHECK(vkQueueSubmit(m_gfxqueue, 1, &submit, m_upload_ctx.fence));

	vkWaitForFences(m_device, 1, &m_upload_ctx.fence, true, 9999999999);
	vkResetFences(m_device, 1, &m_upload_ctx.fence);

	vkResetCommandPool(m_device, m_upload_ctx.pool, 0);
}

void VulkanEngine::loadImages() {
	Texture lost_empire;
	
	vkutil::loadImage(*this, "../../assets/lost_empire-RGBA.png", lost_empire.image);

	VkImageViewCreateInfo image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = lost_empire.image.image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_SRGB,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	vkCreateImageView(m_device, &image_info, nullptr, &lost_empire.view);
	m_main_delete_queue.push(vkDestroyImageView, m_device, lost_empire.view, nullptr);

	m_textures.push("empire_diffuse", lost_empire);
}

////////////////////////////////////////////////////////////

static u32 vulkan_print_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void *pUserData
) {
	trace::Level level = trace::Level::Info;
	switch (messageSeverity) {
		case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			level = trace::Level::Info;
			break;
		case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			level = trace::Level::Info;
			break;
		case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			level = trace::Level::Warn;
			break;
		case VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			level = trace::Level::Error;
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

	trace::print(level, "(Vulkan / %s): %s", type, pCallbackData->pMessage);

	return VK_FALSE;
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


VkPipeline PipelineBuilder::build(VkDevice device, VkRenderPass pass) {
	for (auto &stage : m_shader_stages) {
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	}

	// make viewport state from our stored viewport and scissor.
	// at the moment we won't support multiple viewports or scissors
	VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &m_viewport,
		.scissorCount = 1,
		.pScissors = &m_scissor,
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
