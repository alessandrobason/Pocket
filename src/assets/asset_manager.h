#pragma once

#include "std/common.h"
#include "std/mem.h"
#include "core/guid.h"

template<typename T>
struct Handle {
    Handle() = default;
    Handle(u32 value) : value(value) {}

    bool isLoaded() const;
    T *get() const;

    operator bool() const { return value != 0; }

    u32 value = 0;
};

struct Texture;
struct Descriptor;
struct Buffer;

namespace AssetManager {
    void loadDefaults();
    void cleanup();

    // TEXTURES //////////////////////////////////////////////////////////////////

    Texture *get(Handle<Texture> handle);
    void destroy(Handle<Texture> handle);
    bool isLoaded(Handle<Texture> handle);
    void startLoading(Handle<Texture> handle);
    void finishLoading(Handle<Texture> handle, Texture &&asset);
    Handle<Texture> getNewTextureHandle();

    // DESCRIPTORS ///////////////////////////////////////////////////////////////

    Descriptor *get(Handle<Descriptor> handle);
    void destroy(Handle<Descriptor> handle);
    bool isLoaded(Handle<Descriptor> handle);
    void startLoading(Handle<Descriptor> handle);
    void finishLoading(Handle<Descriptor> handle, Descriptor &&asset);
    Handle<Descriptor> getNewDescriptorHandle();

    // BUFFERS ///////////////////////////////////////////////////////////////////

    Buffer *get(Handle<Buffer> handle);
    void destroy(Handle<Buffer> handle);
    bool isLoaded(Handle<Buffer> handle);
    void startLoading(Handle<Buffer> handle);
    void finishLoading(Handle<Buffer> handle, Buffer &&asset);
    Handle<Buffer> getNewBufferHandle();

} // namespace AssetManager

template<typename T>
bool Handle<T>::isLoaded() const {
    return AssetManager::isLoaded(*this);
}

template<typename T>
T *Handle<T>::get() const {
    return AssetManager::get(*this);
}