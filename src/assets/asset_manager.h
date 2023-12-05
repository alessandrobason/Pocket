#pragma once

#include "std/common.h"
#include "std/mem.h"
#include "core/guid.h"
#include "core/handle.h"

struct Asset;
struct Texture;

namespace AssetManager {
    void loadDefaults();

    template<typename T>
    T *get(Handle<T> handle) = delete;

    template<typename T>
    bool isLoaded(Handle<T> handle) = delete;

    template<typename T>
    void startLoading(Handle<T> handle) = delete;

    template<typename T>
    void finishLoading(Handle<T> handle, T &&asset) = delete;

    template<typename T>
    Handle<T> getNewHandle() = delete;

    // TEXTURES //////////////////////////////////////////////////////////////////

    extern template Texture *get(Handle<Texture> handle);
    extern template bool isLoaded(Handle<Texture> handle);
    extern template void startLoading(Handle<Texture> handle);
    extern template void finishLoading(Handle<Texture> handle, Texture &&asset);
    extern template Handle<Texture> getNewHandle();

#if 0
    Asset *getAsset(u32 type_id, u32 handle);
    bool isAssetLoaded(u32 type_id, u32 handle);
    u32 getNewAssetHandle(u32 type_id);
    void startLoadingAsset(u32 type_id, u32 handle);
    void finishLoadingAsset(u32 type_id, u32 handle, mem::ptr<Asset> &&asset);

    template<typename T>
    T *get(u32 handle) {
        return getAsset(guid::type<T>(), handle);
    }

    template<typename T>
    bool isLoaded(u32 handle) {
        return isAssetLoaded(guid::type<T>(), handle);
    }

    template<typename T>
    void startLoading(u32 handle) {
        startLoadingAsset(guid::type<T>(), handle);
    }

    template<typename T>
    void finishLoading(u32 handle, mem::ptr<Asset> &&asset) {
        finishLoadingAsset(guid::type<T>(), handle, mem::move(asset));
    }

    template<typename T>
    u32 getNewHandle(bool start_loading = true) {
        u32 handle = getNewAssetHandle(guid::type<T>());
        if (start_loading) {
            startLoading<T>(handle);
        }
        return handle;
    }
#endif
} // namespace AssetManager
