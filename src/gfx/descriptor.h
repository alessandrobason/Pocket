#pragma once

#include <vulkan/vulkan.h>

#include "std/arr.h"
#include "std/hashmap.h"
#include "std/pair.h"

#include "vk_ptr.h"

struct DescriptorAllocator {
    struct PoolSizes {
        arr<pair<VkDescriptorType, float>> sizes = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f }
        };
    };

    void resetPools();
    bool allocate(VkDescriptorSet &set, VkDescriptorSetLayout layout);

    void init(VkDevice new_device);

    VkDevice device;

private:
    VkDescriptorPool grabPool();   

    VkDescriptorPool current_pool = nullptr;
    PoolSizes descriptor_sizes;
    arr<vkptr<VkDescriptorPool>> used_pools;
    arr<vkptr<VkDescriptorPool>> free_pools;
};

struct DescriptorLayoutCache {
    void init(VkDevice new_device);

    VkDescriptorSetLayout createDescLayout(const VkDescriptorSetLayoutCreateInfo &info);

    struct DescriptorLayoutInfo {
        arr<VkDescriptorSetLayoutBinding> bindings;

        bool operator==(const DescriptorLayoutInfo &other) const;

        u32 hash() const;
    };

private:
    // TODO use actual hashmap
    HashMap<DescriptorLayoutInfo, vkptr<VkDescriptorSetLayout>> layout_cache;
    VkDevice device;
};

struct DescriptorBuilder {
    DescriptorBuilder(DescriptorLayoutCache &c, DescriptorAllocator &a);

    static DescriptorBuilder begin(DescriptorLayoutCache &layout_cache, DescriptorAllocator &layout_allocator);

    DescriptorBuilder &bindBuffer(u32 binding, const VkDescriptorBufferInfo &info, VkDescriptorType type, VkShaderStageFlags flags = 0);
    DescriptorBuilder &bindImage(u32 binding, const VkDescriptorImageInfo &info, VkDescriptorType type, VkShaderStageFlags flags = 0);

    bool build(VkDescriptorSet &set, VkDescriptorSetLayout &layout);
    bool build(VkDescriptorSet &set);

    VkDescriptorSet build(VkDescriptorSetLayout &layout);
    VkDescriptorSet build();

private:
    arr<VkWriteDescriptorSet> writes;
    arr<VkDescriptorSetLayoutBinding> bindings;

    DescriptorLayoutCache &cache;
    DescriptorAllocator &allocator;
};