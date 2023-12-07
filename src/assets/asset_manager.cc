#include "asset_manager.h"

#include "std/arr.h"
#include "std/hashset.h"

#include "gfx/engine.h"

#include "asset.h"
#include "texture.h"
#include "descriptor.h"
#include "buffer.h"

// MANAGER ///////////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct AssetListManager {
    AssetListManager() {
        arena = Arena::make(gb(1), Arena::Virtual);
    }

    void cleanup() {
        for (u32 i = 0; i < count; ++i) {
            values[i].~T();
        }
    }

    T *get(Handle<T> handle) {
        u32 index = handle.value;
        if (index >= count) return nullptr;
        if (!is_loaded[index]) return nullptr;
        return &values[index];
    }

    void destroy(Handle<T> handle) {
        u32 index = handle.value;
        if (index >= count) return;
        values[index].~T();
        is_loaded[index] = false;
        freelist.push(index);
    }

    bool isLoaded(Handle<T> handle) {
        u32 index = handle.value;
        if (index >= count) return false;
        return is_loaded[index];
    }

    void startLoading(Handle<T> handle) {
        u32 index = handle.value;
        if (index >= count) return;
        is_loaded[index] = false;
    }

    void finishLoading(Handle<T> handle, T &&asset) {
        u32 index = handle.value;
        if (index >= count) return;
        is_loaded[index] = true;
        values[index] = mem::move(asset);
    }

    Handle<T> getNewHandle() {
        if (freelist.empty()) {
            u32 index = (u32)count;
            is_loaded.push(false);
            // arena is only used by values, meaning it will be one big list
            arena.alloc<T>();
            values = (T *)arena.start;
            ++count;
            return index;
        }

        u32 index = freelist.back();
        freelist.pop();
        return index;
    }

    T *values;
    u32 count = 0;
    arr<bool> is_loaded;
    arr<u32> freelist;
    Arena arena;
};

#define MAKE_MANAGER(type, prefix)                                                                                                           \
    static AssetListManager<type> prefix##_manager;                                                                                          \
    type *AssetManager::get(Handle<type> handle)                        { return prefix##_manager.get(handle); }                             \
    void AssetManager::destroy(Handle<type> handle)                     { return prefix##_manager.destroy(handle); }                         \
    bool AssetManager::isLoaded(Handle<type> handle)                    { return prefix##_manager.isLoaded(handle); }                        \
    void AssetManager::startLoading(Handle<type> handle)                { return prefix##_manager.startLoading(handle); }                    \
    void AssetManager::finishLoading(Handle<type> handle, type &&asset) { return prefix##_manager.finishLoading(handle, mem::move(asset)); } \
    Handle<type> AssetManager::getNew##type##Handle()                   { return prefix##_manager.getNewHandle(); }

MAKE_MANAGER(Texture, tex);
MAKE_MANAGER(Descriptor, desc);
MAKE_MANAGER(Buffer, buf);

// PUBLIC FUNCTIONS //////////////////////////////////////////////////////////////////////////////

void AssetManager::loadDefaults() {
    Handle<Texture> default_texture = Texture::load("default.png");
    buf_manager.getNewHandle();
    //// wait for the default texture to load
    //while (!default_texture.isLoaded()) {
    //    g_engine->transferUpdate();
    //}
}

bool AssetManager::areDefaultsLoaded() {
    return tex_manager.isLoaded(0) && desc_manager.isLoaded(0);
}

void AssetManager::cleanup() {
    tex_manager.cleanup();
    desc_manager.cleanup();
    buf_manager.cleanup();
}
