file(GLOB PK_STD "std/*.h" "std/*.cc")
file(GLOB PK_FMT "formats/*.h" "formats/*.cc")
file(GLOB PK_SRC "*.h" "*.cpp" "*.cc")

# Add source to this project's executable.
add_executable(vulkan_guide
    ${PK_STD}
    ${PK_FMT}
	${PK_SRC}
)

source_group("std" FILES ${PK_STD})
source_group("formats" FILES ${PK_FMT})
source_group("src" FILES ${PK_SRC})

set_property(TARGET vulkan_guide PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_guide>")

target_include_directories(vulkan_guide PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vulkan_guide PRIVATE vkbootstrap vma glm tinyobjloader imgui stb_image)

#target_link_libraries(vulkan_guide Vulkan::Vulkan sdl2)
# target_link_libraries(vulkan_guide PRIVATE SDL3::SDL3)

add_dependencies(vulkan_guide Shaders)
