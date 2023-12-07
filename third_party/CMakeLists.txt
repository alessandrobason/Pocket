find_package(Vulkan REQUIRED)

# Assimp options
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Link assimp statically" FORCE)

# Tracy options
option(TRACY_ENABLE "" ON)
option(TRACY_ON_DEMAND "" ON)

add_subdirectory(assimp)
add_subdirectory(glm)
add_subdirectory(imgui)
add_subdirectory(json)
add_subdirectory(lz4)
add_subdirectory(minicoro)
add_subdirectory(sdl)
add_subdirectory(spirv_reflect)
add_subdirectory(stb_image)
add_subdirectory(tinyobjloader)
add_subdirectory(vkbootstrap)
add_subdirectory(vma)
add_subdirectory(tracy)