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
	// vec3 col;
	vec2 uv;
	u32 col;

	static VertexInDesc getVertexDesc();
};

using Index = u32;

struct Meshlet {
	u32 vertices[64];
	u32 indices[126 * 3];
	u32 vcount;
	u32 icount;
};

struct Mesh2 {
	Handle<Buffer> vert_buf;
	Handle<Buffer> local_ind_buf;
	Handle<Buffer> global_ind_buf;
	Handle<Buffer> meshlets;
	u32 meshlets_count = 0;

	void load(StrView fname, StrView name);
};

struct Mesh {
	Handle<Buffer> vbuf;
	Handle<Buffer> ibuf;
	u32 index_count = 0;

	bool loadFromObj(const char *fname);
	bool load(const char *fname, StrView name, arr<Meshlet> *gen_meshlets = nullptr);

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
