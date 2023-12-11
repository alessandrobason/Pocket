#pragma once

#include "mesh.h"

// TODO remove tiny_obj_loader
#include <vector>
#include <string>
#include <tiny_obj_loader.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "std/common.h"
#include "std/logging.h"
#include "std/asio.h"

#include "formats/assets.h"
#include "engine.h"

VertexInDesc Vertex::getVertexDesc() {
	VertexInDesc desc;

	desc.bindings.push(VkVertexInputBindingDescription{
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	});

	desc.attributes.push(VkVertexInputAttributeDescription{
		.location = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, pos)
	});

	desc.attributes.push(VkVertexInputAttributeDescription{
		.location = 1,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, norm)
	});

	desc.attributes.push(VkVertexInputAttributeDescription{
		.location = 2,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = offsetof(Vertex, uv)
	});

	desc.attributes.push(VkVertexInputAttributeDescription{
		.location = 3,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.offset = offsetof(Vertex, col)
	});

	return desc;
}

bool Mesh::loadFromObj(const char *fname) {
	return true;
}

#define PK_VKCHECK(x) \
    do { \
        VkResult err = x; \
        if(err) { \
            fatal("%s: Vulkan error: %d", #x, err);  \
        } \
    } while (0)

void mesh__upload(Handle<Buffer> out, VkBufferUsageFlagBits vert_or_ind, const void *data, usize size) {
	g_engine->jobpool.pushJob(
		[out, data, size, vert_or_ind]
		() {
			Handle<Buffer> staging_handle = Buffer::make(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
			Buffer *staging_buf = staging_handle.get();

			void *gpu_data = staging_buf->map();
			memcpy(gpu_data, data, size);
			staging_buf->unmap();

			Buffer buf;
			buf.allocate(size, vert_or_ind | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

			Engine::AsyncQueue &queue = g_engine->async_transfer;
    		VkCommandBuffer cmd = queue.getCmd();
    		pk_assert(cmd);

			VkBufferCopy copy = { .size = size };
			vkCmdCopyBuffer(cmd, staging_buf->value, buf.value, 1, &copy);

			queue.waitUntilFinished(cmd);

			AssetManager::destroy(staging_handle);
			AssetManager::finishLoading(out, mem::move(buf));

			info("loaded %s buffer", vert_or_ind == VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ? "vertex" : "index");
		}
	);
}

void Mesh2::load(StrView fname, StrView name) {
	Str filename = fname;
	Str mesh_name = name;

	Handle<Buffer> vbuf  = Buffer::makeAsync();
	Handle<Buffer> libuf = Buffer::makeAsync();
	Handle<Buffer> gibuf = Buffer::makeAsync();
	Handle<Buffer> mbuf  = Buffer::makeAsync();

	vert_buf       = vbuf;
	local_ind_buf  = libuf;
	global_ind_buf = gibuf;
	meshlets       = mbuf;

	g_engine->jobpool.pushJob(
		[
			vert_buf = vbuf, 
		 	local_ind_buf = libuf,
		 	global_ind_buf = gibuf,
		 	meshlets = mbuf, 
		 	name = mem::move(mesh_name), 
		 	fname = mem::move(filename)
		]
		() {
			asio::File file;
			file.init(fname);
			if (!file.isValid()) {
				err("failed to load asset file %s", fname);
				return;
			}

			while (!file.poll()) {
				co::yield();
			}

			arr<byte> file_data = file.getData();

			AssetFile asset;
			if (!asset.load(file_data)) {
				err("failed to load asset file %s", fname);
				return;
			}

			AssetMesh info = AssetMesh::readInfo(asset);
			static_assert(sizeof(Vertex) == sizeof(AssetMesh::Vertex));

			arr<Vertex> verts;
			arr<u32> indices;
			
			verts.grow(info.vbuf_size);
			indices.grow(info.ibuf_size / info.index_size);
			
			//if (Mesh *mesh = g_engine->m_meshes.get(name)) {
			//	mesh->index_count = (u32)indices.len;
			//}

			info.unpack(asset.blob, (byte *)verts.data(), (byte *)indices.data());
			
			//mesh__upload(vrt_buf, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verts.data(), verts.byteSize());
			//mesh__upload(ind_buf, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data(), indices.byteSize());

			//while (!vrt_buf.isLoaded()) co::yield();
			//while (!ind_buf.isLoaded()) co::yield();

			//if (gen_meshlets) {
			//	Meshlet meshlet = {};
			//	for (u32 index : indices) {
			//		meshlet.indices[meshlet.icount++] = index;
			//		meshlet.indices[meshlet.vcount++] = index;
			//		if (meshlet.icount >= 0 || meshlet.vcount >= 0) {
			//
			//		}
			//		// meshlet[]
			//	}
			//}

			info("finished loading model %s", fname.cstr());
		}
	);
}

bool Mesh::load(const char *fname, StrView name, arr<Meshlet> *gen_meshlets) {
	Str filename = fname;
	Str mesh_name = name;

	Handle<Buffer> vrt_buf = Buffer::makeAsync();
	Handle<Buffer> ind_buf = Buffer::makeAsync();

	vbuf = vrt_buf;
	ibuf = ind_buf;

	g_engine->jobpool.pushJob(
		[vrt_buf, ind_buf, gen_meshlets, name = mem::move(mesh_name), fname = mem::move(filename)]
		() {
			asio::File file;
			file.init(fname);
			if (!file.isValid()) {
				err("failed to load asset file %s", fname);
				return;
			}

			while (!file.poll()) {
				co::yield();
			}

			arr<byte> file_data = file.getData();

			AssetFile asset;
			if (!asset.load(file_data)) {
				err("failed to load asset file %s", fname);
				return;
			}

			AssetMesh info = AssetMesh::readInfo(asset);
			static_assert(sizeof(Vertex) == sizeof(AssetMesh::Vertex));

			arr<Vertex> verts;
			arr<u32> indices;
			
			verts.grow(info.vbuf_size / sizeof(Vertex));
			indices.grow(info.ibuf_size / info.index_size);
			
			if (Mesh *mesh = g_engine->m_meshes.get(name)) {
				mesh->index_count = (u32)indices.len;
			}

			info.unpack(asset.blob, (byte *)verts.data(), (byte *)indices.data());
			
			mesh__upload(vrt_buf, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verts.data(), verts.byteSize());
			mesh__upload(ind_buf, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data(), indices.byteSize());

			while (!vrt_buf.isLoaded()) co::yield();
			while (!ind_buf.isLoaded()) co::yield();

			if (gen_meshlets) {
				Meshlet meshlet = {};
				for (u32 index : indices) {
					meshlet.indices[meshlet.icount++] = index;
					meshlet.indices[meshlet.vcount++] = index;
					if (meshlet.icount >= 0 || meshlet.vcount >= 0) {

					}
					// meshlet[]
				}
			}

			info("finished loading model %s", fname.cstr());
		}
	);
	
	return true;
}

// void Mesh::upload() {
// 	vbuf = mesh__upload(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verts.data(), verts.byteSize());
// 	ibuf = mesh__upload(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data(), indices.byteSize());
// }
