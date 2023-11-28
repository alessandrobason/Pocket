#pragma once

#include "common.h"
#include "str.h"
#include "arr.h"
#include "slice.h"

struct Arena;

struct File {
    enum Mode : u8 {
        Read = 1 << 0,
        Write = 1 << 1,
        Clear = 1 << 2,
        Both = Read | Write
    };

    File() = default;
    File(StrView filename, Mode mode = Read);
    ~File();

    static u64 getTime(StrView path);
    static bool exists(StrView fname);
    static arr<byte> readWhole(StrView fname);
    static Str readWholeText(StrView fname);

    //static arr<byte> readWhole(Arena &arena, const char *fname);
    static Str readWholeText(Arena &arena, StrView fname);

    static bool writeWhole(StrView fname, Slice<byte> data);
    static bool writeWhole(StrView fname, StrView string);
    template<typename T>
    static bool writeWhole(StrView fname, Slice<T> data) {
        return writeWhole(fname, Slice<byte>((byte *)data.buf, data.byteSize()));
    }

    bool open(StrView fname, Mode mode = Read);
    void close();

    bool isValid() const;

    bool putc(char c);
    bool puts(StrView view);

    bool read(void *buf, usize len);
    bool write(const void *buf, usize len);

    template<typename T>
    bool read(T *buf, usize count = 1) {
        return read((void *)buf, sizeof(T) * count);
    }

    template<typename T>
    bool read(T &buf, usize count = 1) {
        return read((void *)&buf, sizeof(T) * count);
    }

    template<typename T>
    bool write(const T *buf, usize count = 1) {
        return write((const void *)buf, sizeof(T) * count);
    }

    template<typename T>
    bool write(const T &buf, usize count = 1) {
        return write((const void *)&buf, sizeof(T) * count);
    }

    bool seekEnd();
    bool rewind();

    u64 tell();
    usize getSize();

    u64 getTime();

    uptr file_ptr = 0;
};
