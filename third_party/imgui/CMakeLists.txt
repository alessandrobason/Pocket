add_library(imgui STATIC)

file(GLOB IMGUI_SRC "*.h" "*.cpp")

target_sources(imgui PRIVATE ${IMGUI_SRC})
target_include_directories(imgui PUBLIC .)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan sdl2)
