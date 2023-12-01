add_library(minicoro STATIC)

target_sources(minicoro PRIVATE "minicoro.h" "minicoro.c")
target_include_directories(minicoro INTERFACE .)