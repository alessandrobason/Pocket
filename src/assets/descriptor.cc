#include "descriptor.h"

#include "gfx/engine.h"
#include "gfx/descriptor_cache.h"

#include "texture.h"

Handle<Descriptor> Descriptor::make(AsyncDescBuilder &builder) {
    Handle<Descriptor> handle = AssetManager::getNewDescriptorHandle();

    g_engine->jobpool.pushJob(
        //[handle, builder = mem::move(builder)]
        [handle, builder]
        () {
            using Type = AsyncDescBuilder::BindType;
            // wait for all resources to be ready
            for (const AsyncDescBuilder::Binding &bind : builder.bindings) {
                switch (bind.bind_type) {
                    case Type::Error:   err("unkown bind type"); break;
                    case Type::Texture: 
                        while (!AssetManager::isLoaded(bind.texture)) {
                            co::yield();
                        }
                        break;
                    case Type::Buffer:  err("buffer not supported yet"); break;
                }
            }

            DescriptorBuilder desc_builder = DescriptorBuilder::begin(g_engine->m_desc_cache, g_engine->m_desc_alloc);
        
            for (const auto &bind : builder.bindings) {
                switch (bind.bind_type) {
                    case Type::Error:   err("unkown bind type"); break;
                    case Type::Texture:
                    {
                        Texture *texture = bind.texture.get();
                        pk_assert(texture);

                        desc_builder.bindImage(
                            bind.slot,
                            {
                                .sampler = bind.sampler,
                                .imageView = texture->view,
                                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            },
                            bind.type,
                            bind.flags
                            );
                        break;
                    }
                    case Type::Buffer:  err("buffer not supported yet"); break;
                }
            }

            Descriptor desc;
            desc.set = desc_builder.build();

            AssetManager::finishLoading(handle, mem::move(desc));
        }
    );

    return handle;   
}

AsyncDescBuilder AsyncDescBuilder::begin() {
    return AsyncDescBuilder();
}

AsyncDescBuilder &AsyncDescBuilder::bindImage(u32 slot, Handle<Texture> texture, VkSampler sampler, VkDescriptorType type, VkShaderStageFlags flags) {
    bindings.push(Binding{ slot, type, flags, texture, sampler, BindType::Texture });
    return *this;
}
