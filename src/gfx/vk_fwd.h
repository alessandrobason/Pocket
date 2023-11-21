#pragma once

#include "std/common.h"

// Forward declare vulkan stuff
#define PK_VULKAN_HANDLE(object) typedef struct object##_T* object
#define PK_VULKAN_FUN(ret, name, ...) extern "C" ret __stdcall name(__VA_ARGS__)

// == Vulkan Memory Allocator ==============================================
enum VmaMemoryUsage;

PK_VULKAN_HANDLE(VmaAllocator);
PK_VULKAN_HANDLE(VmaAllocation);
PK_VULKAN_HANDLE(VkImage);
PK_VULKAN_HANDLE(VkBuffer);

extern "C" void vmaDestroyAllocator(VmaAllocator allocator);
extern "C" void vmaDestroyImage(VmaAllocator allocator, VkImage image, VmaAllocation allocation);
extern "C" void vmaDestroyBuffer(VmaAllocator allocator, VkBuffer image, VmaAllocation allocation);

// == Vulkan.h =============================================================
typedef u32 VkBufferUsageFlags;
typedef u32 VkPipelineVertexInputStateCreateFlags;
typedef u32 VkDebugUtilsMessageTypeFlagsEXT;

struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;
struct VkPhysicalDeviceProperties;
struct VkAllocationCallbacks;

enum VkFormat;
enum VkDebugUtilsMessageSeverityFlagBitsEXT;
enum VkPhysicalDeviceType;

PK_VULKAN_HANDLE(VkCommandBuffer);
PK_VULKAN_HANDLE(VkShaderModule);
PK_VULKAN_HANDLE(VkPipeline);
PK_VULKAN_HANDLE(VkPipelineLayout);
PK_VULKAN_HANDLE(VkDevice);
PK_VULKAN_HANDLE(VkInstance);
PK_VULKAN_HANDLE(VkDebugUtilsMessengerEXT);
PK_VULKAN_HANDLE(VkPhysicalDevice);
PK_VULKAN_HANDLE(VkSurfaceKHR);
PK_VULKAN_HANDLE(VkSwapchainKHR);
PK_VULKAN_HANDLE(VkImage);
PK_VULKAN_HANDLE(VkImageView);
PK_VULKAN_HANDLE(VkQueue);
PK_VULKAN_HANDLE(VkSemaphore);
PK_VULKAN_HANDLE(VkFence);
PK_VULKAN_HANDLE(VkCommandPool);
PK_VULKAN_HANDLE(VkDescriptorSet);
PK_VULKAN_HANDLE(VkBuffer);
PK_VULKAN_HANDLE(VkBuffer);
PK_VULKAN_HANDLE(VkRenderPass);
PK_VULKAN_HANDLE(VkFramebuffer);
PK_VULKAN_HANDLE(VkDescriptorSetLayout);
PK_VULKAN_HANDLE(VkDescriptorPool);
PK_VULKAN_HANDLE(VkSampler);

// VK_NO_PROTOTYPES

PK_VULKAN_FUN(void, vkDestroyDevice, VkDevice, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroyInstance, VkInstance, const VkAllocationCallbacks*);

PK_VULKAN_FUN(void, vkDestroySurfaceKHR, VkInstance, VkSurfaceKHR, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroyImageView, VkDevice, VkImageView, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroySwapchainKHR, VkDevice, VkSwapchainKHR, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroyCommandPool, VkDevice, VkCommandPool, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroyRenderPass, VkDevice, VkRenderPass, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroyFramebuffer, VkDevice, VkFramebuffer, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroyFence, VkDevice, VkFence, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroySemaphore, VkDevice, VkSemaphore, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroyPipelineLayout, VkDevice, VkPipelineLayout, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroyShaderModule, VkDevice, VkShaderModule, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroyDescriptorSetLayout, VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroyDescriptorPool, VkDevice, VkDescriptorPool, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroySampler, VkDevice, VkSampler, const VkAllocationCallbacks*);
PK_VULKAN_FUN(void, vkDestroyPipeline, VkDevice, VkPipeline, const VkAllocationCallbacks*);

constexpr uint vk_max_physical_device_name_size = 256u;
constexpr uint vk_uuid_size = 16u;

struct VkFwd_PhysicalDeviceSparseProperties {
    u32 residencyStandard2DBlockShape;
    u32 residencyStandard2DMultisampleBlockShape;
    u32 residencyStandard3DBlockShape;
    u32 residencyAlignedMipSize;
    u32 residencyNonResidentStrict;
};

