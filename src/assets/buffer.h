#pragma once

#include "gfx/vk_ptr.h"

#include "asset_manager.h"

struct Buffer {
    static Handle<Buffer> make(usize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);
    static Handle<Buffer> makeAsync();

    void allocate(usize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);

    void *map();
    template<typename T>
    T *map() {
        return (T *)map();
    }

    void unmap();

    vkptr<VkBuffer> value;
};
