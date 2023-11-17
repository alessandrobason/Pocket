add_library(stb_image STATIC)

target_sources(stb_image PRIVATE "stb_image.h" "stb.c")
target_include_directories(stb_image INTERFACE .)