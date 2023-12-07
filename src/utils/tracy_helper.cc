#include "tracy_helper.h"

#include <vulkan/vulkan.h>
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

#include "gfx/engine.h"

void Tracy::init() {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    for (usize i = 0; i < kframe_overlap; ++i) {
        auto &frame = g_engine->m_frames[i];
        alloc_info.commandPool = frame.cmd_pool;

        //VkCommandBuffer cmdbuf;
        //vkAllocateCommandBuffers(g_engine->m_device, &alloc_info, &cmdbuf);

        // TracyVkCtx ctx = TracyVkContext(g_engine->m_chosen_gpu, g_engine->m_device, g_engine->m_gfxqueue, cmdbuf);
        TracyVkCtx ctx = TracyVkContext(g_engine->m_chosen_gpu, g_engine->m_device, g_engine->m_gfxqueue, frame.cmd_buf);
        Str name = Str::fmt("Frame %zu", i);
        TracyVkContextName(ctx, name.cstr(), (u16)name.size());
        ctxs.push(ctx);
    }
}

void *Tracy::getCtx(u32 frame) {
    return ctxs[frame % kframe_overlap];
}

void Tracy::cleanup() {
    for (void *ctx : ctxs) {
        TracyVkDestroy((tracy::VkCtx *)ctx);
    }
}