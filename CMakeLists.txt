﻿cmake_minimum_required (VERSION 3.8)

project("pocket")

string(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}") 
message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}\n") 
# add_compile_options(-fsanitize=address) 

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
# find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)

add_subdirectory(third_party)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

add_subdirectory(src)
add_subdirectory(asset-importer)

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

## find all the shader files under the shaders folder
file(GLOB_RECURSE GLSL_SOURCE_FILES
  "${PROJECT_SOURCE_DIR}/assets/shaders/*.frag"
  "${PROJECT_SOURCE_DIR}/assets/shaders/*.vert"
  "${PROJECT_SOURCE_DIR}/assets/shaders/*.comp"
  "${PROJECT_SOURCE_DIR}/assets/shaders/*.mesh"
)

## iterate each shader
foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/assets/shaders/spv/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  ##execute glslang command to compile that specific shader
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
  Shaders 
  DEPENDS ${SPIRV_BINARY_FILES}
)
