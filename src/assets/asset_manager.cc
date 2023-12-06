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
    T *get(Handle<T> handle) {
        u32 index = handle.value;
        if (index >= values.len) return nullptr;
        if (!is_loaded[index]) return nullptr;
        return &values[index];
    }

    void destroy(Handle<T> handle) {
        u32 index = handle.value;
        if (index >= values.len) return;
        values[index].~T();
        is_loaded[index] = false;
        freelist.push(index);
    }

    bool isLoaded(Handle<T> handle) {
        u32 index = handle.value;
        if (index >= values.len) return false;
        return is_loaded[index];
    }

    void startLoading(Handle<T> handle) {
        u32 index = handle.value;
        if (index >= values.len) return;
        is_loaded[index] = false;
    }

    void finishLoading(Handle<T> handle, T &&asset) {
        u32 index = handle.value;
        if (index >= values.len) return;
        is_loaded[index] = true;
        values[index] = mem::move(asset);
    }

    Handle<T> getNewHandle() {
        if (freelist.empty()) {
            u32 index = (u32)values.len;
            is_loaded.push(false);
            values.push();
            return index;
        }

        u32 index = freelist.back();
        freelist.pop();
        return index;
    }

    arr<T> values;
    arr<u32> freelist;
    arr<bool> is_loaded;
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

    // wait for the default texture to load
    while (!default_texture.isLoaded()) {
        g_engine->transferUpdate();
    }
}

void AssetManager::cleanup() {
    tex_manager.values.clear();
    desc_manager.values.clear();
    buf_manager.values.clear();
}
