#pragma once

// TODO remove this
#include <functional>

#include "std/arr.h"
#include "std/str.h"
#include "std/slice.h"
#include "std/hashmap.h"

#include "utils/glm.h"

#include "vk_fwd.h"
#include "buffer.h"
#include "mesh.h"

// constants
constexpr uint kframe_overlap = 2;

// Forward declare SDL stuff
struct SDL_Window;

// stupid simple deletion queue, probably need something better
struct DeletionQueue {
	arr<std::function<void()>> m_deleters;

	template<typename Fn, typename ...Targs>
	void push(Fn &&fn, Targs &&...args) {
		m_deleters.push(std::bind(fn, args...));
	}

	void flush();
};

struct Engine {
    // forward declare
    struct RenderObject;
    struct FrameData;

    void init();
    void cleanup();

    void run();

    void immediateSubmit(std::function<void(VkCommandBuffer)> &&fun);

    VkShaderModule loadShaderModule(const char *path);

    Material *makeMaterial(VkPipeline pipeline, VkPipelineLayout layout, StrView name);
    Material *getMaterial(StrView name);

    Mesh *loadMesh(const char *obj_path, StrView name);
    Mesh *getMesh(StrView name);

    Buffer makeBuffer(usize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);

    usize padUniformBufferSize(usize size) const;

    void initGfx();
    void initSwapchain();
    void initCommandBuffers();
    void initDefaultRenderPass();
    void initFrameBuffers();
    void initSyncStructures();
    void initPipeline();
    void initDescriptors();

    void initScene();
    void initImGui();

    void loadImages();
    void uploadMesh(Mesh &mesh);

    void draw();
    void drawObjects(VkCommandBuffer cmd, Slice<RenderObject> objects);

    FrameData &getCurrentFrame();

    // -- types --
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

    struct SceneData {
	    vec4 fog_colour;
	    vec4 fog_distances;
	    vec4 ambient_colour;
	    vec4 sunlight_dir;
	    vec4 sunlight_colour;
    };

    struct UploadContext {
        VkFence fence;
        VkCommandPool pool;
        VkCommandBuffer buffer;
    };

    struct RenderObject {
        Mesh *mesh;
        Material *material;
        mat4 matrix = mat4(1);
    };

    struct ObjectData {
        mat4 model;
    };

    // -- variables --
    u64 m_frame_num = 0;
    uint m_window_width = 800;
    uint m_window_height = 600;
    SDL_Window *m_window = nullptr;
    VkInstance m_instance = nullptr;
	VkDebugUtilsMessengerEXT m_debug_messenger = nullptr;
	VkPhysicalDevice m_chosen_gpu = nullptr;
	VkDevice m_device = nullptr;
	VkSurfaceKHR m_surface = nullptr;
	VkSwapchainKHR m_swapchain = nullptr;
	VkFormat m_swapchain_img_format;
	arr<VkImage> m_swapchain_images;
	arr<VkImageView> m_swapchain_img_views;
	VkFwd_PhysicalDeviceProperties m_gpu_properties;
	VmaAllocator m_allocator;
	DeletionQueue m_delete_queue;

	VkQueue m_gfxqueue = nullptr;
	u32 m_gfxqueue_family = 0;

    FrameData m_frames[kframe_overlap];

	VkRenderPass m_render_pass;
	arr<VkFramebuffer> m_framebuffers;
    
	Image m_depth_img;
	VkImageView m_depth_view;
	VkFormat m_depth_format;

	VkDescriptorSetLayout m_global_set_layout;
	VkDescriptorSetLayout m_object_set_layout;
	VkDescriptorSetLayout m_single_texture_set_layout;
	VkDescriptorPool m_descriptor_pool;
    
	arr<RenderObject> m_drawable;
	HashMap<StrView, Material> m_materials;
	HashMap<StrView, Mesh> m_meshes;
	HashMap<StrView, Texture> m_textures;
    
	SceneData m_scene_params;
	Buffer m_scene_params_buf;

	UploadContext m_upload_ctx;
};