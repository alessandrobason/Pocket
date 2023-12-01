#pragma once

#include "std/arr.h"
#include "std/str.h"
#include "core/handle.h"
#include "gfx/vk_ptr.h"

#include "asset.h"

struct Texture : Asset {
    static Handle<Texture> load(StrView filename);

    vkptr<VkImage> image;
    vkptr<VkImageView> view;
};
