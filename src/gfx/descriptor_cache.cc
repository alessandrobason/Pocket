#include "descriptor_cache.h"

#include "std/hash.h"

// == DESCRIPTOR ALLOCATOR ===================================================================================================================================

static VkDescriptorPool desc__create_pool(
    VkDevice device, 
    const DescriptorAllocator::PoolSizes &pool_sizes, 
    u32 count, 
    VkDescriptorPoolCreateFlags flags
) {
    arr<VkDescriptorPoolSize> sizes;
    sizes.reserve(pool_sizes.sizes.len);
    for (const auto &sz : pool_sizes.sizes) {
        sizes.push({ sz.first, u32(sz.second * count) });
    }

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = flags,
        .maxSets = count,
        .poolSizeCount = (u32)sizes.len,
        .pPoolSizes = sizes.buf,
    };

    VkDescriptorPool descriptor_pool;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool);

    return descriptor_pool;
}

void DescriptorAllocator::resetPools() {
    allocator_mtx.lock();

    // reset all used pools
    for (vkptr<VkDescriptorPool> &p : used_pools) {
        vkResetDescriptorPool(device, p, 0);
        free_pools.push(mem::move(p));
    }

    used_pools.clear();
    current_pool = nullptr;

    allocator_mtx.unlock();
}

bool DescriptorAllocator::allocate(VkDescriptorSet &set, VkDescriptorSetLayout layout) {
    allocator_mtx.lock();
    
    if (!current_pool) {
        current_pool = used_pools.push(grabPool());
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = current_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkResult alloc_result = vkAllocateDescriptorSets(device, &alloc_info, &set);
    bool need_realloc = false;

    switch (alloc_result) {
        case VK_SUCCESS:
            allocator_mtx.unlock();
            return true;
        case VK_ERROR_FRAGMENTED_POOL:
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            need_realloc = true;
            break;
        default:
            allocator_mtx.unlock();
            return false;
    }

    if (need_realloc) {
        current_pool = used_pools.push(grabPool());
        alloc_result = vkAllocateDescriptorSets(device, &alloc_info, &set);

        allocator_mtx.unlock();
        return alloc_result == VK_SUCCESS;
    }

    allocator_mtx.unlock();
    return false;
}

void DescriptorAllocator::init(VkDevice new_device) {
    device = new_device;
}

VkDescriptorPool DescriptorAllocator::grabPool() {
    if (!free_pools.empty()) {
        VkDescriptorPool pool = free_pools.back();
        free_pools.pop();
        return pool;
    }
    else {
        return desc__create_pool(device, descriptor_sizes, 1000, 0);
    }
}

// == DESCRIPTOR LAYOUT CACHE ================================================================================================================================

#include <algorithm> // std::sort
#include "utils/sort.h"

void DescriptorLayoutCache::init(VkDevice new_device) {
    device = new_device;
}

VkDescriptorSetLayout DescriptorLayoutCache::createDescLayout(const VkDescriptorSetLayoutCreateInfo &info) {
    DescriptorLayoutInfo layout_info;
    layout_info.bindings.reserve(info.bindingCount);
    bool is_sorted = true;
    u32 last_binding = -1;

    // copy from the info struct into our own one
    for (u32 i = 0; i < info.bindingCount; ++i) {
        layout_info.bindings.push(info.pBindings[i]);

        if (is_sorted && info.pBindings[i].binding > last_binding) {
            last_binding = info.pBindings[i].binding;
        }
        else {
            is_sorted = false;
        }
    }

    if (!is_sorted && !layout_info.bindings.empty()) {
        // radixSort(&layout_info.bindings[0].binding, (u32)layout_info.bindings.len, sizeof(VkDescriptorSetLayoutBinding));
        std::sort(layout_info.bindings.begin(), layout_info.bindings.end(), 
            [](const VkDescriptorSetLayoutBinding &a, const VkDescriptorSetLayoutBinding &b) { 
                return a.binding < b.binding; 
            }
        );
    }

    cache_mtx.lock();

    if (vkptr<VkDescriptorSetLayout> *it = layout_cache.get(layout_info)) {
        cache_mtx.unlock();
        return it->value;
    }
    else {
        VkDescriptorSetLayout layout;
        vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);

        layout_cache.push(layout_info, layout);
        cache_mtx.unlock();
        return layout; 
    }
}

bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo &other) const {
    if (other.bindings.size() != bindings.size()) return false;

    for (usize i = 0; i < bindings.size(); ++i) {
        const VkDescriptorSetLayoutBinding &a = bindings[i];
        const VkDescriptorSetLayoutBinding &b = other.bindings[i];
        if (a.binding != b.binding || 
            a.descriptorType != b.descriptorType || 
            a.stageFlags != b.stageFlags
        ) {
            return false;
        }
    }

    return true;
}

// == DESCRIPTOR BUILDER =====================================================================================================================================

DescriptorBuilder::DescriptorBuilder(DescriptorLayoutCache &c, DescriptorAllocator &a) 
    : cache(c), allocator(a)
{
}

DescriptorBuilder DescriptorBuilder::begin(DescriptorLayoutCache &layout_cache, DescriptorAllocator &layout_allocator) {
    return DescriptorBuilder{ layout_cache, layout_allocator };
}

DescriptorBuilder &DescriptorBuilder::bindBuffer(u32 binding, const VkDescriptorBufferInfo &info, VkDescriptorType type, VkShaderStageFlags flags) {
    bindings.push({
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
        .stageFlags = flags,
    });

    writes.push({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = type,
        .pBufferInfo = &info,
    });

    return *this;
}

DescriptorBuilder &DescriptorBuilder::bindImage(u32 binding, const VkDescriptorImageInfo &info, VkDescriptorType type, VkShaderStageFlags flags) {
    bindings.push({
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
        .stageFlags = flags,
    });

    writes.push({
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = binding,
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = &info,
    });

    return *this;
}

bool DescriptorBuilder::build(VkDescriptorSet &set, VkDescriptorSetLayout &layout) {
    layout = cache.createDescLayout({
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = (u32)bindings.len,
        .pBindings = bindings.buf,
    });

    if (!allocator.allocate(set, layout)) {
        return false;
    }

    for (VkWriteDescriptorSet &w : writes) {
        w.dstSet = set;
    }

    vkUpdateDescriptorSets(allocator.device, (u32)writes.len, writes.buf, 0, nullptr);

    return true;
}

bool DescriptorBuilder::build(VkDescriptorSet &set) {
    VkDescriptorSetLayout layout;
    return build(set, layout);
}

VkDescriptorSet DescriptorBuilder::build(VkDescriptorSetLayout &layout) {
    VkDescriptorSet set;
    
    layout = cache.createDescLayout({
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = (u32)bindings.len,
        .pBindings = bindings.buf,
    });

    if (!allocator.allocate(set, layout)) {
        return nullptr;
    }

    for (VkWriteDescriptorSet &w : writes) {
        w.dstSet = set;
    }

    vkUpdateDescriptorSets(allocator.device, (u32)writes.len, writes.buf, 0, nullptr);

    return set;
}

VkDescriptorSet DescriptorBuilder::build() {
    VkDescriptorSetLayout layout;
    return build(layout);
}

u32 hash_impl(const DescriptorLayoutCache::DescriptorLayoutInfo &v) {
    return hashFnv132(v.bindings.data(), v.bindings.byteSize());
}
