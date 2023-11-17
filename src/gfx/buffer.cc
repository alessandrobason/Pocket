#include "buffer.h"

#include <stb_image.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "std/logging.h"
#include "formats/assets.h"
#include "engine.h"

static Image texture__upload(u32 width, u32 height, VkFormat format, Engine &engine, Buffer &staging_buf) {
    VkExtent3D image_extent = {
        .width = width,
        .height = height,
        .depth = 1,
    };

    VkImageCreateInfo img_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
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
        [&new_image, &staging_buf, &image_extent]
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
                staging_buf.buffer,
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

    engine.m_delete_queue.push(vmaDestroyImage, engine.m_allocator, new_image.image, new_image.allocation);
    vmaDestroyBuffer(engine.m_allocator, staging_buf.buffer, staging_buf.allocation);

    return new_image;
}

bool Texture::load(Engine &engine, const char *filename) {
    AssetFile file;
    if (!file.load(filename)) {
        err("failed to load asset file %s", filename);
        return false;
    }

    AssetTexture info = AssetTexture::readInfo(file);

    VkDeviceSize image_size = info.byte_size;
    VkFormat image_format;
    switch (info.format) {
        case AssetTexture::Rgba8:
            image_format = VK_FORMAT_R8G8B8A8_UNORM;
            break;
        default:
            err("unrecognised texture format: %u", info.format);
            return false;
    }

    Buffer staging_buf = engine.makeBuffer(info.byte_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void *data;
    vmaMapMemory(engine.m_allocator, staging_buf.allocation, &data);

    info.unpack(file.blob, (byte *)data);

    vmaUnmapMemory(engine.m_allocator, staging_buf.allocation);

    image = texture__upload(info.pixel_size[0], info.pixel_size[1], image_format, engine, staging_buf);

    return true;
}