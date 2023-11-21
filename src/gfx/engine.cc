#include "engine.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <glm/gtx/transform.hpp>
#include <VkBootstrap.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include "std/logging.h"
#include "std/mem.h"
#include "std/file.h"

#include "mesh.h"
#include "pipeline_builder.h"
#include "camera.h"
#include "core/input.h"
#include "formats/ini.h"

#define PK_VKCHECK(x) \
    do { \
        VkResult err = x; \
        if(err) { \
            fatal("%s: Vulkan error: %d", #x, err);  \
        } \
    } while (0)

VkDevice global_device = nullptr;
VkInstance global_instance = nullptr;
VmaAllocator global_alloc = nullptr;

static u32 vulkan_print_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void									   *pUserData
);
static void setImGuiTheme();

// void DeletionQueue::flush() {
//     for (const auto &del : m_deleters) {
//         del();
//     }
//     m_deleters.clear();
// }

void Engine::init() {
	info("Initializing");

	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);
    
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(
		SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
	);

	m_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		m_window_width,
		m_window_height,
		window_flags
	);

	initGfx();
	initSwapchain();
	initCommandBuffers();
	initDefaultRenderPass();
    initFrameBuffers();
	initSyncStructures();
	initDescriptors();
	initPipeline();
	loadImages();
	initScene();
	initImGui();
}

void Engine::cleanup() {
	info("Cleaning up");

    for (FrameData &frame : m_frames) {
        PK_VKCHECK(vkWaitForFences(m_device, 1, frame.render_fence.getRef(), true, 1000000000));
    }

	ImGui_ImplVulkan_Shutdown();
	SDL_DestroyWindow(m_window);
}

void Engine::run() {
	SDL_Event e;
	bool should_quit = false;

	//main loop
	while (!should_quit) {
		inputNewFrame();

		//Handle events on queue
		while (SDL_PollEvent(&e) != 0) {
			ImGui_ImplSDL2_ProcessEvent(&e);
			inputEvent(e);
			
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) {
                should_quit = true;
            }

			if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				  m_window_width = e.window.data1;
				  m_window_height = e.window.data2;
				  resizeWindow(e.window.data1, e.window.data2);
			}
		}

		if (isKeyPressed(Key::Escape)) {
			should_quit = true;
		}

		m_cam.update();

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(m_window);
		ImGui::NewFrame();

		ImGui::ShowDemoWindow();

		ImGui::LabelText("Camera Position", "%.2f %.2f %.2f", m_cam.pos.x, m_cam.pos.y, m_cam.pos.z);
		ImGui::LabelText("Camera Rotation", "%.2f %.2f", m_cam.pitch, m_cam.yaw);
		ImGui::LabelText("Mouse Position", "%d %d", getMousePos().x, getMousePos().y);
		ImGui::LabelText("Mouse Relative", "%d %d", getMouseRel().x, getMouseRel().y);

		draw();
	}
}

void Engine::immediateSubmit(std::function<void(VkCommandBuffer)> &&fun) {
	VkCommandBuffer cmd = m_upload_ctx.buffer;

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	PK_VKCHECK(vkBeginCommandBuffer(cmd, &begin_info));

	fun(cmd);

	PK_VKCHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
	};

	PK_VKCHECK(vkQueueSubmit(m_gfxqueue, 1, &submit, m_upload_ctx.fence));

	vkWaitForFences(m_device, 1, m_upload_ctx.fence.getRef(), true, 9999999999);
	vkResetFences(m_device, 1, m_upload_ctx.fence.getRef());

	vkResetCommandPool(m_device, m_upload_ctx.pool, 0);
}

VkShaderModule Engine::loadShaderModule(const char *path) {
	arr<byte> data = File::readWhole(path);

	VkShaderModuleCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = data.len,
		.pCode = (u32 *)data.buf,
	};

	VkShaderModule module;
	if (vkCreateShaderModule(m_device, &info, nullptr, &module)) {
		module = nullptr;
	}

	return module;
}

Material *Engine::makeMaterial(VkPipeline pipeline, VkPipelineLayout layout, StrView name) {
	Material mat = {
		.pipeline_ref = pipeline,
		.layout_ref = layout,
	};
	return m_materials.push(name, mem::move(mat));
}