struct VkFwd_PhysicalDeviceLimits {
    u32    maxImageDimension1D;
    u32    maxImageDimension2D;
    u32    maxImageDimension3D;
    u32    maxImageDimensionCube;
    u32    maxImageArrayLayers;
    u32    maxTexelBufferElements;
    u32    maxUniformBufferRange;
    u32    maxStorageBufferRange;
    u32    maxPushConstantsSize;
    u32    maxMemoryAllocationCount;
    u32    maxSamplerAllocationCount;
    u64    bufferImageGranularity;
    u64    sparseAddressSpaceSize;
    u32    maxBoundDescriptorSets;
    u32    maxPerStageDescriptorSamplers;
    u32    maxPerStageDescriptorUniformBuffers;
    u32    maxPerStageDescriptorStorageBuffers;
    u32    maxPerStageDescriptorSampledImages;
    u32    maxPerStageDescriptorStorageImages;
    u32    maxPerStageDescriptorInputAttachments;
    u32    maxPerStageResources;
    u32    maxDescriptorSetSamplers;
    u32    maxDescriptorSetUniformBuffers;
    u32    maxDescriptorSetUniformBuffersDynamic;
    u32    maxDescriptorSetStorageBuffers;
    u32    maxDescriptorSetStorageBuffersDynamic;
    u32    maxDescriptorSetSampledImages;
    u32    maxDescriptorSetStorageImages;
    u32    maxDescriptorSetInputAttachments;
    u32    maxVertexInputAttributes;
    u32    maxVertexInputBindings;
    u32    maxVertexInputAttributeOffset;
    u32    maxVertexInputBindingStride;
    u32    maxVertexOutputComponents;
    u32    maxTessellationGenerationLevel;
    u32    maxTessellationPatchSize;
    u32    maxTessellationControlPerVertexInputComponents;
    u32    maxTessellationControlPerVertexOutputComponents;
    u32    maxTessellationControlPerPatchOutputComponents;
    u32    maxTessellationControlTotalOutputComponents;
    u32    maxTessellationEvaluationInputComponents;
    u32    maxTessellationEvaluationOutputComponents;
    u32    maxGeometryShaderInvocations;
    u32    maxGeometryInputComponents;
    u32    maxGeometryOutputComponents;
    u32    maxGeometryOutputVertices;
    u32    maxGeometryTotalOutputComponents;
    u32    maxFragmentInputComponents;
    u32    maxFragmentOutputAttachments;
    u32    maxFragmentDualSrcAttachments;
    u32    maxFragmentCombinedOutputResources;
    u32    maxComputeSharedMemorySize;
    u32    maxComputeWorkGroupCount[3];
    u32    maxComputeWorkGroupInvocations;
    u32    maxComputeWorkGroupSize[3];
    u32    subPixelPrecisionBits;
    u32    subTexelPrecisionBits;
    u32    mipmapPrecisionBits;
    u32    maxDrawIndexedIndexValue;
    u32    maxDrawIndirectCount;
    float  maxSamplerLodBias;
    float  maxSamplerAnisotropy;
    u32    maxViewports;
    u32    maxViewportDimensions[2];
    float  viewportBoundsRange[2];
    u32    viewportSubPixelBits;
    usize  minMemoryMapAlignment;
    u64    minTexelBufferOffsetAlignment;
    u64    minUniformBufferOffsetAlignment;
    u64    minStorageBufferOffsetAlignment;
    i32    minTexelOffset;
    u32    maxTexelOffset;
    i32    minTexelGatherOffset;
    u32    maxTexelGatherOffset;
    float  minInterpolationOffset;
    float  maxInterpolationOffset;
    u32    subPixelInterpolationOffsetBits;
    u32    maxFramebufferWidth;
    u32    maxFramebufferHeight;
    u32    maxFramebufferLayers;
    u32    framebufferColorSampleCounts;
    u32    framebufferDepthSampleCounts;
    u32    framebufferStencilSampleCounts;
    u32    framebufferNoAttachmentsSampleCounts;
    u32    maxColorAttachments;
    u32    sampledImageColorSampleCounts;
    u32    sampledImageIntegerSampleCounts;
    u32    sampledImageDepthSampleCounts;
    u32    sampledImageStencilSampleCounts;
    u32    storageImageSampleCounts;
    u32    maxSampleMaskWords;
    u32    timestampComputeAndGraphics;
    float  timestampPeriod;
    u32    maxClipDistances;
    u32    maxCullDistances;
    u32    maxCombinedClipAndCullDistances;
    u32    discreteQueuePriorities;
    float  pointSizeRange[2];
    float  lineWidthRange[2];
    float  pointSizeGranularity;
    float  lineWidthGranularity;
    u32    strictLines;
    u32    standardSampleLocations;
    u64    optimalBufferCopyOffsetAlignment;
    u64    optimalBufferCopyRowPitchAlignment;
    u64    nonCoherentAtomSize;
};

struct VkFwd_PhysicalDeviceProperties {
    u32                                  apiVersion;
    u32                                  driverVersion;
    u32                                  vendorID;
    u32                                  deviceID;
    VkPhysicalDeviceType                 deviceType;
    char                                 deviceName[vk_max_physical_device_name_size];
    u8                                   pipelineCacheUUID[vk_uuid_size];
    VkFwd_PhysicalDeviceLimits           limits;
    VkFwd_PhysicalDeviceSparseProperties sparseProperties;
};

#undef VK_MAX_PHYSICAL_DEVICE_NAME_SIZE

#undef PK_VULKAN_HANDLE
