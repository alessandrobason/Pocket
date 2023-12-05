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

static vkptr<VkImage> texture__upload(u32 width, u32 height, VkFormat format, Buffer &&staging_buf) {
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
    // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

    vkptr<VkImage> new_image;
    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    vmaCreateImage(g_engine->m_allocator, &img_info, &alloc_info, &new_image.buffer, &new_image.alloc, nullptr);

    VkCommandBuffer cmd = g_engine->getTransferCmd();
    pk_assert(cmd);

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

    // image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    // image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    // image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // vkCmdPipelineBarrier(
    //     cmd,
    //     VK_PIPELINE_STAGE_TRANSFER_BIT,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     0,
    //     0,
    //     nullptr,
    //     0,
    //     nullptr,
    //     1,
    //     &image_barrier
    // );

    u64 wait_handle = 0;

    info("submitting the command");

    // try submitting the command
    while (!(wait_handle = g_engine->trySubmitTransferCommand(cmd))) {
        co::yield();
    }

    info("waiting for transfer to finish");

    // wait for the transfer queue to be over
    while (!g_engine->isTransferFinished(wait_handle)) {
        co::yield();
    }

    info("uploaded!");

    staging_buf.destroy();

    return new_image;
}
    
static vkptr<VkImageView> texture__make_view(VkImage texture) {
	VkImageViewCreateInfo view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = texture,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
    vkptr<VkImageView> view;
    vkCreateImageView(g_engine->m_device, &view_info, nullptr, view.getRef());
    return view;
}

Handle<Texture> Texture::load(StrView filename) {
    Handle<Texture> handle = AssetManager::getNewHandle<Texture>();
    
    g_engine->jobpool.pushJob(
        [filename, handle]
        () {
            Texture texture;
            
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

            int req_comp = STBI_rgb_alpha;

            int width, height, comp;
            byte *data = stbi_load_from_memory(file_data.buf, (int)file_data.len, &width, &height, &comp, req_comp);

            if (!data) {
                err("couldn't load image %.*s: %s", filename.len, filename.buf, stbi_failure_reason());
                return;
            }

            Buffer staging = g_engine->makeBuffer(width * height * req_comp, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

            void *bufdata;
            vmaMapMemory(g_engine->m_allocator, staging.alloc, &bufdata);
            memcpy(bufdata, data, width * height * req_comp);
            vmaUnmapMemory(g_engine->m_allocator, staging.alloc);

            stbi_image_free(data);

            texture.image = texture__upload(width, height, VK_FORMAT_R8G8B8A8_UNORM, mem::move(staging));
	        texture.view = texture__make_view(texture.image);

            // AssetManager::finishLoading<Texture>(handle, mem::move(texture));
            AssetManager::finishLoading<Texture>(handle, mem::move(texture));
        }
    );

    return handle;
}
