#pragma once

#include "vk_fwd.h"

// forward declarations 
struct Engine;

struct Buffer {
	VkBuffer buffer;
	VmaAllocation allocation;
};

struct Image {
	VkImage image;
	VmaAllocation allocation;
};

struct Texture {
	Image image;
	VkImageView view;

	bool load(Engine &engine, const char *filename);
};