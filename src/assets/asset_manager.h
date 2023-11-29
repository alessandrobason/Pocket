#pragma once

#include "std/common.h"

struct Texture;

namespace AssetManager {
    void loadDefaults();

    template<typename T>
    T *get(u32 handle) = delete;

    template<typename T>
    bool isLoaded(u32 handle) = delete;
    
    // textures ///////////////////////////////////////////////////////////////////////////////////

    template<>
    Texture *get(u32 handle);

    template<>
    bool isLoaded<Texture>(u32 handle);

} // namespace AssetManager
