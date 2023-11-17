#include "assets.h"

#include <float.h> // float limits
#include <math.h>  // sqrt

// TODO use custom parser so we don't include 20k lines of code and STL
// maybe even use ini instead?
#include <json.hpp>
#include <lz4.h>

#include "std/file.h"
#include "std/logging.h"
#include "std/maths.h"

static StrView std__to_strv(const std::string &str) {
    return StrView(str.data(), str.size());
}

static const char *asset__comp_as_str(Compression compression);
static Compression asset__parse_compression(StrView compression);

bool AssetFile::save(const char *path) const {
    File fp;
    if (!fp.open(path, File::Write)) {
        err("could not open file %s to save asset", path);
    }

    fp.write(type, pk_arrlen(type));
    fp.write(version);
    fp.write((u32)json.size());
    fp.write((u32)blob.len);
    fp.write(json.data(), json.size());
    fp.write(blob.data(), blob.size());

    return true;
}

bool AssetFile::load(const char *path) {
    File fp;
    fp.open(path, File::Read);

    if (!fp.isValid()) {
        err("could not open %s", path);
        return false;
    }

    fp.read(type, pk_arrlen(type));
    fp.read(version);

    u32 json_size, blob_size;
    fp.read(json_size);
    fp.read(blob_size);

    json.resize(json_size);
    blob.resize(blob_size);

    fp.read(json.data(), json_size);
    fp.read(blob.data(), blob_size);

    return true;
}

static const char *asset__comp_as_str(Compression compression) {
    switch (compression) {
        case Compression::Lz4: return "LZ4";
    }
    return "none";
}

static Compression asset__parse_compression(StrView compression) {
    if (compression == "LZ4") return Compression::Lz4;
    return Compression::None;
}

// == ASSET TEXTURE =======================================================================================================================================================================================

static AssetTexture::Format texture__parse_format(StrView format);

AssetTexture AssetTexture::readInfo(const AssetFile &file) {
    AssetTexture info;
    
    nlohmann::json metadata = nlohmann::json::parse(file.json.cstr(), nullptr, false);
    
    info.format = texture__parse_format(metadata["format"].get<std::string>().c_str());
    info.compression = asset__parse_compression(std__to_strv(metadata["compression"]));
    info.pixel_size[0] = metadata["width"];
    info.pixel_size[1] = metadata["height"];
    info.byte_size = metadata["buffer_size"];
    info.original_file = std__to_strv(metadata["original_file"]);

    return info;
}

void AssetTexture::unpack(Slice<byte> buffer, byte *destination) {
    switch (compression) {
        case Compression::Lz4:
            LZ4_decompress_safe((const char *)buffer.buf, (char *)destination, (int)buffer.len, (int)byte_size); 
            break;
    }
}

AssetFile AssetTexture::pack(byte *pixel_data) {
    AssetFile file = {
        .type = { 'T', 'E', 'X', 'I' },
        .version = 1,
    };

    int compress_staging = LZ4_compressBound((int)byte_size);

    file.blob.resize(compress_staging);

    int compressed_size = LZ4_compress_default(
        (const char *)pixel_data, 
        (char *)file.blob.data(), 
        (int)byte_size, 
        compress_staging
    );

    file.blob.resize(compressed_size);

    nlohmann::json metadata = {
        { "format", "RGBA8" },
        { "width", pixel_size[0] },
        { "height", pixel_size[1] },
        { "buffer_size", byte_size },
        { "original_file", original_file.cstr() },
        { "compression", asset__comp_as_str(compression) },
    };

    // TODO really don't use json here, i don't want to copy
    file.json = std__to_strv(metadata.dump());
    
    return file;
}

static AssetTexture::Format texture__parse_format(StrView format) {
    if (format == "RGBA8") return AssetTexture::Rgba8;
    return AssetTexture::Unknown;
}

// == ASSET MESH ==========================================================================================================================================================================================