Material *Engine::getMaterial(StrView name) {
	return m_materials.get(name);
}

Mesh *Engine::loadMesh(const char *asset_path, StrView name) {
	Mesh mesh;
	mesh.load(asset_path);
	mesh.upload(*this);
	return m_meshes.push(name, mem::move(mesh));
}

Mesh *Engine::getMesh(StrView name) {
	return m_meshes.get(name);
}

Buffer Engine::makeBuffer(usize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage) {
    VkBufferCreateInfo buf_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage
	};

	VmaAllocationCreateInfo alloc_info = {
		.usage = memory_usage,
	};

	Buffer buffer = {};

	PK_VKCHECK(vmaCreateBuffer(
		m_allocator,
		&buf_info,
		&alloc_info,
		&buffer.buffer,
		&buffer.alloc,
		nullptr
	));
	
	return buffer;
}

usize Engine::padUniformBufferSize(usize size) const {
	size_t min_ubo_alignment = m_gpu_properties.limits.minUniformBufferOffsetAlignment;
	return min_ubo_alignment > 0 ? mem::alignTo(size, min_ubo_alignment) : size;
}

void Engine::initGfx() {
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

	SDL_Vulkan_CreateSurface(m_window, m_instance, m_surface.getRef());

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
    memcpy(&m_gpu_properties, &physical_device.properties, sizeof(m_gpu_properties));

    static_assert(sizeof(m_gpu_properties) == sizeof(physical_device.properties), "");

	m_gfxqueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
	m_gfxqueue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo alloc_info = {
		.physicalDevice = m_chosen_gpu,
		.device = m_device,
		.instance = m_instance,
	};
	vmaCreateAllocator(&alloc_info, m_allocator.getRef());

	info("The GPU has a minimum buffer aligment of %zu", m_gpu_properties.limits.minUniformBufferOffsetAlignment);

	global_device = m_device;
	global_instance = m_instance;
	global_alloc = m_allocator;
}

void Engine::initSwapchain() {
	if (m_window_width == 0 || m_window_height == 0) {
		warn("minimised");
		return;
	}

	if (m_swapchain) {
		m_swapchain_img_views.clear();
		m_swapchain.destroy();
		m_depth_view.destroy();
		m_depth_img.destroy();
		// vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
		// vkDestroyImageView(m_device, m_depth_view, nullptr);
		// vmaDestroyImage(m_allocator, m_depth_img.image, m_depth_img.allocation);
	}

	vkb::SwapchainBuilder sc_builder{ m_chosen_gpu, m_device, m_surface };

	vkb::Swapchain vkb_sc = sc_builder
		.use_default_format_selection()
		.set_desired_format({ VK_FORMAT_R8G8B8A8_UNORM })
		.set_desired_format({ VK_FORMAT_B8G8R8A8_UNORM })
		//.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
		.set_desired_extent(m_window_width, m_window_height)
		.build()
		.value();

	m_swapchain = vkb_sc.swapchain;

	auto images = vkb_sc.get_images().value();
	m_swapchain_images.clear();
	m_swapchain_images.reserve(images.size());
	for (auto i : images) m_swapchain_images.push(i);

	auto views = vkb_sc.get_image_views().value();
	m_swapchain_img_views.clear();
	m_swapchain_img_views.reserve(images.size());
	for (auto v : views) m_swapchain_img_views.push(v);

	m_swapchain_img_format = vkb_sc.image_format;

	VkExtent3D depth_img_extent = {
		.width = m_window_width,
		.height = m_window_height,
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
		&m_depth_img.buffer,
		&m_depth_img.alloc,
		nullptr
	);

	VkImageViewCreateInfo depth_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = m_depth_img.buffer,
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

	PK_VKCHECK(vkCreateImageView(m_device, &depth_view_info, nullptr, m_depth_view.getRef()));

}

