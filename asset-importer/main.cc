#include <filesystem>
#include <unordered_map>
#include <string>
#include <iostream>

#include "std/logging.h"
#include "std/file.h"
#include "formats/assets.h"

namespace fs = std::filesystem;

enum class AssetType {
    Error, 
    Ignore,
    File, 
    Texture, 
    Mesh, 
    Material
};

std::unordered_map<fs::path, AssetType> type_map = {
    // texture types
    { ".png",  AssetType::Texture },
    { ".jpg",  AssetType::Texture },
    { ".jpeg", AssetType::Texture },

    // mesh types
    { ".obj",  AssetType::Mesh },
    { ".fbx",  AssetType::Mesh },
    { ".gltf", AssetType::Mesh },

    // material types
    { ".mtl",  AssetType::Material },

    // ignore types
    { ".txt",  AssetType::Ignore },
    { ".vert", AssetType::Ignore },
    { ".frag", AssetType::Ignore },
};

static fs::path base_path;

static AssetType getAssetType(const fs::path &ext);
static bool shouldReplace(const fs::path &in, const fs::path &out);
static void convertFile(const fs::path &fname);
static void convertImage(const fs::path &fname);
static void convertMesh(const fs::path &fname);
static void convertMaterial(const fs::path &fname);

void run(fs::path path) {
    for (auto &p : fs::directory_iterator(path)) {
        if (!fs::is_regular_file(p)) {
            continue;
        }

        info("File: %S", p.path().filename().c_str());

        switch (getAssetType(p.path().extension())) {
            case AssetType::File:     convertFile(p); break;
            case AssetType::Texture:  convertImage(p); break;
            case AssetType::Mesh:     convertMesh(p); break;
            case AssetType::Material: convertMaterial(p); break;
            case AssetType::Ignore:   break;
            default: err("unrecognized file type %S", p.path().c_str()); break;
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        err("usage: importer <folder>");
        //return 1;
    }

    try {
        //base_path = argv[1];
        base_path = "../../assets";
        fs::current_path(base_path);
        fs::create_directory("imported");

        run(".");
    }
    catch (std::exception &e) {
        err("exception: %s", e.what());
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static AssetType getAssetType(const fs::path &ext) {
    auto it = type_map.find(ext);
    if (it == type_map.end()) {
        warn("returing generic file for %S", ext.c_str());
        return AssetType::File;
    }
    return it->second;
}

static bool shouldReplace(const fs::path &in, const fs::path &out) {
    return fs::last_write_time(in) > fs::last_write_time(out);
}

static void convertFile(const fs::path &fname) {
    std::cout << "converting generic file " << fname.filename() << "\n";
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stb_image.h>

static void convertImage(const fs::path &fname) {
    fs::path out = fs::path("imported") / fname.filename();
    out.replace_extension(".tx");

    if (fs::exists(out) && !shouldReplace(fname, out)) {
        info("no need to convert file, input is older than output");
        return;
    }

    int x, y, n;
    stbi_uc *pixels = stbi_load(fname.string().c_str(), &x, &y, &n, STBI_rgb_alpha);

    if (!pixels) {
        err("failed to load texture file %S", fname.c_str());
        return;
    }

    AssetTexture info = {
        .byte_size = (usize)(x * y * 4),
        .format = AssetTexture::Rgba8,
        .pixel_size = { (u64)x, (u64)y, 1 },
        .original_file = fname.filename().string().c_str(),
    };

    AssetFile image = info.pack(pixels);

    stbi_image_free(pixels);

    if (!image.save(out.string().c_str())) {
        err("could not save packed texture %S", fname.c_str());
    }

    info("converted %S to %S", fname.filename().c_str(), out.c_str());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <assimp/importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

using namespace glm;

struct Vertex {
	vec3 pos;
	vec3 norm;
	vec3 col;
	vec2 uv;
};

struct Mesh {
    arr<Vertex> verts;
    arr<u8>  ind8;
    arr<u16> ind16;
    arr<u32> ind32;
    aiAABB bounding;
};

template<typename T>
void addIndices(Slice<aiFace> faces, arr<T> &indices) {
    for (const aiFace &f : faces) {
        for (uint i = 0; i < f.mNumIndices; ++i) {
            indices.push(f.mIndices[i]);
        }
    }
}

static void processMesh(aiMesh *mesh, const aiScene *scene, Mesh &out_mesh) {
    for (uint i = 0; i < mesh->mNumVertices; ++i) {
        const aiVector3D &v = mesh->mVertices[i];
        const aiVector3D &n = mesh->mNormals[i];

        aiVector3D t = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);
        aiColor4D c = mesh->mColors[0] ? mesh->mColors[0][i] : aiColor4D(1, 1, 1, 1);

        out_mesh.verts.push(Vertex{
            .pos = { v.x, v.y, v.z },
            .norm = { n.x, n.y, n.z },
            .col = { c.r, c.g, c.b },
            .uv = { t.x, t.y },
        });
    }

    Slice<aiFace> faces = { mesh->mFaces, mesh->mNumFaces };

#if 0
    u32 total_count = 0;

    for (const aiFace &f : faces) {
        total_count += f.mNumIndices;
    }

    if (total_count < INT8_MAX) {
        addIndices(faces, out_mesh.ind8);
    }
    else if (total_count < INT16_MAX) {
        addIndices(faces, out_mesh.ind16);
    }
    else if (total_count < INT32_MAX) {
        addIndices(faces, out_mesh.ind32);
    }
    else {
        fatal("too many indices!: %u", total_count);
    }
#endif

    addIndices(faces, out_mesh.ind32);

    out_mesh.bounding.mMin = math::min(out_mesh.bounding.mMin, mesh->mAABB.mMin);
    if (out_mesh.bounding.mMax < mesh->mAABB.mMax) {
        out_mesh.bounding.mMax = mesh->mAABB.mMax;
    }
}

static void processNode(aiNode *node, const aiScene *scene, Mesh &out_mesh) {
    for (uint i = 0; i < node->mNumMeshes; ++i) {
        processMesh(scene->mMeshes[node->mMeshes[i]], scene, out_mesh);
    } 

    for (uint i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], scene, out_mesh);
    }
}

static void convertMesh(const fs::path &fname) {
    fs::path out = fs::path("imported") / fname.filename();
    out.replace_extension(".mesh");

    if (fname != ".\\triangle.obj" && fs::exists(out) && !shouldReplace(fname, out)) {
        info("no need to convert file, input is older than output");
        return;
    }

    arr<byte> data = File::readWhole(fname.string().c_str());

    Assimp::Importer importer;
    uint import_flags = 
        aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_ImproveCacheLocality | 
        aiProcess_RemoveRedundantMaterials | aiProcess_OptimizeMeshes | aiProcess_OptimizeMeshes |
        aiProcess_FlipUVs | aiProcess_GenBoundingBoxes;
    const aiScene *scene = importer.ReadFile(fname.string(), import_flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        err("assimp error: %s", importer.GetErrorString());
        return;
    }

    Mesh mesh = {};

    processNode(scene->mRootNode, scene, mesh);

    for (u32 i : mesh.ind32) {
        info("%u", i);
    }

    Slice<byte> indices;
    u8 index_size = 0;

    if (!mesh.ind8.empty()) {
        indices = { (byte *)mesh.ind8.data(), mesh.ind8.byteSize() };
        index_size = sizeof(u8);
    }
    else if (!mesh.ind16.empty()) {
        indices = { (byte *)mesh.ind16.data(), mesh.ind16.byteSize() };
        index_size = sizeof(u16);
    }
    else if (!mesh.ind32.empty()) {
        indices = { (byte *)mesh.ind32.data(), mesh.ind32.byteSize() };
        index_size = sizeof(u32);
    }

    assert(((int)mesh.ind8.empty() + (int)mesh.ind16.empty() + (int)mesh.ind32.empty()) == 2);
    assert(index_size == sizeof(u32));

    aiVector3D scale = (mesh.bounding.mMax - mesh.bounding.mMin) / 2.f;
    aiVector3D origin = scale + mesh.bounding.mMin;

    AssetMesh info = {
        .vbuf_size = mesh.verts.byteSize(),
        .ibuf_size = mesh.ind8.byteSize() + mesh.ind16.byteSize() + mesh.ind32.byteSize(),
        .bounds = {
            .origin = { origin.x, origin.y, origin.z },
            .scale = { scale.x, scale.y, scale.z },
        },
        .index_size = index_size,
        .compression = Compression::Lz4,
        .original_file = fname.filename().string().c_str(),
    };

    AssetFile mesh_file = info.pack((byte *)mesh.verts.data(), indices.data());

    if (!mesh_file.save(out.string().c_str())) {
        err("could not save packed mesh %S", fname.filename().c_str());
    }

    info("converted %S to %S", fname.filename().c_str(), out.c_str());
}

static void convertMaterial(const fs::path &fname) {
    std::cout << "converting material " << fname.filename() << "\n";
}
