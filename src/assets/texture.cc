#include "texture.h"

#include <stb_image.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "std/logging.h"
#include "std/file.h"
#include "std/asio.h"
#include "std/arr.h"
#include "gfx/engine.h"

#include "asset_manager.h"
#include "buffer.h"

static vkptr<VkImage> texture__upload(u32 width, u32 height, VkFormat format, Handle<Buffer> staging_buf) {
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
        .mipLevels = 3,
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
    
    Engine::AsyncQueue &queue = g_engine->async_transfer;

    VkCommandBuffer cmd = queue.getCmd();
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

    Buffer *staging = staging_buf.get();

    vkCmdCopyBufferToImage(
        cmd,
        staging->value,
        new_image.buffer,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copy_region
    );

    image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barrier.dstAccessMask = VK_IMAGE_LAYOUT_UNDEFINED;
    image_barrier.srcQueueFamilyIndex = g_engine->m_transferqueue_family;
    image_barrier.dstQueueFamilyIndex = g_engine->m_gfxqueue_family;

    image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &image_barrier
    );

    u64 wait_handle = 0;

    info("submitting the command");

    queue.waitUntilFinished(cmd);

    AssetManager::destroy(staging_buf);

    // change layout to shader read optimal

    Engine::AsyncQueue &gfx_queue = g_engine->getCurrentFrame().async_gfx;
    cmd = gfx_queue.getCmd();
    pk_assert(cmd);

    image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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

    gfx_queue.waitUntilFinished(cmd);

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
    Handle<Texture> handle = AssetManager::getNewTextureHandle();
    Str fname = filename;
    
    g_engine->jobpool.pushJob(
        [filename = mem::move(fname), handle]
        () {
            Texture texture;
            
            asio::File file;
            if (!file.init(filename)) {
                err("could not open texture file: %s", filename.cstr());
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
                err("couldn't load image %s: %s", filename.cstr(), stbi_failure_reason());
                return;
            }

            Handle<Buffer> staging = Buffer::make(width * height * req_comp, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
            Buffer *staging_buf = staging.get();

            void *bufdata = staging_buf->map();
            memcpy(bufdata, data, width * height * req_comp);
            staging_buf->unmap();

            stbi_image_free(data);

            texture.image = texture__upload(width, height, VK_FORMAT_R8G8B8A8_UNORM, mem::move(staging));
	        texture.view = texture__make_view(texture.image);

            AssetManager::finishLoading(handle, mem::move(texture));
        }
    );

    return handle;
}
