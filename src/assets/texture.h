#pragma once

#include "std/str.h"
#include "gfx/vk_ptr.h"

#include "asset_manager.h"

struct Texture {
    static Handle<Texture> load(StrView filename);

    vkptr<VkImage> image;
    vkptr<VkImageView> view;
};
