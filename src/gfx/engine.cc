#include "engine.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <VkBootstrap.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#include "std/logging.h"
#include "std/mem.h"
#include "std/file.h"
#include "std/callstack.h"

#include "assets/asset_manager.h"

#include "mesh.h"
#include "pipeline_builder.h"
#include "camera.h"
#include "shader.h"
#include "core/input.h"
#include "assets/asset_manager.h"
#include "assets/texture.h"
#include "formats/ini.h"

#include <chrono>
using timer = std::chrono::high_resolution_clock;

// using PFN_vkCmdDrawMeshTasksEXT = void(VkCommandBuffer, u32, u32, u32);
PFN_vkCmdDrawMeshTasksEXT fn_vkCmdDrawMeshTasksEXT = nullptr;

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

void Engine::init() {
	info("Initializing");

	CallStack::init();
	jobpool.start(5);

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

	tracy_helper.init();
	async_transfer.init(m_transferqueue, m_transferqueue_family, true);
	AssetManager::loadDefaults();

	initDescriptors();
	initPipeline();

	loadImages();
	initScene();
	initImGui();
	
}

void Engine::cleanup() {
	info("Cleaning up");

    for (FrameData &frame : m_frames) {
        PK_VKCHECK(vkWaitForFences(m_device, 1, frame.render_fence.getRef(), true, 9999999999));
    }

	tracy_helper.cleanup();

	//PK_VKCHECK(vkQueueWaitIdle(m_transferqueue));
	//PK_VKCHECK(vkWaitForFences(m_device, 1, transfer_fence.getRef(), true, 9999999999));

	ImGui_ImplVulkan_Shutdown();
	SDL_DestroyWindow(m_window);

	AssetManager::cleanup();
	CallStack::cleanup();
	jobpool.stop();
}

void Engine::run() {
	SDL_Event e;
	bool should_quit = false;

	auto time_now = timer::now();
	auto time_last = time_now;

	//main loop
	while (!should_quit) {
		inputNewFrame();

		time_last = time_now;
		time_now = timer::now();
		usize time_diff = std::chrono::duration_cast<std::chrono::microseconds>(time_now - time_last).count();
		frame_time = (double)time_diff / 1000000.0;

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

		async_transfer.updateWithFence();

		m_cam.update();

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(m_window);
		ImGui::NewFrame();

		drawFpsWidget();
		draw();
	}
}

void Engine::immediateSubmit(Delegate<void(VkCommandBuffer)> &&fun) {
	m_upload_ctx.mtx.lock();

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

	m_upload_ctx.mtx.unlock();
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
	mesh.load(asset_path, name);
	// mesh.upload();
	return m_meshes.push(name, mem::move(mesh));
}

Mesh *Engine::getMesh(StrView name) {
	return m_meshes.get(name);
}

usize Engine::padUniformBufferSize(usize size) const {
	size_t min_ubo_alignment = m_gpu_properties.limits.minUniformBufferOffsetAlignment;
	return min_ubo_alignment > 0 ? mem::alignTo(size, min_ubo_alignment) : size;
}

