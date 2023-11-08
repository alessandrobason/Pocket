
# Add source to this project's executable.
add_executable(vulkan_guide
    main.cpp
    utils.cpp
    utils.h
    mesh.h
    mesh.cpp
    vm_mem_alloc.cpp
    vk_engine.cpp
    vk_engine.h
    vk_types.h
    vk_initializers.cpp
    vk_initializers.h
    vk_textures.h
    vk_textures.cpp
)


set_property(TARGET vulkan_guide PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_guide>")

target_include_directories(vulkan_guide PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vulkan_guide PRIVATE vkbootstrap vma glm tinyobjloader imgui stb_image)

#target_link_libraries(vulkan_guide Vulkan::Vulkan sdl2)
# target_link_libraries(vulkan_guide PRIVATE SDL3::SDL3)

add_dependencies(vulkan_guide Shaders)
