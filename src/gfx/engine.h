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

#include "utils/tracy_helper.h"

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
    void drawFpsWidget();

    FrameData &getCurrentFrame();

    // -- types --

    struct AsyncQueue {
        void init(VkQueue in_queue, u32 in_family, bool use_fence = false);
        VkCommandBuffer getCmd();
        u64 trySubmitCmd(VkCommandBuffer cmd);
        bool isFinished(u64 wait_handle);
        void waitUntilFinished(VkCommandBuffer cmd);

        void update(VkCommandBuffer cmd);
        void updateWithFence();

        VkCommandBuffer allocCmd();
        void resetSubmitList();

        struct PoolData {
            vkptr<VkCommandPool> pool;
            uptr thread_id;
            arr<VkCommandBuffer> freelist;
        };

        void addPool(uptr thread_id);
        u32 getPoolIndex();

        VkQueue queue;
        u32 family;
        arr<PoolData> pools;
        Mutex pool_mtx;
        // vkptr<VkCommandPool> pool;
        // Mutex freelist_mtx;
        arr<VkCommandBuffer> submit;
        arr<u32> submit_data;
        Mutex submit_mtx;
        std::atomic<bool> can_submit = true;
        std::atomic<u64> cur_generation = 1;

        // optional if using own cmdbuf
        VkCommandBuffer cmdbuf;
        vkptr<VkFence> fence;
    };

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
        AsyncQueue async_gfx;
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

    double frame_time = 0.0;

    std::atomic<u64> m_frame_num = 0;
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

    Tracy tracy_helper;

    // GRAPHICS QUEUE ///////////////////////////////////////////////

	VkQueue m_gfxqueue = nullptr;
	u32 m_gfxqueue_family = 0;

    // TRANSFER STUFF ///////////////////////////////////////////////

    VkQueue m_transferqueue = nullptr;
    u32 m_transferqueue_family = 0;
    AsyncQueue async_transfer;

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