void Engine::initGfx() {
	vkb::InstanceBuilder builder;

	auto inst_ret = builder.
		set_app_name("Vulkan Application")
		.request_validation_layers(PK_DEBUG)
		.require_api_version(1, 2, 0)
		.set_debug_callback(vulkan_print_callback)
		//.enable_extension("VK_NV_mesh_shader")
		//.enable_extension("VK_EXT_mesh_shader")
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	m_instance = vkb_inst.instance;
	m_debug_messenger = vkb_inst.debug_messenger;
	fn_vkCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)vkb_inst.fp_vkGetInstanceProcAddr(m_instance, "vkCmdDrawMeshTasksEXT");
	pk_assert(fn_vkCmdDrawMeshTasksEXT);

	SDL_Vulkan_CreateSurface(m_window, m_instance, m_surface.getRef());

	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physical_device = selector
		.set_minimum_version(1, 1)
		.set_surface(m_surface)
		.add_required_extension("VK_NV_mesh_shader")
		.add_required_extension("VK_EXT_mesh_shader")
		.select()
		.value();

	VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_params_feature = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
		.shaderDrawParameters = true,
	};

	VkPhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
		.taskShader = true,
		.meshShader = true,
	};

	//VkPhysicalDeviceMeshShaderFeaturesNV mesh_shader_feature = {
	//	.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV,
	//	.taskShader = true,
	//	.meshShader = true,
	//};

	vkb::DeviceBuilder device_builder{ physical_device };
	vkb::Device vkb_device = device_builder
		.add_pNext(&shader_draw_params_feature)
		.add_pNext(&mesh_shader_features)
		.build()
		.value();

	m_device = vkb_device.device;
	m_chosen_gpu = physical_device.physical_device;
    memcpy(&m_gpu_properties, &physical_device.properties, sizeof(m_gpu_properties));

    static_assert(sizeof(m_gpu_properties) == sizeof(physical_device.properties), "");

	VkPhysicalDeviceMeshShaderPropertiesNV mesh_shader_properties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV,
	};

	VkPhysicalDeviceProperties2 device_properties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		.pNext = &mesh_shader_properties,
	};

	vkGetPhysicalDeviceProperties2(m_chosen_gpu, &device_properties);

	mesh_shader_properties;

    info("maxDrawMeshTasksCount: %u", mesh_shader_properties.maxDrawMeshTasksCount);
    info("maxTaskWorkGroupInvocations: %u", mesh_shader_properties.maxTaskWorkGroupInvocations);
    info("maxTaskWorkGroupSize: %u %u %u", mesh_shader_properties.maxTaskWorkGroupSize[0], mesh_shader_properties.maxTaskWorkGroupSize[1], mesh_shader_properties.maxTaskWorkGroupSize[2]);
    info("maxTaskTotalMemorySize: %u", mesh_shader_properties.maxTaskTotalMemorySize);
    info("maxTaskOutputCount: %u", mesh_shader_properties.maxTaskOutputCount);
    info("maxMeshWorkGroupInvocations: %u", mesh_shader_properties.maxMeshWorkGroupInvocations);
    info("maxMeshWorkGroupSize: %u %u %u", mesh_shader_properties.maxMeshWorkGroupSize[0], mesh_shader_properties.maxMeshWorkGroupSize[1], mesh_shader_properties.maxMeshWorkGroupSize[2]);
    info("maxMeshTotalMemorySize: %u", mesh_shader_properties.maxMeshTotalMemorySize);
    info("maxMeshOutputVertices: %u", mesh_shader_properties.maxMeshOutputVertices);
    info("maxMeshOutputPrimitives: %u", mesh_shader_properties.maxMeshOutputPrimitives);
    info("maxMeshMultiviewViewCount: %u", mesh_shader_properties.maxMeshMultiviewViewCount);
    info("meshOutputPerVertexGranularity: %u", mesh_shader_properties.meshOutputPerVertexGranularity);
    info("meshOutputPerPrimitiveGranularity: %u", mesh_shader_properties.meshOutputPerPrimitiveGranularity);

	m_gfxqueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
	m_gfxqueue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

	m_transferqueue = vkb_device.get_queue(vkb::QueueType::transfer).value();
	m_transferqueue_family = vkb_device.get_queue_index(vkb::QueueType::transfer).value();

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

    m_desc_cache.init(m_device);
    m_desc_alloc.init(m_device);
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

		frame.async_gfx.init(m_gfxqueue, m_gfxqueue_family);
	}

	fence_info.flags = 0;
	PK_VKCHECK(vkCreateFence(m_device, &fence_info, nullptr, m_upload_ctx.fence.getRef()));
}

