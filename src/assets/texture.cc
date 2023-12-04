#include "texture.h"

#include <stb_image.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "std/logging.h"
#include "std/file.h"
#include "std/asio.h"
#include "gfx/engine.h"

#include "asset_manager.h"

extern u32 am__push_texture(Texture &&tex);


static Image texture__upload(u32 width, u32 height, VkFormat format, Buffer &&staging_buf) {
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

    vmaCreateImage(g_engine->m_allocator, &img_info, &alloc_info, &new_image.buffer, &new_image.alloc, nullptr);

    g_engine->immediateSubmit(
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
                .image = new_image.buffer,
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
                new_image.buffer,
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

    // engine.m_delete_queue.push(vmaDestroyImage, engine.m_allocator, new_image.image, new_image.allocation);
    staging_buf.destroy();

    return new_image;
}
    
Handle<Texture> Texture::load(StrView filename) {
    u32 handle = AssetManager::getNewHandle<Texture>();
    Handle<Texture> out = handle;
    
    g_engine->jobpool.pushJob(
        [filename, handle] 
        () {
            mem::ptr<Texture> texture = (Texture *)pk_calloc(sizeof(Texture), 1);
            
            asio::File file;
            if (!file.init(filename)) {
                err("could not open texture file: %.*s", filename.len, filename.buf);
                return;
            }

            // wait for the file to be read
            do {
                co::yield();
            } while (!file.poll());

            arr<byte> file_data = file.getData();

            int width, height, comp;
            byte *data = stbi_load_from_memory(file_data.buf, (int)file_data.len, &width, &height, &comp, STBI_rgb_alpha);

            if (!data) {
                err("couldn't load image %.*s: %s", filename.len, filename.buf, stbi_failure_reason());
                return;
            }

            Buffer staging = g_engine->makeBuffer(width * height * comp, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

            void *bufdata;
            vmaMapMemory(g_engine->m_allocator, staging.alloc, &bufdata);
            memcpy(bufdata, data, width * height * comp);
            vmaUnmapMemory(g_engine->m_allocator, staging.alloc);

            stbi_image_free(data);

            texture->image = texture__upload(width, height, VK_FORMAT_R8G8B8A8_UNORM, mem::move(staging));
            // todo: view

            // AssetManager::finishLoading<Texture>(handle, mem::move(texture));
            AssetManager::finishLoading<Texture>(handle, texture.release());
        });

    return out;

    // Texture texture;
    
    // int width, height, comp;
    // fs::Path path = fs::getPath(filename);
    // byte *data = stbi_load(path.cstr(), &width, &height, &comp, STBI_rgb_alpha);

    // if (!data) {
    //     err("couldn't load image %.*s: %s", filename.len, filename.buf, stbi_failure_reason());
    //     return 0;
    // }

    // Buffer staging = g_engine->makeBuffer(width * height * comp, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    // void *bufdata;
    // vmaMapMemory(g_engine->m_allocator, staging.alloc, &bufdata);
    // memcpy(bufdata, data, width * height * comp);
    // vmaUnmapMemory(g_engine->m_allocator, staging.alloc);

    // stbi_image_free(data);

    // texture.image = texture__upload(width, height, VK_FORMAT_R8G8B8A8_UNORM, mem::move(staging));
    // // todo: view

    // return am__push_texture(mem::move(texture));
}
