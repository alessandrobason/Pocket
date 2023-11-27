#pragma once

#include "std/common.h"
#include "std/arr.h"
#include "std/slice.h"
#include "std/str.h"
#include "std/hashmap.h"

#include "vk_fwd.h"
#include "vk_ptr.h"

struct DescriptorLayoutCache;

struct ShaderCompiler {
	struct Overload {
		StrView name;
		VkDescriptorType type;
	};

	void init(VkDevice device);
	bool addStage(const char *filename, Slice<Overload> overloads = {});
	bool build(DescriptorLayoutCache &desc_cache);

	struct StageInfo {
		vkptr<VkShaderModule> module;
		VkShaderStageFlagBits stage;
	};

	arr<StageInfo> stages;
	vkptr<VkPipelineLayout> pipeline_layout;

private:
	struct LayoutData {
		arr<VkDescriptorSetLayoutBinding> bindings;
	};

	VkDevice device = nullptr;
	arr<LayoutData> sets;
	arr<VkPushConstantRange> push_constants;
};