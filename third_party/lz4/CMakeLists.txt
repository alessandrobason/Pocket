add_library(lz4 STATIC)

target_include_directories(lz4 PUBLIC .)
target_sources(lz4 PRIVATE "lz4.h" "lz4.c")