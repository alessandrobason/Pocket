#pragma once

#include "std/arr.h"
#include "utils/glm.h"

#include "vk_fwd.h"
#include "buffer.h"

struct VertexInDesc {
	arr<VkVertexInputBindingDescription> bindings;
	arr<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
	vec3 pos;
	vec3 norm;
	vec3 col;
	vec2 uv;

	static VertexInDesc getVertexDesc();
};

struct Mesh {
	arr<Vertex> verts;
	Buffer buffer;

	bool loadFromObj(const char *fname);

	struct PushConstants {
		vec4 data;
		mat4 model;
	};
};

struct Material {
	VkPipeline pipeline;
	VkPipelineLayout layout;
	VkDescriptorSet texture_set = nullptr;
};
