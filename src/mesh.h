// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vector>
#include <glm/vec3.hpp>
#include <vk_types.h>

struct Vertex {
	glm::vec3 pos;
	glm::vec3 norm;
	glm::vec3 col;
};

struct VertexInDesc {
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Mesh {
	std::vector<Vertex> m_verts;
	Buffer m_buf;

	bool loadFromObj(const char *fname);
	static VertexInDesc getVertexDesc();
};