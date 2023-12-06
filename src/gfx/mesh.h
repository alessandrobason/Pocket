#pragma once

#include <glm/mat4x4.hpp>

#include "std/arr.h"
#include "std/vec.h"
#include "std/str.h"
// #include "utils/glm.h"

#include "vk_fwd.h"
#include "assets/buffer.h"
#include "assets/descriptor.h"

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

using Index = u32;

struct Mesh {
	Handle<Buffer> vbuf;
	Handle<Buffer> ibuf;
	u32 index_count = 0;

	bool loadFromObj(const char *fname);
	bool load(const char *fname, StrView name);

	struct PushConstants {
		vec4 data;
		glm::mat4 model;
	};
};

struct Material {
	VkPipeline pipeline_ref;
	VkPipelineLayout layout_ref;
	// VkDescriptorSet texture_set = nullptr;
	Handle<Descriptor> texture_desc;
};
