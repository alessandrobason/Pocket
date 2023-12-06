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
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, col)
	});

	desc.attributes.push(VkVertexInputAttributeDescription{
		.location = 3,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = offsetof(Vertex, uv)
	});

	return desc;
}

bool Mesh::loadFromObj(const char *fname) {
#if 0
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn_str, err_str;

	tinyobj::LoadObj(
		&attrib,
		&shapes,
		&materials,
		&warn_str,
		&err_str,
		fname,
		nullptr
	);

	if (!warn_str.empty()) {
		warn("%s", warn_str.c_str());
	}

	if (!err_str.empty()) {
		err("%s", err_str.c_str());
		return false;
	}

	for (const auto &shape : shapes) {
		usize index_offset = 0;
		
		for (const auto &face : shape.mesh.num_face_vertices) {
			usize fv = 3;

			for (usize v = 0; v < fv; ++v) {
				tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

				//copy it into our vertex
				Vertex new_vert = {
					.pos = { 
						attrib.vertices[3 * idx.vertex_index + 0], 
						attrib.vertices[3 * idx.vertex_index + 1], 
						attrib.vertices[3 * idx.vertex_index + 2] 
					},
					.norm = { 
						attrib.normals[3 * idx.normal_index + 0], 
						attrib.normals[3 * idx.normal_index + 1], 
						attrib.normals[3 * idx.normal_index + 2] 
					},
					.col = new_vert.norm,
					.uv = {
						attrib.texcoords[2 * idx.texcoord_index + 0],
						1 - attrib.texcoords[2 * idx.texcoord_index + 1],
					},
				};

				verts.push(new_vert);
			}

			index_offset += fv;
		}
	}
#endif
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

    		VkCommandBuffer cmd = g_engine->getTransferCmd();
    		pk_assert(cmd);

			VkBufferCopy copy = { .size = size };
			vkCmdCopyBuffer(cmd, staging_buf->value, buf.value, 1, &copy);

			u64 wait_handle = 0;
			while (!(wait_handle = g_engine->trySubmitTransferCommand(cmd))) {
				co::yield();
			}

			while (!g_engine->isTransferFinished(wait_handle)) {
				co::yield();
			}

			AssetManager::destroy(staging_handle);
			AssetManager::finishLoading(out, mem::move(buf));

			info("loaded %s buffer", vert_or_ind == VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ? "vertex" : "index");
		}
	);
}

bool Mesh::load(const char *fname, StrView name) {
	Str filename = fname;
	Str mesh_name = name;

	Handle<Buffer> vrt_buf = Buffer::makeAsync();
	Handle<Buffer> ind_buf = Buffer::makeAsync();

	vbuf = vrt_buf;
	ibuf = ind_buf;

	g_engine->jobpool.pushJob(
		[vrt_buf, ind_buf, name = mem::move(mesh_name), fname = mem::move(filename)]
		() {
			AssetFile file;
			if (!file.load(fname.cstr())) {
				err("failed to load asset file %s", fname);
				return;
			}

			AssetMesh info = AssetMesh::readInfo(file);
			static_assert(sizeof(Vertex) == sizeof(AssetMesh::Vertex));

			arr<Vertex> verts;
			arr<u32> indices;
			
			verts.grow(info.vbuf_size);
			indices.grow(info.ibuf_size / info.index_size);
			
			if (Mesh *mesh = g_engine->m_meshes.get(name)) {
				mesh->index_count = (u32)indices.len;
			}

			info.unpack(file.blob, (byte *)verts.data(), (byte *)indices.data());
			
			mesh__upload(vrt_buf, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verts.data(), verts.byteSize());
			mesh__upload(ind_buf, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data(), indices.byteSize());

			while (!vrt_buf.isLoaded()) co::yield();
			while (!ind_buf.isLoaded()) co::yield();

			info("finished loading model %s", fname.cstr());
		}
	);
	
	return true;
}

// void Mesh::upload() {
// 	vbuf = mesh__upload(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verts.data(), verts.byteSize());
// 	ibuf = mesh__upload(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data(), indices.byteSize());
// }
