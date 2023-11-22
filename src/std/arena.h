#pragma once

#include "common.h"

constexpr usize kb(usize b) { return b * 1024; }
constexpr usize mb(usize b) { return kb(b) * 1024; }
constexpr usize gb(usize b) { return mb(b) * 1024; }

struct Arena {
    enum Type : u8 {
        Virtual  = 1 << 0, // using virtual memory (allocates only when needed)
        Malloc   = 1 << 1, // using malloc (preallocates memory on heap)
        Static   = 1 << 2, // using static buffer (you will need to provide it!)
        
        __TypeMask = 0x7F,
        __NotOwned = 1 << 7,
    };

    enum Flags : u8 {
        None     = 0,
        NoZero   = 1 << 0, // will not set the memory to zero
        SoftFail = 1 << 1, // will not panic when it can't allocate memory
    };

    static Arena make(usize initial_allocation, Type type = Type::Virtual);
    static Arena makeStatic(byte *data, usize len);
    template<usize size>
    static Arena makeStatic(byte (&data)[size]) {
        return makeStatic(data, size);
    }

    Arena() = default;
    Arena(const Arena &arena);
    Arena(Arena &&arena);
    ~Arena();

    void cleanup();

    void *alloc(usize size, usize count, usize align = 1, Flags flags = Flags::None);
    template<typename T>
    T *alloc(usize count = 1, Flags flags = Flags::None, usize size = sizeof(T), usize align = alignof(T)) {
        return (T *)alloc(size, align, count, flags);
    }

    usize tell() const;
    void rewind(usize from_start);
    void pop(usize amount);
    template<typename T>
    void pop(usize count = 1) { pop(sizeof(T) * count); }

    Arena &operator=(const Arena &other);
    Arena &operator=(Arena &&other);

    byte *start = nullptr;
    byte *current = nullptr;
    byte *end = nullptr;
    Type type = Type::Virtual;
};