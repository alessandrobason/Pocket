add_library(vkbootstrap STATIC)

target_sources(vkbootstrap PRIVATE 
    VkBootstrap.h
    VkBootstrap.cpp
)

target_include_directories(vkbootstrap PUBLIC .)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)