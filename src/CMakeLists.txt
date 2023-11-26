file(GLOB PK_STD   "std/*.h"     "std/*.cc")
file(GLOB PK_FMT   "formats/*.h" "formats/*.cc")
file(GLOB PK_GFX   "gfx/*.h"     "gfx/*.cc")
file(GLOB PK_UTILS "utils/*.h"   "utils/*.cc")
file(GLOB PK_CORE  "core/*.h"    "core/*.cc")
file(GLOB PK_SRC   "*.h"         "*.cc")

add_library(pocket_std STATIC)
target_sources(pocket_std PRIVATE ${PK_STD})
target_include_directories(pocket_std PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(pocket_formats STATIC)
target_sources(pocket_formats PRIVATE ${PK_FMT})
target_include_directories(pocket_formats PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pocket_formats PRIVATE pocket_std json lz4)

# Add source to this project's executable.
add_executable(pocket
    ${PK_GFX}
    ${PK_UTILS}
	${PK_CORE}
	${PK_SRC}
    "../pocket.natvis"
)

source_group("std"     FILES ${PK_STD})
source_group("formats" FILES ${PK_FMT})
source_group("gfx"     FILES ${PK_GFX})
source_group("utils"   FILES ${PK_UTILS})
source_group("core"    FILES ${PK_CORE})
source_group("src"     FILES ${PK_SRC})

set_property(TARGET pocket PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:pocket>")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT pocket)
set_property(TARGET pocket PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

target_include_directories(pocket PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pocket PRIVATE pocket_std pocket_formats vkbootstrap vma glm tinyobjloader imgui stb_image spirv-reflect)

add_dependencies(pocket Shaders)
