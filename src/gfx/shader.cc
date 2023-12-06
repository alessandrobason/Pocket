#include "shader.h"

#include <vulkan/vulkan.h>
#include <spirv_reflect.h>

#include "std/file.h"
#include "std/logging.h"

#include "pipeline_builder.h"
#include "descriptor_cache.h"

void ShaderCompiler::init(VkDevice in_device) {
    device = in_device;

    //bindings.setTombstone(UINT32_MAX);
    //bindings.reserve(32);
}

bool ShaderCompiler::addStage(const char *filename, Slice<Overload> overloads) {
    info("### BEGIN ###");

    // load module
    arr<byte> code = File::readWhole(filename);
    if (code.empty()) {
        err("couldn't load shader %s", filename);
        return false;
    }

	VkShaderModuleCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.len,
		.pCode = (u32 *)code.buf,
	};

	VkShaderModule module;
	if (vkCreateShaderModule(device, &info, nullptr, &module)) {
        err("couldn't create shader module");
        return false;
	}

    // reflect shader using spirv-reflect

#define SPV_CHECK(...) if (spv_result != SPV_REFLECT_RESULT_SUCCESS) { err(__VA_ARGS__); return {}; }

    SpvReflectShaderModule spv_module;
    SpvReflectResult spv_result = spvReflectCreateShaderModule(code.len, code.buf, &spv_module);

    SPV_CHECK("could not reflect shader %s: %d", filename, spv_result);

    u32 desc_set_count = 0;
    spv_result = spvReflectEnumerateDescriptorSets(&spv_module, &desc_set_count, nullptr);

    SPV_CHECK("could not reflect shader descriptor sets %s: %d", filename, spv_result);

    arr<SpvReflectDescriptorSet*> desc_sets;
    desc_sets.resize(desc_set_count);
    spv_result = spvReflectEnumerateDescriptorSets(&spv_module, &desc_set_count, desc_sets.buf);

    SPV_CHECK("could not reflect shader descriptor sets %s: %d", filename, spv_result);

    for (SpvReflectDescriptorSet *set : desc_sets) {
        if (set->set >= sets.len) {
            sets.resize(set->set + 1);
        }

        LayoutData &data = sets[set->set];
        
        for (u32 i = 0; i < set->binding_count; ++i) {
            SpvReflectDescriptorBinding *binding = set->bindings[i];

            if (binding->binding >= data.bindings.len) {
                data.bindings.resize(binding->binding + 1);
            }

            VkDescriptorSetLayoutBinding &lay_bind = data.bindings[binding->binding];
            
            lay_bind.binding = binding->binding;
            lay_bind.descriptorType = (VkDescriptorType)binding->descriptor_type;
            lay_bind.stageFlags = (VkShaderStageFlagBits)spv_module.shader_stage;
            lay_bind.descriptorCount = 1;
            for (u32 dim = 0; dim < binding->array.dims_count; ++dim) {
                lay_bind.descriptorCount *= binding->array.dims[dim];
            }

            for (const Overload &overload : overloads) {
                if (overload.name == binding->name) {
                    lay_bind.descriptorType = overload.type;
                }
            }

            const char *e[] = {
                "VK_DESCRIPTOR_TYPE_SAMPLER",
                "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER",
                "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE",
                "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE",
                "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER",
                "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER",
                "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER",
                "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER",
                "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC",
                "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC",
                "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT",
            };

            info("%s:(%u:%u) -> %s | %s", filename, set->set, i, binding->name, e[lay_bind.descriptorType]);
        }

        //layout.info.bindingCount = set->binding_count;
        //layout.info.pBindings = layout.bindings.data();

        //if (!layout.bindings.empty()) {
        //    vkptr<VkDescriptorSetLayout> &set_layout = set_layouts.push();
        //    //desc_cache.createDescLayout();
        //    VkResult vk_result = vkCreateDescriptorSetLayout(device, &layout.info, nullptr, set_layout.getRef());
        //    pk_assert(vk_result == VK_SUCCESS);
        //}
    }

    // reflect push constants

    u32 push_count = 0;
    spv_result = spvReflectEnumeratePushConstantBlocks(&spv_module, &push_count, nullptr);

    SPV_CHECK("could not reflect push constant blocks for %s: %d", filename, spv_result);

    arr<SpvReflectBlockVariable*> refl_pc;
    refl_pc.resize(push_count);
    spv_result = spvReflectEnumeratePushConstantBlocks(&spv_module, &push_count, refl_pc.buf);

    SPV_CHECK("could not reflect push constant blocks for %s: %d", filename, spv_result);

    pk_assert(push_count < 2);

    push_constants.reserve(push_constants.len + refl_pc.size());

    for (SpvReflectBlockVariable *pc : refl_pc) {
        push_constants.push({
            .stageFlags = (VkShaderStageFlags)spv_module.shader_stage,
            .offset = pc->offset,
            .size = pc->size,
        });
    }

    stages.push(StageInfo{
        .module = module,
        .stage = (VkShaderStageFlagBits)spv_module.shader_stage,
    });

