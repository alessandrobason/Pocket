#pragma once

#include "std/common.h"
#include "std/mem.h"
#include "core/guid.h"

struct Asset;

namespace AssetManager {
    void loadDefaults();

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
} // namespace AssetManager
