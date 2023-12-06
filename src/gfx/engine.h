#pragma once

// TODO remove this
// #include <functional>
#include <atomic>
#include <glm/mat4x4.hpp>

#include "std/arr.h"
#include "std/str.h"
#include "std/slice.h"
#include "std/hashmap.h"
#include "std/vec.h"
#include "std/delegate.h"

#include "core/thread_pool.h"

// #include "utils/glm.h"

#include "vk_fwd.h"
#include "vk_ptr.h"
#include "mesh.h"
#include "camera.h"
#include "descriptor_cache.h"

// constants
constexpr uint kframe_overlap = 2;

// Forward declare SDL stuff
struct SDL_Window;

struct Engine;
extern Engine *g_engine;

struct Engine {
    // forward declare
    struct RenderObject;
    struct FrameData;

    void init();
    void cleanup();

    void run();

    // void immediateSubmit(std::function<void(VkCommandBuffer)> &&fun);
    void immediateSubmit(Delegate<void(VkCommandBuffer)> &&fun);

    VkShaderModule loadShaderModule(const char *path);

    Material *makeMaterial(VkPipeline pipeline, VkPipelineLayout layout, StrView name);
    Material *getMaterial(StrView name);

    Mesh *loadMesh(const char *asset_path, StrView name);
    Mesh *getMesh(StrView name);

    // Buffer makeBuffer(usize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);

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

    void resizeWindow(int new_width, int new_height);

    void loadImages();
    // void uploadMesh(Mesh &mesh);

    void draw();
    void drawObjects(VkCommandBuffer cmd, Slice<RenderObject> objects);

    FrameData &getCurrentFrame();

    VkCommandBuffer getTransferCmd();
    u64 trySubmitTransferCommand(VkCommandBuffer cmd);
    bool isTransferFinished(u64 handle);

    void transferUpdate();

    VkCommandBuffer allocateTransferCommandBuf();

    // -- types --
    struct FrameData {
        vkptr<VkSemaphore> present_sem;
        vkptr<VkSemaphore> render_sem;
        vkptr<VkFence> render_fence;
        vkptr<VkCommandPool> cmd_pool;
        VkCommandBuffer cmd_buf;
        Handle<Buffer> camera_buf;
        VkDescriptorSet global_descriptor;
        Handle<Buffer> object_buf;
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
        vkptr<VkFence> fence;
        vkptr<VkCommandPool> pool;
        VkCommandBuffer buffer;
        Mutex mtx;
    };

    struct RenderObject {
        Mesh *mesh;
        Material *material;
        glm::mat4 matrix = glm::mat4(1);
    };

    struct ObjectData {
        glm::mat4 model;
    };

    // -- variables --
    vkptr<VkInstance> m_instance;
    vkptr<VkDevice> m_device;
    vkptr<VkSurfaceKHR> m_surface;
    vkptr<VkSwapchainKHR> m_swapchain;
    vkptr<VkDebugUtilsMessengerEXT> m_debug_messenger;
    vkptr<VmaAllocator> m_allocator;

    u64 m_frame_num = 0;
    uint m_window_width = 800;
    uint m_window_height = 600;
    SDL_Window *m_window = nullptr;

	VkPhysicalDevice m_chosen_gpu;
    VkFwd_PhysicalDeviceProperties m_gpu_properties;
	VkFormat m_swapchain_img_format;
	arr<VkImage> m_swapchain_images;
	arr<vkptr<VkImageView>> m_swapchain_img_views;

	vkptr<VkDescriptorPool> m_imgui_pool;

    ThreadPool jobpool;

    DescriptorLayoutCache m_desc_cache;
    DescriptorAllocator m_desc_alloc;

	VkQueue m_gfxqueue = nullptr;
	u32 m_gfxqueue_family = 0;

    // TRANSFER STUFF ///////////////////////////////////////////////

    VkQueue m_transferqueue = nullptr;
    u32 m_transferqueue_family = 0;
    vkptr<VkCommandPool> m_transfer_pool;
    VkCommandBuffer transfer_cmd;
    Mutex transfer_pool_mtx;
    
    vkptr<VkFence> transfer_fence;

    arr<VkCommandBuffer> transf_cmd_freelist;
    arr<VkCommandBuffer> transf_submit;
    Mutex transf_freelist_mtx;
    Mutex transf_submit_mtx;

    std::atomic<u64> cur_fence_gen = 1;
    bool has_submitted = false;

    /////////////////////////////////////////////////////////////////
    
    FrameData m_frames[kframe_overlap];

	vkptr<VkRenderPass> m_render_pass;
	arr<vkptr<VkFramebuffer>> m_framebuffers;
    
	vkptr<VkImage> m_depth_img;
	vkptr<VkImageView> m_depth_view;
	VkFormat m_depth_format;

	VkDescriptorSetLayout m_global_set_layout;
	VkDescriptorSetLayout m_object_set_layout;
	VkDescriptorSetLayout m_single_texture_set_layout;
	vkptr<VkDescriptorPool> m_descriptor_pool;

    arr<vkptr<VkPipeline>> m_pipeline_cache;
    arr<vkptr<VkPipelineLayout>> m_pipeline_layout_cache;
    arr<vkptr<VkSampler>> m_sampler_cache;
    
	arr<RenderObject> m_drawable;
	HashMap<StrView, Material> m_materials;
	HashMap<StrView, Mesh> m_meshes;
    Material *default_material = nullptr;
	//HashMap<StrView, Texture> m_textures;
    
	SceneData m_scene_params;
	Handle<Buffer> m_scene_params_buf;

	UploadContext m_upload_ctx;

    Camera m_cam;
};