#pragma once

#include "std/common.h"

// forward declare AssetManager
namespace AssetManager {
    template<typename T> bool isLoaded(u32);
    template<typename T> T *get(u32);
} // namespace AssetManager

template<typename T>
struct Handle {
    Handle() = default;
    Handle(u32 value) : value(value) {}

    bool isLoaded() {
        return AssetManager::isLoaded<T>(value);
    }

    T *get() {
        return AssetManager::get<T>(value);
    }

    u32 value = 0;
};