void Engine::initCommandBuffers() {
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
		PK_VKCHECK(vkCreateCommandPool(m_device, &cmdpool_info, nullptr, frame.cmd_pool.getRef()));

		cmdalloc_info.commandPool = frame.cmd_pool;
		PK_VKCHECK(vkAllocateCommandBuffers(m_device, &cmdalloc_info, &frame.cmd_buf));
	}

	PK_VKCHECK(vkCreateCommandPool(m_device, &cmdpool_info, nullptr, m_upload_ctx.pool.getRef()));

	cmdalloc_info.commandPool = m_upload_ctx.pool;
	PK_VKCHECK(vkAllocateCommandBuffers(m_device, &cmdalloc_info, &m_upload_ctx.buffer));
}

void Engine::initDefaultRenderPass() {
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

	PK_VKCHECK(vkCreateRenderPass(m_device, &render_pass_info, nullptr, m_render_pass.getRef()));
}

void Engine::initFrameBuffers() {
	bool is_new = m_framebuffers.empty();

	m_framebuffers.clear();

	VkFramebufferCreateInfo fb_info = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = m_render_pass,
		.attachmentCount = 1,
		.width = m_window_width,
		.height = m_window_height,
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
		PK_VKCHECK(vkCreateFramebuffer(m_device, &fb_info, nullptr, m_framebuffers[i].getRef()));
	}
}

void Engine::initSyncStructures() {
	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	VkSemaphoreCreateInfo sem_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	for (FrameData &frame : m_frames) {
		PK_VKCHECK(vkCreateFence(m_device, &fence_info, nullptr, frame.render_fence.getRef()));

		PK_VKCHECK(vkCreateSemaphore(m_device, &sem_info, nullptr, frame.present_sem.getRef()));
		PK_VKCHECK(vkCreateSemaphore(m_device, &sem_info, nullptr, frame.render_sem.getRef()));
	}

	fence_info.flags = 0;
	PK_VKCHECK(vkCreateFence(m_device, &fence_info, nullptr, m_upload_ctx.fence.getRef()));
}

void Engine::initPipeline() {
	vkptr<VkShaderModule> vert = loadShaderModule("../../shaders/mesh.vert.spv");
	vkptr<VkShaderModule> frag = loadShaderModule("../../shaders/triangle.frag.spv");

	if (!vert) err("could not compile vertex shader");
	else info("compiled vertex shader");

	if (!frag) err("could not compile fragment shader");
	else info("compiled fragment shader");

	VkPushConstantRange push_constat = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(Mesh::PushConstants),
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

	vkptr<VkPipelineLayout> pipeline_layout;

	PK_VKCHECK(vkCreatePipelineLayout(m_device, &mesh_pip_layout_info, nullptr, pipeline_layout.getRef()));

	PipelineBuilder pipeline_builder;
	
	vkptr<VkPipeline> mesh_pip = pipeline_builder
		.setVertexInput(Vertex::getVertexDesc())
		.setInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.setViewport(0, 0, (float)m_window_width, (float)m_window_height, 0, 1)
		.setScissor({ m_window_width, m_window_height })
		.setRasterizer(VK_CULL_MODE_NONE)
		.setColourBlend()
		.setMultisampling(VK_SAMPLE_COUNT_1_BIT)
		.setLayout(pipeline_layout)
		.setDepthStencil(VK_COMPARE_OP_LESS_OR_EQUAL)
		.pushShader(VK_SHADER_STAGE_VERTEX_BIT, vert)
		.pushShader(VK_SHADER_STAGE_FRAGMENT_BIT, frag)
		.setDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
		.build(m_device, m_render_pass);

	makeMaterial(mesh_pip, pipeline_layout, "default");
	makeMaterial(mesh_pip, pipeline_layout, "texturedmesh");

	m_pipeline_cache.push(mem::move(mesh_pip));
	m_pipeline_layout_cache.push(mem::move(pipeline_layout));
}

