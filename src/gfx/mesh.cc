﻿#pragma once

#include "mesh.h"

// TODO remove tiny_obj_loader
#include <vector>
#include <string>
#include <tiny_obj_loader.h>
#include <vulkan/vulkan.h>

#include "std/common.h"
#include "std/logging.h"

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