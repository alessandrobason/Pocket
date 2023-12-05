#include "asset_manager.h"

#include "std/arr.h"
#include "std/hashset.h"

#include "core/guid.h"

#include "asset.h"
#include "texture.h"

static arr<arr<mem::ptr<Asset>>> assets;
static HashSet<u64> loading_assets;

static arr<Texture> textures;
static arr<bool> tex_is_loaded;

//////////////////////////////////////////////////////////////////////////////////////////////////

void AssetManager::loadDefaults() {
    Texture::load("lost_empire-RGBA.png");
    Texture::load("default.png");
}

template<>
Texture *AssetManager::get(Handle<Texture> handle) {
    u32 index = handle.value;
    if (index >= textures.len) return nullptr;
    if (!tex_is_loaded[index]) return nullptr;
    return &textures[index];
}

template<>
bool AssetManager::isLoaded(Handle<Texture> handle) {
    u32 index = handle.value;
    if (index >= textures.len) return false;
    return tex_is_loaded[index];
}

template<>
void AssetManager::startLoading(Handle<Texture> handle) {
    u32 index = handle.value;
    if (index >= textures.len) return;
    tex_is_loaded[index] = false;
}

template<>
void AssetManager::finishLoading(Handle<Texture> handle, Texture &&asset) {
    u32 index = handle.value;
    if (index >= textures.len) return;
    tex_is_loaded[index] = true;
    textures[index] = mem::move(asset);
}

template<>
Handle<Texture> AssetManager::getNewHandle() {
    u32 index = (u32)textures.len;
    tex_is_loaded.push(false);
    textures.push();
    return index;
}

#if 0
Asset *AssetManager::getAsset(u32 type_id, u32 handle) {
    if (type_id >= assets.len) return nullptr;
    auto &type_assets = assets[type_id];
    
    if (handle >= type_assets.len) return nullptr;
    return type_assets[handle].get();
}

bool AssetManager::isAssetLoaded(u32 type_id, u32 handle) {
    u64 value = ((u64)type_id << 32) | handle;
    return loading_assets.has(value);
}

u32 AssetManager::getNewAssetHandle(u32 type_id) {
    if (type_id >= assets.len) {
        assets.resize(type_id + 1);
    }

    return assets[type_id].push();
}

void AssetManager::startLoadingAsset(u32 type_id, u32 handle) {
    u64 value = ((u64)type_id << 32) | handle;
    if (!loading_assets.push(value)) {
        warn("trying to load asset again, type id: %u, handle: %u", type_id, handle);
    }
}

void AssetManager::finishLoadingAsset(u32 type_id, u32 handle, mem::ptr<Asset> &&asset) {
    u64 value = ((u64)type_id << 32) | handle;
    if (!loading_assets.remove(value)) {
        warn("finished loading asset wasn't loading, type id: %u, handle: %u", type_id, handle);
    }
    pk_assert(type_id < assets.len);
    pk_assert(handle < assets[type_id].len);
    assets[type_id][handle] = mem::move(asset);
}
#endif