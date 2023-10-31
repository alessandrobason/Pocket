// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vector>
#include <deque>
#include <functional>

#include <vk_types.h>


struct DeletionQueue {
	typedef void (*del_fn_f)(VkDevice, void *, const VkAllocationCallbacks *);
	struct DelObj {
		del_fn_f fn;
		void *obj;
	};

	//std::deque<std::function<void()>> deleters;
	std::deque<DelObj> deleters;

	template<typename T>
	void push(void (*fn)(VkDevice, T, const VkAllocationCallbacks *), T obj) {
		deleters.push_back({ (del_fn_f)fn, obj });
	}

	void flush(VkDevice device) {
		for (const auto &del : deleters) {
			del.fn(device, del.obj, nullptr);
		}
		deleters.clear();
	}
};

class VulkanEngine {
public:

	bool m_isInitialized{ false };
	int m_frameNumber {0};

	VkExtent2D m_windowExtent{ 800, 600 };

	struct SDL_Window *m_window{ nullptr };

	VkInstance m_instance = nullptr;
	VkDebugUtilsMessengerEXT m_debug_messenger = nullptr;
	VkPhysicalDevice m_chosen_gpu = nullptr;
	VkDevice m_device = nullptr;
	VkSurfaceKHR m_surface = nullptr;
	VkSwapchainKHR m_swapchain = nullptr;
	VkFormat m_swapchain_img_format;
	std::vector<VkImage> m_swapchain_images;
	std::vector<VkImageView> m_swapchain_img_views;

	VkQueue m_gfxqueue = nullptr;
	uint32_t m_gfxqueue_family = 0;

	VkCommandPool m_cmdpool;
	VkCommandBuffer m_main_cmdbuf;

	VkRenderPass m_render_pass;
	std::vector<VkFramebuffer> m_framebuffers;

	VkSemaphore m_present_sem;
	VkSemaphore m_render_sem;
	VkFence m_render_fence;

	VkPipelineLayout m_tri_pipeline_layout;
	VkPipeline m_tri_pipeline;

	DeletionQueue m_main_delete_queue;

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

private:
	void initVulkan();
	void initSwapChain();
	void initCommands();

	void initDefaultRenderPass();
	void initFramebuffers();
	void initSyncStructures();
	void initPipeline();

	VkShaderModule loadShaderModule(const char *path);
};

class PipelineBuilder {
public:
	std::vector<VkPipelineShaderStageCreateInfo> m_shader_stages;
	VkPipelineVertexInputStateCreateInfo m_vtx_input;
	VkPipelineInputAssemblyStateCreateInfo m_input_assembly;
	VkViewport m_viewport;
	VkRect2D m_scissor;
	VkPipelineRasterizationStateCreateInfo m_rasterizer;
	VkPipelineColorBlendAttachmentState m_colour_blend;
	VkPipelineMultisampleStateCreateInfo m_multisampling;
	VkPipelineLayout m_layout;

	VkPipeline build(VkDevice device, VkRenderPass pass);
};