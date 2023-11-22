#pragma once

#include "std/mem.h"
#include "vk_fwd.h"

extern VkDevice global_device;
extern VkInstance global_instance;
extern VmaAllocator global_alloc;

#define PK_DECLARE_VKPTR_BASE(T, fun, del)                  \
    template<>                                              \
    struct vkptr<T> {                                       \
        vkptr() = default;                                  \
        vkptr(T v) : value(v) {}                            \
        vkptr(vkptr &&other) { *this = mem::move(other); }  \
                                                            \
        ~vkptr() { destroy(); }                             \
                                                            \
        void destroy() {                                    \
            if (value) {                                    \
                del;                                        \
                value = nullptr;                            \
            }                                               \
        }                                                   \
                                                            \
        vkptr &operator=(vkptr &&other) {                   \
            mem::swap(value, other.value);                  \
            return *this;                                   \
        }                                                   \
                                                            \
        operator T() { return value; }                      \
        operator bool() { return value != nullptr; }        \
        T &get() { return value; }                          \
        T *getRef() { return &value; }                      \
                                                            \
        T value = nullptr;                                  \
    }

#define PK_DECLARE_VMAPTR(T, fun)                               \
    template<>                                                  \
    struct vkptr<T> {                                           \
        vkptr() = default;                                      \
        vkptr(T b, VmaAllocation a) : buffer(b), alloc(a) {}    \
        vkptr(vkptr &&other) { *this = mem::move(other); }      \
                                                                \
        ~vkptr() { destroy(); }                                 \
                                                                \
        void destroy() {                                        \
            if (buffer) {                                       \
                fun(global_alloc, buffer, alloc);               \
                buffer = nullptr;                               \
                alloc = nullptr;                                \
            }                                                   \
        }                                                       \
                                                                \
        vkptr &operator=(vkptr &&other) {                       \
            mem::swap(buffer, other.buffer);                    \
            mem::swap(alloc, other.alloc);                      \
            return *this;                                       \
        }                                                       \
                                                                \
        operator T() { return buffer; }                         \
        operator bool() { return buffer != nullptr; }           \
        T &get() { return buffer; }                             \
        T *getRef() { return &buffer; }                         \
                                                                \
        T buffer = nullptr;                                     \
        VmaAllocation alloc = nullptr;                          \
    }

#define PK_DECLARE_VKPTR_DEV(T, fun)  PK_DECLARE_VKPTR_BASE(T, fun, fun(global_device, value, nullptr))
#define PK_DECLARE_VKPTR_INST(T, fun) PK_DECLARE_VKPTR_BASE(T, fun, fun(global_instance, value, nullptr))

template<typename T>
struct vkptr {
    vkptr() = delete;
};

PK_DECLARE_VKPTR_INST(VkSurfaceKHR, vkDestroySurfaceKHR);

PK_DECLARE_VKPTR_DEV(VkImageView, vkDestroyImageView);
PK_DECLARE_VKPTR_DEV(VkSwapchainKHR, vkDestroySwapchainKHR);
PK_DECLARE_VKPTR_DEV(VkCommandPool, vkDestroyCommandPool);
PK_DECLARE_VKPTR_DEV(VkRenderPass, vkDestroyRenderPass);
PK_DECLARE_VKPTR_DEV(VkFramebuffer, vkDestroyFramebuffer);
PK_DECLARE_VKPTR_DEV(VkFence, vkDestroyFence);
PK_DECLARE_VKPTR_DEV(VkSemaphore, vkDestroySemaphore);
PK_DECLARE_VKPTR_DEV(VkPipelineLayout, vkDestroyPipelineLayout);
PK_DECLARE_VKPTR_DEV(VkShaderModule, vkDestroyShaderModule);
PK_DECLARE_VKPTR_DEV(VkDescriptorSetLayout, vkDestroyDescriptorSetLayout);
PK_DECLARE_VKPTR_DEV(VkDescriptorPool, vkDestroyDescriptorPool);
PK_DECLARE_VKPTR_DEV(VkSampler, vkDestroySampler);
PK_DECLARE_VKPTR_DEV(VkPipeline, vkDestroyPipeline);

PK_DECLARE_VKPTR_BASE(VkDevice, vkDestroyDevice, vkDestroyDevice(value, nullptr));
PK_DECLARE_VKPTR_BASE(VkInstance, vkDestroyInstance, vkDestroyInstance(value, nullptr));

PK_DECLARE_VKPTR_BASE(VmaAllocator, vmaDestroyAllocator, vmaDestroyAllocator(value));
PK_DECLARE_VMAPTR(VkImage, vmaDestroyImage);
PK_DECLARE_VMAPTR(VkBuffer, vmaDestroyBuffer);

namespace vkb { 
    void destroy_debug_utils_messenger(const VkInstance, const VkDebugUtilsMessengerEXT, VkAllocationCallbacks*);
}

PK_DECLARE_VKPTR_BASE(VkDebugUtilsMessengerEXT, vkb::destroy_debug_utils_messenger, vkb::destroy_debug_utils_messenger(global_instance, value, nullptr));
