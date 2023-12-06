#pragma once

#include "std/arr.h"
#include "gfx/vk_ptr.h"

#include "asset_manager.h"

struct AsyncDescBuilder;

struct Descriptor {
    static Handle<Descriptor> make(AsyncDescBuilder &builder);

    VkDescriptorSet set = nullptr;
};

struct AsyncDescBuilder {
    static AsyncDescBuilder begin();
    AsyncDescBuilder &bindImage(u32 slot, Handle<Texture> texture, VkSampler sampler, VkDescriptorType type, VkShaderStageFlags flags = 0);

    enum class BindType : u8 {
        Error, Texture, Buffer,
    };

    struct Binding {
        u32 slot;
        VkDescriptorType type;
        VkShaderStageFlags flags;
        Handle<Texture> texture;
        VkSampler sampler;
        BindType bind_type = BindType::Error;
        // Handle<Buffer> buffer ? Buffer &buffer?
    };

    arr<Binding> bindings;
};