void Engine::initDescriptors() {
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

	vkCreateDescriptorSetLayout(m_device, &set_info, nullptr, m_global_set_layout.getRef());

	VkDescriptorSetLayoutBinding object_bindings = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	};

	set_info.bindingCount = 1;
	set_info.pBindings = &object_bindings;

	vkCreateDescriptorSetLayout(m_device, &set_info, nullptr, m_object_set_layout.getRef());

	VkDescriptorSetLayoutBinding texture_bindings = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	set_info.bindingCount = 1;
	set_info.pBindings = &texture_bindings;

	vkCreateDescriptorSetLayout(m_device, &set_info, nullptr, m_single_texture_set_layout.getRef());

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

	vkCreateDescriptorPool(m_device, &pool_info, nullptr, m_descriptor_pool.getRef());

	uint64_t scene_buf_size = kframe_overlap * padUniformBufferSize(sizeof(SceneData));
	m_scene_params_buf = makeBuffer(scene_buf_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	for (size_t i = 0; i < pk_arrlen(m_frames); ++i) {
		FrameData &frame = m_frames[i];

		constexpr int max_objects = 10'000;
		frame.object_buf = makeBuffer(sizeof(ObjectData) * max_objects, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		frame.camera_buf = makeBuffer(
			sizeof(GPUCameraData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		VkDescriptorSetAllocateInfo alloc_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = m_descriptor_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = m_global_set_layout.getRef(),
		};

		PK_VKCHECK(vkAllocateDescriptorSets(m_device, &alloc_info, &frame.global_descriptor));

		alloc_info.pSetLayouts = m_object_set_layout.getRef();

		PK_VKCHECK(vkAllocateDescriptorSets(m_device, &alloc_info, &frame.object_descriptor));

		VkDescriptorBufferInfo cam_info = {
			.buffer = frame.camera_buf.buffer,
			.offset = 0,
			.range = sizeof(GPUCameraData),
		};

		VkDescriptorBufferInfo scene_info = {
			.buffer = m_scene_params_buf.buffer,
			.offset = 0,
			.range = sizeof(SceneData),
		};

		VkDescriptorBufferInfo object_info = {
			.buffer = frame.object_buf.buffer,
			.offset = 0,
			.range = sizeof(ObjectData) * max_objects,
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

void Engine::initScene() {
    // loadMesh("../../assets/imported/monkey_smooth.mesh", "monkey");
    loadMesh("../../assets/imported/lost_empire.mesh", "lost_empire");
    // loadMesh("../../assets/imported/triangle.mesh", "triangle");

    RenderObject map = {
        .mesh = getMesh("lost_empire"),
        .material = getMaterial("texturedmesh"),
		.matrix = glm::mat4(1),
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

	vkptr<VkSampler> blocky_sampler;
	vkCreateSampler(m_device, &sampler_info, nullptr, blocky_sampler.getRef());

	Material* textured_mat = getMaterial("texturedmesh");

	VkDescriptorSetAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = m_descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = m_single_texture_set_layout.getRef(),
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

	m_sampler_cache.push(mem::move(blocky_sampler));
}

void Engine::initImGui() {
	// Create descriptor pool for ImGui
	VkDescriptorPoolSize pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 1000,
		.poolSizeCount = pk_arrlen(pool_sizes),
		.pPoolSizes = pool_sizes
	};

	PK_VKCHECK(vkCreateDescriptorPool(m_device, &pool_info, nullptr, m_imgui_pool.getRef()));

	// Initialise imgui
	ImGui::CreateContext();

	ImGui_ImplSDL2_InitForVulkan(m_window);

	ImGui_ImplVulkan_InitInfo init_info = {
		.Instance = m_instance,
		.PhysicalDevice = m_chosen_gpu,
		.Device = m_device,
		.Queue = m_gfxqueue,
		.DescriptorPool = m_imgui_pool,
		.MinImageCount = 3,
		.ImageCount = 3,
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
	};

	ImGui_ImplVulkan_Init(&init_info, m_render_pass);

	setImGuiTheme();
}

void Engine::resizeWindow(int new_width, int new_height) {
	// resize imgui
	//ImGui_ImplVulkanH_CreateOrResizeWindow(m_instance, m_chosen_gpu, m_device, nullptr, m_gfxqueue_family, nullptr, new_width, new_height, 2);

	vkDeviceWaitIdle(m_device);

	initSwapchain();
	initFrameBuffers();
}

void Engine::loadImages() {
	Texture lost_empire;
    lost_empire.load(*this, "../../assets/imported/lost_empire-RGBA.tx");

	VkImageViewCreateInfo image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = lost_empire.image.buffer,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	vkCreateImageView(m_device, &image_info, nullptr, lost_empire.view.getRef());

	m_textures.push("empire_diffuse", mem::move(lost_empire));
}

#if 0
void Engine::uploadMesh(Mesh &mesh) {
	const size_t buffer_size = mesh.verts.byteSize();

	VkBufferCreateInfo buf_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = (u32)buffer_size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	};

	VmaAllocationCreateInfo alloc_info = {
		.usage = VMA_MEMORY_USAGE_CPU_ONLY,
	};

	Buffer staging_buffer;

	PK_VKCHECK(vmaCreateBuffer(
		m_allocator,
		&buf_info,
		&alloc_info,
		&staging_buffer.buffer,
		&staging_buffer.allocation,
		nullptr
	));

	void *gpu_data;
	vmaMapMemory(m_allocator, staging_buffer.allocation, &gpu_data);
	memcpy(gpu_data, mesh.verts.data(), buffer_size);
	vmaUnmapMemory(m_allocator, staging_buffer.allocation);

	buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	PK_VKCHECK(vmaCreateBuffer(
		m_allocator,
		&buf_info,
		&alloc_info,
		&mesh.buffer.buffer,
		&mesh.buffer.allocation,
		nullptr
	));

	immediateSubmit(
		[&buffer_size, &staging_buffer, &mesh]
		(VkCommandBuffer cmd) {
			VkBufferCopy copy = {
				.size = buffer_size,
			};
			vkCmdCopyBuffer(cmd, staging_buffer.buffer, mesh.buffer.buffer, 1, &copy);
		}
	);

	vmaDestroyBuffer(m_allocator, staging_buffer.buffer, staging_buffer.allocation);
}
#endif

void Engine::draw() {
	ImGui::Render();

	FrameData &frame = getCurrentFrame();

	PK_VKCHECK(vkWaitForFences(m_device, 1, frame.render_fence.getRef(), true, 1000000000));
	PK_VKCHECK(vkResetFences(m_device, 1, frame.render_fence.getRef()));
	
	PK_VKCHECK(vkResetCommandBuffer(frame.cmd_buf, 0));

	u32 sc_img_index = 0;
	PK_VKCHECK(vkAcquireNextImageKHR(m_device, m_swapchain, 1000000000, frame.present_sem, nullptr, &sc_img_index));

	VkCommandBuffer cmd = frame.cmd_buf;

	VkCommandBufferBeginInfo beg_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	PK_VKCHECK(vkBeginCommandBuffer(cmd, &beg_info));
	
	VkClearValue clear_col = {
		.color = { { 0.2f, 0.3f, 0.4f, 1.f } }
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
			.extent = { m_window_width, m_window_height },
		},
		.clearValueCount = pk_arrlen(clear_values),
		.pClearValues = clear_values
	};

	vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

	VkRect2D scissor = { .extent = { m_window_width, m_window_height } };
	VkViewport viewport = { 0, 0, (float)m_window_width, (float)m_window_height, 0, 1 };

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	drawObjects(cmd, m_drawable);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRenderPass(cmd);
	PK_VKCHECK(vkEndCommandBuffer(cmd));

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submit = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = frame.present_sem.getRef(),
		.pWaitDstStageMask = &wait_stage,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = frame.render_sem.getRef(),
	};

	PK_VKCHECK(vkQueueSubmit(m_gfxqueue, 1, &submit, frame.render_fence));

	VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = frame.render_sem.getRef(),
		.swapchainCount = 1,
		.pSwapchains = m_swapchain.getRef(),
		.pImageIndices = &sc_img_index,
	};

	PK_VKCHECK(vkQueuePresentKHR(m_gfxqueue, &present_info));

	++m_frame_num;
}

void Engine::drawObjects(VkCommandBuffer cmd, Slice<RenderObject> objects) {
	glm::mat4 view = m_cam.getView();
	glm::mat4 proj = glm::perspective(
		glm::radians(70.f),
		(float)(m_window_width) / (float)(m_window_height),
		0.1f,
		200.f
	);
	proj[1][1] *= -1;

	FrameData &frame = getCurrentFrame();

	GPUCameraData cam_data = {
		.view = view,
		.proj = proj,
		.viewproj = proj * view,
	};

	void *gpu_data;
	vmaMapMemory(m_allocator, frame.camera_buf.alloc, &gpu_data);
	memcpy(gpu_data, &cam_data, sizeof(cam_data));
	vmaUnmapMemory(m_allocator, frame.camera_buf.alloc);

	uint64_t frame_index = m_frame_num % kframe_overlap;
	float framed = m_frame_num / 120.f;
	m_scene_params.ambient_colour = { sin(framed), 0, cos(framed), 1 };

	void *data;
	vmaMapMemory(m_allocator, m_scene_params_buf.alloc, &data);
	
	uint8_t *scene_data = (uint8_t *)data;
	scene_data += padUniformBufferSize(sizeof(SceneData)) * frame_index;
	memcpy(scene_data, &m_scene_params, sizeof(m_scene_params));

	vmaUnmapMemory(m_allocator, m_scene_params_buf.alloc);

	void *obj_data;
	vmaMapMemory(m_allocator, frame.object_buf.alloc, &obj_data);

	ObjectData *gpu_objects = (ObjectData *)obj_data;

	for (usize i = 0; i < objects.len; ++i) {
		gpu_objects[i].model = objects[i].matrix;
	}

	vmaUnmapMemory(m_allocator, frame.object_buf.alloc);

	Mesh *last_mesh = nullptr;
	Material *last_material = nullptr;
	for (usize i = 0; i < objects.len; ++i) {
		const RenderObject &obj = objects[i];

		if (obj.material != last_material) {
			last_material = obj.material;
			uint32_t uniform_offset = (uint32_t)(padUniformBufferSize(sizeof(SceneData)) * frame_index);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, obj.material->pipeline_ref);
			vkCmdBindDescriptorSets(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				obj.material->layout_ref,
				0,
				1,
				&frame.global_descriptor,
				1,
				&uniform_offset
			);

			vkCmdBindDescriptorSets(
				cmd,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				obj.material->layout_ref,
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
					obj.material->layout_ref,
					2,
					1,
					&obj.material->texture_set,
					0,
					nullptr
				);
			}
		}

		float time_passed = (float)SDL_GetTicks64() / 1000.f;
		Mesh::PushConstants constants = {
			.data = { 0, 0, 0, time_passed },
			.model = obj.matrix,
		};

		vkCmdPushConstants(
			cmd,
			obj.material->layout_ref,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(Mesh::PushConstants),
			&constants
		);

		if (obj.mesh != last_mesh) {
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(
				cmd,
				0, 1,
				&obj.mesh->vbuf.buffer,
				&offset
			);
			vkCmdBindIndexBuffer(
				cmd,
				obj.mesh->ibuf.buffer,
				0,
				VK_INDEX_TYPE_UINT32
			);
			last_mesh = obj.mesh;
		}

		vkCmdDrawIndexed(cmd, (u32)obj.mesh->indices.size(), 1, 0, 0, (u32)i);
		//vkCmdDraw(cmd, (u32)obj.mesh->verts.size(), 1, 0, (u32)i);
	}
}

Engine::FrameData &Engine::getCurrentFrame() {
	return m_frames[m_frame_num % kframe_overlap];
}

static u32 vulkan_print_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void									   *pUserData
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

static void setImGuiTheme() {
	// from https://github.com/OverShifted/OverEngine
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_Border]                = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.08f, 0.50f, 0.72f, 1.00f);
	style.Colors[ImGuiCol_Button]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	style.Colors[ImGuiCol_Header]                = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	style.Colors[ImGuiCol_Separator]             = style.Colors[ImGuiCol_Border];
	style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.41f, 0.42f, 0.44f, 1.00f);
	style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.29f, 0.30f, 0.31f, 0.67f);
	style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	style.Colors[ImGuiCol_Tab]                   = ImVec4(0.08f, 0.08f, 0.09f, 0.83f);
	style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.33f, 0.34f, 0.36f, 0.83f);
	style.Colors[ImGuiCol_TabActive]             = ImVec4(0.23f, 0.23f, 0.24f, 1.00f);
	style.Colors[ImGuiCol_TabUnfocused]          = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	style.Colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_DockingPreview]        = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
	style.Colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	style.GrabRounding                           = style.FrameRounding = 2.3f;
}