AssetMesh AssetMesh::readInfo(const AssetFile &file) {
    AssetMesh info;

    nlohmann::json metadata = nlohmann::json::parse(file.json.cstr(), nullptr, false);

    info.vbuf_size = metadata["vertex_buf_size"];
    info.ibuf_size = metadata["index_buf_size"];
    info.index_size = metadata["index_size"];
    info.original_file = std__to_strv(metadata["original_file"]);
    info.compression = asset__parse_compression(std__to_strv(metadata["compression"]));
    
    const std::vector<float> &bounds = metadata["bounds"];

    info.bounds = {
        .origin = { bounds[0], bounds[1], bounds[2] },
        .radius = bounds[3],
        .scale  = { bounds[4], bounds[5], bounds[6] },
    };

    return info;
}

void AssetMesh::unpack(Slice<byte> buffer, byte *dest_vbuf, byte *dest_ibuf) {
    arr<char> decompress_buffer;
    decompress_buffer.resize(vbuf_size + ibuf_size);

    LZ4_decompress_safe(
        (const char *)buffer.data(),
        decompress_buffer.data(),
        (int)buffer.size(),
        (int)decompress_buffer.size()
    );

    memcpy(dest_vbuf, decompress_buffer.data(), vbuf_size);
    memcpy(dest_ibuf, decompress_buffer.data() + vbuf_size, ibuf_size);
}

AssetFile AssetMesh::pack(const byte *vertices, const byte *indices) {
    AssetFile file = {
        .type = { 'M', 'E', 'S', 'H' },
        .version = 1,
    };

    nlohmann::json metadata = {
        { "vertex_buf_size", vbuf_size },
        { "index_buf_size", ibuf_size },
        { "index_size", index_size },
        { "original_file", original_file.cstr() },
        { "compression", "LZ4" },
        { "bounds", {
            bounds.origin[0], bounds.origin[1], bounds.origin[2],
            bounds.radius,
            bounds.scale[0], bounds.scale[1], bounds.scale[2]
        }},
    };

    usize full_size = vbuf_size + ibuf_size;
    arr<char> merged;
    merged.resize(full_size);

    memcpy(merged.data(), vertices, vbuf_size);
    memcpy(merged.data() + vbuf_size, indices, ibuf_size);

    int compress_staging = LZ4_compressBound((int)full_size);
    file.blob.resize((usize)compress_staging);

    int compressed_size = LZ4_compress_default(
        merged.data(), 
        (char *)file.blob.data(), 
        (int)merged.size(), 
        (int)file.blob.size()
    );
    
    file.blob.resize(compressed_size);

    file.json = std__to_strv(metadata.dump());

    return file;
}

AssetMesh::Bounds AssetMesh::calculateBounds(Slice<Vertex> vertices) {
    Bounds bounds;

    float min[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
    float max[3] = { FLT_MIN, FLT_MIN, FLT_MIN };

    for (const Vertex &v : vertices) {
        min[0] = math::min(min[0], v.pos[0]);
        min[1] = math::min(min[1], v.pos[1]);
        min[2] = math::min(min[2], v.pos[2]);

        max[0] = math::max(min[0], v.pos[0]);
        max[1] = math::max(min[1], v.pos[1]);
        max[2] = math::max(min[2], v.pos[2]);
    }

	bounds.scale[0] = (max[0] - min[0]) / 2.0f;
	bounds.scale[1] = (max[1] - min[1]) / 2.0f;
	bounds.scale[2] = (max[2] - min[2]) / 2.0f;

	bounds.origin[0] = bounds.scale[0] + min[0];
	bounds.origin[1] = bounds.scale[1] + min[1];
	bounds.origin[2] = bounds.scale[2] + min[2];

	// go through the vertices again to calculate the exact bounding sphere radius
	float r2 = 0;

    for (const Vertex &v : vertices) {
		float offset[3];
		offset[0] = v.pos[0] - bounds.origin[0];
		offset[1] = v.pos[1] - bounds.origin[1];
		offset[2] = v.pos[2] - bounds.origin[2];

		// pithagoras
		float distance = offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2];
		r2 = math::max(r2, distance);
	}

	bounds.radius = sqrtf(r2);

    return bounds;
}
