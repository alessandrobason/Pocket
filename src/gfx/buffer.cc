#include "buffer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "std/logging.h"
#include "engine.h"

bool Texture::load(Engine &engine, const char *filename) {
    int w, h, c;
    stbi_uc *pixels = stbi_load(filename, &w, &h, &c, STBI_rgb_alpha);
    
    if (!pixels) {
        err("failed to load image %s", filename);
        return false;
    }

    VkDeviceSize image_size = w * h * 4;
    VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

    Buffer staging = engine.makeBuffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void *gpu_data;
	vmaMapMemory(engine.m_allocator, staging.allocation, &gpu_data);
	memcpy(gpu_data, pixels, image_size);
	vmaUnmapMemory(engine.m_allocator, staging.allocation);

    stbi_image_free(pixels);

    VkExtent3D image_extent = {
        .width = (u32)w,
        .height = (u32)h,
        .depth = 1,
    };

    VkImageCreateInfo img_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = image_format,
        .extent = image_extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };

    Image new_image;
    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    vmaCreateImage(engine.m_allocator, &img_info, &alloc_info, &new_image.image, &new_image.allocation, nullptr);

    engine.immediateSubmit(
        [&new_image, &staging, &image_extent]
        (VkCommandBuffer cmd) {
            VkImageSubresourceRange range = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            };

            VkImageMemoryBarrier image_barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .image = new_image.image,
                .subresourceRange = range,
            };

            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &image_barrier
            );

            VkBufferImageCopy copy_region = {
                .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
                },
                .imageExtent = image_extent,
            };

            vkCmdCopyBufferToImage(
                cmd,
                staging.buffer,
                new_image.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copy_region
            );

            image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &image_barrier
            );
        }
    );

    image = new_image;

    engine.m_delete_queue.push(vmaDestroyImage, engine.m_allocator, image.image, image.allocation);
    vmaDestroyBuffer(engine.m_allocator, staging.buffer, staging.allocation);

    return true;
}