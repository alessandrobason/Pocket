// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

class VulkanEngine;

namespace vkutil {
	bool loadImage(VulkanEngine &engine, const char *fname, Image &out_image);
}
