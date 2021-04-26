cmake_minimum_required(VERSION 3.12)

project(GemCaps
    VERSION 0.1.0
)

file(GLOB_RECURSE
    sources ./sources/*
)

SET(CMAKE_CXX_STANDARD 17)

add_subdirectory(${PROJECT_SOURCE_DIR}/lib/benstools)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/yaml-cpp)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/libuv)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/wolfssl)

add_executable(gemcaps
    ${sources}
)

target_include_directories(gemcaps PUBLIC
    ${PROJECT_SOURCE_DIR}/includes
    ${PROJECT_SOURCE_DIR}/lib/yaml-cpp/include
    ${PROJECT_SOURCE_DIR}/lib/libuv/include
    ${PROJECT_SOURCE_DIR}/lib/wolfssl
)

target_link_libraries(gemcaps PUBLIC
    pthread
    benstools
    yaml-cpp
    wolfssl
    uv
)