void Engine::initPipeline() {
	{
		ShaderCompiler mesh_compiler;
		mesh_compiler.init(m_device);
		mesh_compiler.addStage("shaders/spv/mesh-shader.mesh.spv");
		mesh_compiler.addStage("shaders/spv/mesh-shader.frag.spv");
		mesh_compiler.build(m_desc_cache);

		PipelineBuilder pipeline_builder;

		vkptr<VkPipeline> mesh_pip = pipeline_builder
			// .setVertexInput(Vertex::getVertexDesc())
			.setInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
			.setRasterizer(VK_CULL_MODE_NONE)
			.setColourBlend()
			.setMultisampling(VK_SAMPLE_COUNT_1_BIT)
			.setDepthStencil(VK_COMPARE_OP_LESS_OR_EQUAL)
			.pushShaders(mesh_compiler)
			.setDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
			.build(m_device, m_render_pass);

		makeMaterial(mesh_pip, mesh_compiler.pipeline_layout, "mesh");

		m_pipeline_cache.push(mem::move(mesh_pip));
		m_pipeline_layout_cache.push(mem::move(mesh_compiler.pipeline_layout));
	}

	ShaderCompiler shader_compiler;
	shader_compiler.init(m_device);
	shader_compiler.addStage("shaders/spv/mesh.vert.spv");
	shader_compiler.addStage("shaders/spv/triangle.frag.spv", { { "scene_data", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC } });
	shader_compiler.build(m_desc_cache);

	PipelineBuilder pipeline_builder;
	
	vkptr<VkPipeline> mesh_pip = pipeline_builder
		.setVertexInput(Vertex::getVertexDesc())
		.setInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.setViewport(0, 0, (float)m_window_width, (float)m_window_height, 0, 1)
		.setScissor({ m_window_width, m_window_height })
		.setRasterizer(VK_CULL_MODE_FRONT_BIT)
		.setColourBlend()
		.setMultisampling(VK_SAMPLE_COUNT_1_BIT)
		.setDepthStencil(VK_COMPARE_OP_LESS_OR_EQUAL)
		.pushShaders(shader_compiler)
		//.setLayout(pipeline_layout)
		//.pushShader(VK_SHADER_STAGE_VERTEX_BIT, vert)
		//.pushShader(VK_SHADER_STAGE_FRAGMENT_BIT, frag)
		.setDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
		.build(m_device, m_render_pass);

	makeMaterial(mesh_pip, shader_compiler.pipeline_layout, "default");
	makeMaterial(mesh_pip, shader_compiler.pipeline_layout, "texturedmesh");

	m_pipeline_cache.push(mem::move(mesh_pip));
	m_pipeline_layout_cache.push(mem::move(shader_compiler.pipeline_layout));
}

