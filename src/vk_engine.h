// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <unordered_map>

#include <vk_mem_alloc.h>
#include <glm/glm.hpp>

#include "vk_types.h"
#include "mesh.h"

constexpr uint64_t alignTo(uint64_t value, uint64_t alignment) {
	return (value + alignment - 1) & ~(alignment - 1);
}

struct DeletionQueue {
	std::deque<std::function<void()>> deleters;

	template<typename Fn, typename ...Targs>
	void push(Fn &&fn, Targs &&...args) {
		deleters.emplace_back(std::bind(fn, args...));
	}

	void flush(VkDevice device) {
		for (const auto &del : deleters) {
			del();
		}
		deleters.clear();
	}
};

struct MeshPushConstants {
	glm::vec4 data;
	glm::mat4 render_matrix;
};

struct Material {
	VkPipeline pipeline;
	VkPipelineLayout layout;
	VkDescriptorSet texture_set = VK_NULL_HANDLE;
};

struct RenderObject {
	Mesh *mesh;
	Material *material;
	glm::mat4 matrix = glm::mat4(1);
};

struct GPUCameraData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
};

struct GPUSceneData {
	glm::vec4 fog_colour;
	glm::vec4 fog_distances;
	glm::vec4 ambient_colour;
	glm::vec4 sunlight_dir;
	glm::vec4 sunlight_colour;
};

struct FrameData {
	VkSemaphore present_sem;
	VkSemaphore render_sem;
	VkFence render_fence;
	VkCommandPool cmd_pool;
	VkCommandBuffer cmd_buf;
	Buffer camera_buf;
	VkDescriptorSet global_descriptor;
	Buffer object_buf;
	VkDescriptorSet object_descriptor;
};

struct GPUObjectData {
	glm::mat4 model;
};

struct UploadContext {
	VkFence fence;
	VkCommandPool pool;
	VkCommandBuffer buffer;
};

struct Texture {
	Image image;
	VkImageView view;
};

constexpr unsigned int frame_overlap = 2;

class VulkanEngine {
public:

	bool m_isInitialized{ false };
	uint64_t m_frameNumber {0};

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
	VkPhysicalDeviceProperties m_gpu_properties;

	VkQueue m_gfxqueue = nullptr;
	uint32_t m_gfxqueue_family = 0;

	FrameData m_frames[frame_overlap];

	VkRenderPass m_render_pass;
	std::vector<VkFramebuffer> m_framebuffers;

	DeletionQueue m_main_delete_queue;

	VmaAllocator m_allocator;

	Image m_depth_img;
	VkImageView m_depth_view;
	VkFormat m_depth_format;

	VkDescriptorSetLayout m_global_set_layout;
	VkDescriptorSetLayout m_object_set_layout;
	VkDescriptorSetLayout m_single_texture_set_layout;
	VkDescriptorPool m_descriptor_pool;

	std::vector<RenderObject> m_drawable;
	std::unordered_map<std::string, Material> m_materials;
	std::unordered_map<std::string, Mesh> m_meshes;

	GPUSceneData m_scene_params;
	Buffer m_scene_params_buf;

	UploadContext m_upload_ctx;

	std::unordered_map<std::string, Texture> m_textures;

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	void initVulkan();
	void initSwapChain();
	void initCommands();

	void initDefaultRenderPass();
	void initFramebuffers();
	void initSyncStructures();
	void initPipeline();

	void initScene();

	VkShaderModule loadShaderModule(const char *path);
	void loadMeshes();
	void uploadMesh(Mesh &mesh);

	Material *makeMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string &name);
	Material *getMaterial(const std::string &name);
	Mesh *getMesh(const std::string &name);

	void drawObjects(VkCommandBuffer cmd, RenderObject *first, int count);

	FrameData &getFrame();

	Buffer makeBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);
	void initDescriptors();

	size_t padUniformBufferSize(size_t size);

	void writeGpuMemory(Buffer &buffer, void *data, size_t len);

	template<typename T>
	void writeGpuMemory(Buffer &buffer, T *data, size_t count = 1) {
		writeGpuMemory(buffer, (void *)data, sizeof(T) * count);
	}

	void immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&fun);

	void loadImages();
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
	VkPipelineDepthStencilStateCreateInfo m_depth_stencil;

	PipelineBuilder &pushShader(VkShaderStageFlagBits stage, VkShaderModule shader, const char *entry = "main");
	PipelineBuilder &setVertexInput(const VertexInDesc &vtx_desc);
	PipelineBuilder &setVertexInput(const VkVertexInputBindingDescription *bindings, u32 bcount, const VkVertexInputAttributeDescription *attributes, u32 acount);
	PipelineBuilder &setInputAssembly(VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	PipelineBuilder &setViewport(float x, float y, float w, float h, float min_depth = 0, float max_depth = 1);
	PipelineBuilder &setScissor(VkExtent2D  extent, VkOffset2D offset = { 0, 0 });
	PipelineBuilder &setRasterizer(VkCullModeFlags cull_mode, VkFrontFace front_face = VK_FRONT_FACE_CLOCKWISE, VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL);
	PipelineBuilder &setColourBlend(VkColorComponentFlags flags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
	PipelineBuilder &setMultisampling(VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
	PipelineBuilder &setLayout(VkPipelineLayout layout);
	PipelineBuilder &setDepthStencil(VkCompareOp operation = VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipeline build(VkDevice device, VkRenderPass pass);
};