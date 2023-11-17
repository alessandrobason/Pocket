set(CMAKE_CXX_STANDARD 20)

add_executable(asset-importer "main.cc")

target_include_directories(asset-importer PUBLIC "${CMAKE_CURRENT_SOUCE_DIR}")
target_link_libraries(asset-importer PUBLIC pocket_std pocket_formats stb_image json lz4 assimp glm)