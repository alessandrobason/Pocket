#pragma once

#include "vk_fwd.h"
#include "vk_ptr.h"

// forward declarations 
struct Engine;

using Buffer = vkptr<VkBuffer>;
using Image = vkptr<VkImage>;

struct Texture {
	Image image;
	vkptr<VkImageView> view;

	bool load(Engine &engine, const char *filename);
};
