#pragma once

#include "std/common.h"
#include "std/str.h"
#include "std/arr.h"
#include "std/slice.h"

enum class Compression : u8 {
    None,
    Lz4,
};

struct AssetFile {
    byte type[4];
    u16 version;
    Str json;
    arr<byte> blob;

    bool save(const char *path) const;
    bool load(const char *path);
};

struct AssetTexture {
    enum Format : u32 {
        Unknown,
        Rgba8,
    };

    u64 byte_size;
    Format format;
    Compression compression;
    u32 pixel_size[3];
    Str original_file;

    static AssetTexture readInfo(const AssetFile &file);
    void unpack(Slice<byte> buffer, byte *destination);
    AssetFile pack(byte *pixel_data);
};

struct AssetMesh {
    struct Bounds {
        float origin[3];
        float radius;
        float scale[3];
    };

    struct Vertex {
        float pos[3];
        float norm[3];
        float col[3];
        float uv[2];
    };

    u64 vbuf_size;
    u64 ibuf_size;
    Bounds bounds;
    u8 index_size;
    Compression compression;
    Str original_file;

    static AssetMesh readInfo(const AssetFile &file);
    void unpack(Slice<byte> buffer, byte *dest_vbuf, byte *dest_ibuf);
    AssetFile pack(const byte *vertices, const byte *indices);
    Bounds calculateBounds(Slice<Vertex> vertices);
};

// TODO add material asset
struct AssetMaterial {
    enum Transparency : u8 {
        Opaque, Transparent, Masked
    };
};