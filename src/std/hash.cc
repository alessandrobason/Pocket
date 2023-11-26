#include "hash.h"

u32 hashFnv132(const void *data, usize len) {
    constexpr u32 fnv_prime = 0x01000193u;
    constexpr u32 fnv_offset = 0x811c9dc5u;

    u32 hash = fnv_offset;
    byte *bytes = (byte *)data;

    for (usize i = 0; i < len; ++i) {
        hash = (hash * fnv_prime) ^ bytes[i];
    }

    return hash;
}

u64 hashFnv164(const void *data, usize len) {
    constexpr u64 fnv_prime = 0x00000100000001B3ul;
    constexpr u64 fnv_offset = 0xcbf29ce484222325ul;

    u64 hash = fnv_offset;
    byte *bytes = (byte *)data;

    for (usize i = 0; i < len; ++i) {
        hash = (hash * fnv_prime) ^ bytes[i];
    }

    return hash;
}