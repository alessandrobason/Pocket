#include "buffer.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "gfx/engine.h"

Handle<Buffer> Buffer::make(usize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage) {
    Handle<Buffer> handle = AssetManager::getNewBufferHandle();

    Buffer out;
	out.allocate(size, usage, memory_usage);

    AssetManager::finishLoading(handle, mem::move(out));
	return handle;
}

Handle<Buffer> Buffer::makeAsync() {
	return AssetManager::getNewBufferHandle();
}

void Buffer::allocate(usize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage) {
	VkBufferCreateInfo buf_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage
	};

	VmaAllocationCreateInfo alloc_info = {
		.usage = memory_usage,
	};

	vmaCreateBuffer(
		g_engine->m_allocator,
		&buf_info,
		&alloc_info,
		&value.buffer,
		&value.alloc,
		nullptr
	);
}

void *Buffer::map() {
    void *data = nullptr;
    vmaMapMemory(g_engine->m_allocator, value.alloc, &data);
    return data;
}

void Buffer::unmap() {
    vmaUnmapMemory(g_engine->m_allocator, value.alloc);
}
