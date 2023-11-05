// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <stdint.h>
#include <stddef.h>

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using usize = size_t;
using isize = ptrdiff_t;

struct Buffer {
	VkBuffer buffer;
	VmaAllocation allocation;
};

struct Image {
	VkImage image;
	VmaAllocation allocation;
};

#define arrlen(a) (sizeof(a) / sizeof(*(a)))