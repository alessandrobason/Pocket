#include "asset_manager.h"

#include "std/arr.h"

static arr<Texture> textures;
constexpr u32 tex_ind_mask = 0xffffffff;

void AssetManager::loadDefaults() {
    
}

template<>
Texture *AssetManager::get(u32 handle) {
    u32 index = handle & tex_ind_mask;

    if (index >= textures.len) {
        return nullptr;
    }

    return &textures[index];
}

template<>
bool AssetManager::isLoaded<Texture>(u32 handle) {
    return false;
}
