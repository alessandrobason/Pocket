#pragma once

// TODO remove this
#include <functional>
#include <glm/mat4x4.hpp>

#include "std/arr.h"
#include "std/str.h"
#include "std/slice.h"
#include "std/hashmap.h"
#include "std/vec.h"

// #include "utils/glm.h"

#include "vk_fwd.h"
#include "vk_ptr.h"
#include "buffer.h"
#include "mesh.h"
#include "camera.h"

// constants
constexpr uint kframe_overlap = 2;

// Forward declare SDL stuff
struct SDL_Window;

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

    Mesh *loadMesh(const char *asset_path, StrView name);
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

    void resizeWindow(int new_width, int new_height);

    void loadImages();
    // void uploadMesh(Mesh &mesh);

    void draw();
    void drawObjects(VkCommandBuffer cmd, Slice<RenderObject> objects);

    FrameData &getCurrentFrame();

    // -- types --
    struct FrameData {
        vkptr<VkSemaphore> present_sem;
        vkptr<VkSemaphore> render_sem;
        vkptr<VkFence> render_fence;
        vkptr<VkCommandPool> cmd_pool;
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
        vkptr<VkFence> fence;
        vkptr<VkCommandPool> pool;
        VkCommandBuffer buffer;
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
    u64 m_frame_num = 0;
    uint m_window_width = 800;
    uint m_window_height = 600;
    SDL_Window *m_window = nullptr;
    vkptr<VkInstance> m_instance;
	vkptr<VkDebugUtilsMessengerEXT> m_debug_messenger;
	VkPhysicalDevice m_chosen_gpu;
	vkptr<VkDevice> m_device;
	vkptr<VkSurfaceKHR> m_surface;
    vkptr<VkSwapchainKHR> m_swapchain;
	VkFormat m_swapchain_img_format;
	arr<VkImage> m_swapchain_images;
	arr<vkptr<VkImageView>> m_swapchain_img_views;
	VkFwd_PhysicalDeviceProperties m_gpu_properties;
	vkptr<VmaAllocator> m_allocator;
	vkptr<VkDescriptorPool> m_imgui_pool;

	VkQueue m_gfxqueue = nullptr;
	u32 m_gfxqueue_family = 0;

    FrameData m_frames[kframe_overlap];

	vkptr<VkRenderPass> m_render_pass;
	arr<vkptr<VkFramebuffer>> m_framebuffers;
    
	Image m_depth_img;
	vkptr<VkImageView> m_depth_view;
	VkFormat m_depth_format;

	vkptr<VkDescriptorSetLayout> m_global_set_layout;
	vkptr<VkDescriptorSetLayout> m_object_set_layout;
	vkptr<VkDescriptorSetLayout> m_single_texture_set_layout;
	vkptr<VkDescriptorPool> m_descriptor_pool;

    arr<vkptr<VkPipeline>> m_pipeline_cache;
    arr<vkptr<VkPipelineLayout>> m_pipeline_layout_cache;
    arr<vkptr<VkSampler>> m_sampler_cache;
    
	arr<RenderObject> m_drawable;
	HashMap<StrView, Material> m_materials;
	HashMap<StrView, Mesh> m_meshes;
	HashMap<StrView, Texture> m_textures;
    
	SceneData m_scene_params;
	Buffer m_scene_params_buf;

	UploadContext m_upload_ctx;

    Camera m_cam;
};