void Engine::initDescriptors() {
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
	m_scene_params_buf = Buffer::make(scene_buf_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	VkDescriptorSetLayoutBinding texture_bindings = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	VkDescriptorSetLayoutCreateInfo set_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &texture_bindings,
	};

	m_single_texture_set_layout = m_desc_cache.createDescLayout(set_info);

	for (size_t i = 0; i < pk_arrlen(m_frames); ++i) {
		FrameData &frame = m_frames[i];

		constexpr int max_objects = 10'000;
		frame.object_buf = Buffer::make(sizeof(ObjectData) * max_objects, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		frame.camera_buf = Buffer::make(
			sizeof(GPUCameraData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		Buffer *obj_buf = frame.object_buf.get();
		Buffer *cam_buf = frame.camera_buf.get();
		Buffer *scene_buf = m_scene_params_buf.get();

		frame.global_descriptor = DescriptorBuilder::begin(m_desc_cache, m_desc_alloc)
			.bindBuffer(0, { cam_buf->value, 0, sizeof(GPUCameraData) }, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.bindBuffer(1, { scene_buf->value, 0, sizeof(SceneData) }, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(m_global_set_layout);

		frame.object_descriptor = DescriptorBuilder::begin(m_desc_cache, m_desc_alloc)
			.bindBuffer(0, { obj_buf->value, 0, sizeof(ObjectData) * max_objects }, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.build(m_object_set_layout);
	}
}

void Engine::initScene() {
    loadMesh("imported/monkey_smooth.mesh", "monkey");
    loadMesh("guy/soldier.mesh", "guy");
    loadMesh("imported/lost_empire.mesh", "lost_empire");
    loadMesh("imported/triangle.mesh", "triangle");

    RenderObject map = {
        .mesh = getMesh("guy"),
        .material = getMaterial("texturedmesh"),
		.matrix = glm::mat4(1),
    };

	glm::mat4 rot = glm::toMat4(glm::quat(glm::vec3(math::toRad(-84.f), math::toRad(25.4f), math::toRad(-166.8f))));

	for (int y = 0; y < 5; ++y) {
		for (int x = 0; x < 5; ++x) {
			glm::mat4 pos = glm::translate(glm::vec3(x * 50, y * 50, 0));
			map.matrix = rot * pos;
			m_drawable.push(map);
		}
	}

	m_drawable.push(RenderObject{
		.mesh = nullptr,
		.material = getMaterial("mesh"),
		.matrix = glm::mat4(1),
	});

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

	Texture *default_texture = AssetManager::get(Handle<Texture>(0));
	default_material = getMaterial("default");
	default_material->texture_desc = Descriptor::make(
		AsyncDescBuilder::begin()
			.bindImage(0, 0, blocky_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
	);

	Handle<Texture> lost_empire_tex = Texture::load("guy/bojovnikDiffuseMap.jpg");

	map.material->texture_desc = Descriptor::make(
		AsyncDescBuilder::begin()
			.bindImage(0, lost_empire_tex, blocky_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
	);

	m_sampler_cache.push(mem::move(blocky_sampler));

	m_cam.pos.z += 5.f;
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
	vkDeviceWaitIdle(m_device);

	initSwapchain();
	initFrameBuffers();
}

void Engine::loadImages() {
#if 0
	Texture lost_empire;
    lost_empire.load(*this, "imported/lost_empire-RGBA.tx");

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
#endif
}

void Engine::draw() {
	// TODO: handle minimize

	ImGui::Render();

	FrameData &frame = getCurrentFrame();

	PK_VKCHECK(vkWaitForFences(m_device, 1, frame.render_fence.getRef(), true, 1000000000));
	PK_VKCHECK(vkResetFences(m_device, 1, frame.render_fence.getRef()));
	
	PK_VKCHECK(vkResetCommandBuffer(frame.cmd_buf, 0));

	if (!frame.async_gfx.can_submit) {
		frame.async_gfx.resetSubmitList();
	}

	u32 sc_img_index = 0;
	PK_VKCHECK(vkAcquireNextImageKHR(m_device, m_swapchain, 1000000000, frame.present_sem, nullptr, &sc_img_index));

	VkCommandBuffer cmd = frame.cmd_buf;

	VkCommandBufferBeginInfo beg_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	PK_VKCHECK(vkBeginCommandBuffer(cmd, &beg_info));

	frame.async_gfx.update(cmd);

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
	if (!AssetManager::areDefaultsLoaded()) {
		return;
	}

	glm::mat4 view = m_cam.getView();
	glm::mat4 proj = glm::perspective(
		glm::radians(70.f),
		(float)(m_window_width) / (float)(m_window_height),
		0.1f,
		2000.f
	);
	proj[1][1] *= -1;

	FrameData &frame = getCurrentFrame();

	GPUCameraData cam_data = {
		.view = view,
		.proj = proj,
		.viewproj = proj * view,
	};

	uint64_t frame_index = m_frame_num % kframe_overlap;
	float framed = m_frame_num / 120.f;
	m_scene_params.ambient_colour = { sin(framed), 0, cos(framed), 1 };

	Buffer *camera_buf = frame.camera_buf.get();
	Buffer *object_buf = frame.object_buf.get();
	Buffer *scene_buf = m_scene_params_buf.get();

	// Camera Buffer ////////////////////////////////

	void *gpu_data = camera_buf->map();
	memcpy(gpu_data, &cam_data, sizeof(cam_data));
	camera_buf->unmap();

	// Scene Buffer /////////////////////////////////

	u8 *scene_data = scene_buf->map<u8>();
	scene_data += padUniformBufferSize(sizeof(SceneData)) * frame_index;

	memcpy(scene_data, &m_scene_params, sizeof(m_scene_params));
	scene_buf->unmap();

	// Object Buffer ////////////////////////////////

	ObjectData *gpu_objects = object_buf->map<ObjectData>();

	for (usize i = 0; i < objects.len; ++i) {
		gpu_objects[i].model = objects[i].matrix;
	}

	object_buf->unmap();

#if 0
	// Generate Batches /////////////////////////////

	struct Batch {
		Mesh *mesh;
		Material *material;
		u32 count;
	};

	arr<Batch> batches;

	Mesh *last_mesh = nullptr;
	Material *last_material = nullptr;
	for (const RenderObject &obj : objects) {
		if (obj.material != last_material) {
			last_material = obj.material;
			Batch batch;
			batch.mesh = obj.mesh;
			batch.material = obj.material;
			batch.count = 1;
			batches.push(batch);
		}
		else {
			batches.back().count++;
		}
	}

	u32 instance_offset = 0;
	for (Batch &batch : batches) {
		// bind material
		uint32_t uniform_offset = (uint32_t)(padUniformBufferSize(sizeof(SceneData)) * frame_index);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, batch.material->pipeline_ref);
		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			batch.material->layout_ref,
			0,
			1,
			&frame.global_descriptor,
			1,
			&uniform_offset
		);

		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			batch.material->layout_ref,
			1,
			1,
			&frame.object_descriptor,
			0,
			nullptr
		);

		Descriptor *texture_desc = batch.material->texture_desc.get();
		if (!texture_desc) {
			texture_desc = default_material->texture_desc.get();
		}

		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			batch.material->layout_ref,
			2,
			1,
			&texture_desc->set,
			0,
			nullptr
		);
		
		// bind mesh
		Buffer *vbuf = batch.mesh->vbuf.get();
		Buffer *ibuf = batch.mesh->ibuf.get();
		if (!vbuf || !ibuf) continue;

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(
			cmd,
			0, 1,
			&vbuf->value.buffer,
			&offset
		);
		vkCmdBindIndexBuffer(
			cmd,
			ibuf->value.buffer,
			0,
			VK_INDEX_TYPE_UINT32
		);

		vkCmdDrawIndexed(cmd, batch.mesh->index_count, batch.count, 0, 0, instance_offset);
		instance_offset += batch.count;
	}
#endif

	if (Material *mat = getMaterial("mesh")) {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->pipeline_ref);
		fn_vkCmdDrawMeshTasksEXT(cmd, 1, 1, 1);
	}
}

void Engine::drawFpsWidget() {
    static int location = 0;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = 
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | 
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
	
    if (location >= 0) {
        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = (location & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y = (location & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (location & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = (location & 2) ? 1.0f : 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowViewport(viewport->ID);
        window_flags |= ImGuiWindowFlags_NoMove;
    }
    else if (location == -2) {
        // Center window
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        window_flags |= ImGuiWindowFlags_NoMove;
    }
    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
    if (ImGui::Begin("Debug", nullptr, window_flags)) {
		ImGui::Text("Fps: %g", 1.0 / frame_time);
        if (ImGui::BeginPopupContextWindow()) {
            if (ImGui::MenuItem("Custom",       NULL, location == -1)) location = -1;
            if (ImGui::MenuItem("Center",       NULL, location == -2)) location = -2;
            if (ImGui::MenuItem("Top-left",     NULL, location == 0)) location = 0;
            if (ImGui::MenuItem("Top-right",    NULL, location == 1)) location = 1;
            if (ImGui::MenuItem("Bottom-left",  NULL, location == 2)) location = 2;
            if (ImGui::MenuItem("Bottom-right", NULL, location == 3)) location = 3;
            ImGui::EndPopup();
        }
    }
    ImGui::End();
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

void Engine::AsyncQueue::init(VkQueue in_queue, u32 in_family, bool use_fence) {
	queue = in_queue;
	family = in_family;

	const arr<Thread> &threads = g_engine->jobpool.getThreads();

	for (const Thread &thr : threads) {
		addPool(thr.getId());
	}

	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	};

	PK_VKCHECK(vkCreateFence(g_engine->m_device, &fence_info, nullptr, fence.getRef()));
}

VkCommandBuffer Engine::AsyncQueue::getCmd() {
	VkCommandBuffer cmd = nullptr;

	u32 pool_index = getPoolIndex();

	pool_mtx.lock();
	PoolData &pool = pools[pool_index];
	if (!pool.freelist.empty()) {
		VkCommandBuffer cmd = pool.freelist.back();
		pool.freelist.pop();
	}
	pool_mtx.unlock();

	if (!cmd) {
		cmd = allocCmd();
	}

	VkCommandBufferInheritanceInfo inheritance_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
	};

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = &inheritance_info,
	};

	vkBeginCommandBuffer(cmd, &begin_info);

	return cmd;
}

u64 Engine::AsyncQueue::trySubmitCmd(VkCommandBuffer cmd) {
	if (!can_submit) return 0;

	info("cmd: 0x%x", cmd);
	vkEndCommandBuffer(cmd);

	u32 pool_index = getPoolIndex();

	submit_mtx.lock();
	submit.push(cmd);
	submit_data.push(pool_index);
	submit_mtx.unlock();

	return cur_generation;
}

bool Engine::AsyncQueue::isFinished(u64 generation) {
	return cur_generation > generation;
}

void Engine::AsyncQueue::waitUntilFinished(VkCommandBuffer cmd) {
	u64 wait_handle = 0;
	while (!(wait_handle = trySubmitCmd(cmd))) {
		co::yield();
	}

	while (!isFinished(wait_handle)) {
		co::yield();
	}
}

void Engine::AsyncQueue::update(VkCommandBuffer cmd) {
	submit_mtx.lock();

	if (!submit.empty()) {
		can_submit = false;
		vkCmdExecuteCommands(cmd, (u32)submit.len, submit.buf);
	}

	submit_mtx.unlock();
}

void Engine::AsyncQueue::updateWithFence() {
	if (!can_submit) {
		VkResult wait_result = vkWaitForFences(g_engine->m_device, 1, fence.getRef(), true, 0);
		if (wait_result == VK_SUCCESS) {
			vkResetFences(g_engine->m_device, 1, fence.getRef());
			resetSubmitList();
		}
		else {
			return;
		}
	}

	submit_mtx.lock();

	if (!submit.empty()) {
		can_submit = false;
		info("submitting %zu commands for transfer", submit.len);

		VkCommandBufferBeginInfo begin_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};

		PK_VKCHECK(vkBeginCommandBuffer(cmdbuf, &begin_info));
		vkCmdExecuteCommands(cmdbuf, (u32)submit.len, submit.buf);
		PK_VKCHECK(vkEndCommandBuffer(cmdbuf));

		VkSubmitInfo submit = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &cmdbuf,
		};

		PK_VKCHECK(vkQueueSubmit(queue, 1, &submit, fence));
	}

	submit_mtx.unlock();
}

VkCommandBuffer Engine::AsyncQueue::allocCmd() {
	u32 pool_index = getPoolIndex();

	pool_mtx.lock();
	VkCommandBufferAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pools[pool_index].pool,
		.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
		.commandBufferCount = 1,
	};

	VkCommandBuffer cmd = nullptr;
	PK_VKCHECK(vkAllocateCommandBuffers(g_engine->m_device, &alloc_info, &cmd));

	pool_mtx.unlock();
	return cmd;
}

void Engine::AsyncQueue::addPool(uptr thread_id) {
	VkCommandPoolCreateInfo cmdpool_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		// allow pool to reset individual command buffers
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = family,
	};

	VkCommandBufferAllocateInfo cmdalloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	VkCommandPool new_pool;
	PK_VKCHECK(vkCreateCommandPool(g_engine->m_device, &cmdpool_info, nullptr, &new_pool));

	PoolData data;
	data.thread_id = Thread::currentId();
	data.pool = new_pool;

	pool_mtx.lock();
	pools.push(mem::move(data));
	pool_mtx.unlock();

	// TODO could preallocate a few buffers and put them in the freelist

	if (!fence) return;

	pool_mtx.lock();
	cmdalloc_info.commandPool = new_pool;
	PK_VKCHECK(vkAllocateCommandBuffers(g_engine->m_device, &cmdalloc_info, &cmdbuf));
	pool_mtx.unlock();
}

u32 Engine::AsyncQueue::getPoolIndex() {
	// try to find the queue
	uptr thread_id = Thread::currentId();

	pool_mtx.lock();
	for (usize i = 0; i < pools.len; ++i) {
		if (pools[i].thread_id == thread_id) {
			pool_mtx.unlock();
			return (u32)i;
		}
	}

	u32 index = (u32)pools.len;
	pool_mtx.unlock();

	addPool(thread_id);

	return index;
}

void Engine::AsyncQueue::resetSubmitList() {
	++cur_generation;

	pool_mtx.lock();
	while (!submit.empty()) {
		VkCommandBuffer cmd = submit.back();
		u32 pool_index = submit_data.back();
		submit.pop();
		submit_data.pop();

		pools[pool_index].freelist.push(cmd);
	}
	pool_mtx.unlock();

	can_submit = true;
}
