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

	return true;
}

bool Mesh::load(const char *fname) {
	AssetFile file;
	if (!file.load(fname)) {
		err("failed to load asset file %s", fname);
		return false;
	}

	AssetMesh info = AssetMesh::readInfo(file);
	static_assert(sizeof(Vertex) == sizeof(AssetMesh::Vertex));
	
	verts.resize(info.vbuf_size);
	indices.resize(info.ibuf_size / info.index_size);

	info.unpack(file.blob, (byte *)verts.data(), (byte *)indices.data());

	return true;
}

#define PK_VKCHECK(x) \
    do { \
        VkResult err = x; \
        if(err) { \
            fatal("%s: Vulkan error: %d", #x, err);  \
        } \
    } while (0)

Buffer mesh__upload(Engine &engine, VkBufferUsageFlagBits vert_or_ind, const void *data, usize size) {
	Buffer outbuf;
	
	VkBufferCreateInfo buf_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = (u32)size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	};

	VmaAllocationCreateInfo alloc_info = {
		.usage = VMA_MEMORY_USAGE_CPU_ONLY,
	};

	Buffer staging_buffer;

	PK_VKCHECK(vmaCreateBuffer(
		engine.m_allocator,
		&buf_info,
		&alloc_info,
		&staging_buffer.buffer,
		&staging_buffer.alloc,
		nullptr
	));

	void *gpu_data;
	vmaMapMemory(engine.m_allocator, staging_buffer.alloc, &gpu_data);
	memcpy(gpu_data, data, size);
	vmaUnmapMemory(engine.m_allocator, staging_buffer.alloc);

	buf_info.usage = vert_or_ind | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	PK_VKCHECK(vmaCreateBuffer(
		engine.m_allocator,
		&buf_info,
		&alloc_info,
		&outbuf.buffer,
		&outbuf.alloc,
		nullptr
	));

	engine.immediateSubmit(
		[&size, &staging_buffer, &outbuf]
		(VkCommandBuffer cmd) {
			VkBufferCopy copy = {
				.size = size,
			};
			vkCmdCopyBuffer(cmd, staging_buffer.buffer, outbuf.buffer, 1, &copy);
		}
	);

	// vmaDestroyBuffer(engine.m_allocator, staging_buffer.buffer, staging_buffer.allocation);

	return outbuf;
}

void Mesh::upload(Engine &engine) {
	vbuf = mesh__upload(engine, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verts.data(), verts.byteSize());
	ibuf = mesh__upload(engine, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data(), indices.byteSize());
}
