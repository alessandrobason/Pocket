#pragma once

#include "std/arr.h"
#include "std/str.h"
#include "core/handle.h"
#include "gfx/vk_ptr.h"

struct Texture {
    static Handle<Texture> load(StrView filename);

    vkptr<VkImage> image;
    vkptr<VkImageView> view;
};
