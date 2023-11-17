add_library(tinyobjloader STATIC)

target_sources(tinyobjloader PRIVATE 
    tiny_obj_loader.h
    tiny_obj_loader.cc
)

target_include_directories(tinyobjloader PUBLIC .)