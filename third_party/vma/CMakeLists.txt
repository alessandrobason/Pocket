add_library(vma STATIC)

target_sources(vma PRIVATE "vk_mem_alloc.h" "vk_mem_alloc.cc")
target_include_directories(vma INTERFACE .)
target_link_libraries(vma PRIVATE Vulkan::Vulkan pocket_std)