#if 0
    // create pipeline layout

    VkPipelineLayoutCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = (u32)set_layouts.len,
        .pSetLayouts = (VkDescriptorSetLayout *)set_layouts.buf,
        .pushConstantRangeCount = (u32)push_constants.len,
        .pPushConstantRanges = push_constants.buf,
    };

    vkptr<VkPipelineLayout> pipeline_layout;
    VkResult vk_result = vkCreatePipelineLayout(device, &pipeline_info, nullptr, pipeline_layout.getRef());


    // create pipeline
    PipelineBuilder builder;
    builder
        //.setVertexInput(Vertex::getVertexDesc())
        //.setInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        //.setRasterizer(VK_CULL_MODE_BACK_BIT)
        //.setColourBlend()
        //.setMultisampling()
        .setLayout(pipeline_layout)
        //.setDepthStencil(VK_COMPARE_OP_LESS_OR_EQUAL)
		//.setDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .pushShader((VkShaderStageFlagBits)spv_module.shader_stage, module);

#endif
    info("### END ###");

    return true;
}

bool ShaderCompiler::build(DescriptorLayoutCache &desc_cache) {
#if 0
    arr<VkDescriptorSetLayoutCreateInfo> merged;
    merged.resize(max_set + 1);

    arr<VkDescriptorSetLayoutBinding> ordered_bindings;

    for (u32 i = 0; i < max_set; ++i) {
        bool repeat = false;
        u32 last_binding = 0;

        for (const LayoutData &layout : layouts) {
            if (layout.set_number != i) {
                continue;
            }


        }
    }

    for (const LayoutData &layout : layouts) {
        VkDescriptorSetLayoutCreateInfo &info = merged[layout.set_number];
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 0;
        info.pBindings = 0;
    }
#endif

    arr<VkDescriptorSetLayout> set_layouts;
    set_layouts.reserve(sets.len);

    for (LayoutData &data : sets) {
        if (data.bindings.empty()) {
            continue;
        }

        VkDescriptorSetLayoutCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = (u32)data.bindings.len,
            .pBindings = data.bindings.buf,
        };

        set_layouts.push(desc_cache.createDescLayout(info));
    }

    //for (u32 i = 0; i < max_set; ++i) {
    //    arr<VkDescriptorSetLayoutBinding> *binds = bindings.get(i);
    //    if (!binds) {
    //        warn("no binds for set %u", i);
    //        continue;
    //    }
    //
    //    info("set %u, bindings: %zu", i, binds->len);
    //
    //    VkDescriptorSetLayoutCreateInfo info = {
    //        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    //        .bindingCount = (u32)binds->len,
    //        .pBindings = binds->buf,
    //    };
    //
    //    set_layouts.push(desc_cache.createDescLayout(info));
    //}

    // create pipeline layout
    VkPipelineLayoutCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = (u32)set_layouts.len,
        .pSetLayouts = (VkDescriptorSetLayout *)set_layouts.buf,
        .pushConstantRangeCount = (u32)push_constants.len,
        .pPushConstantRanges = push_constants.buf,
    };

    VkResult vk_result = vkCreatePipelineLayout(device, &pipeline_info, nullptr, pipeline_layout.getRef());
    return vk_result == VK_SUCCESS;
}
