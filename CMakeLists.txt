cmake_minimum_required(VERSION 3.12)

project(GemCaps
    VERSION 0.1.0
)

file(GLOB_RECURSE
    sources ./sources/*
)

add_subdirectory(${PROJECT_SOURCE_DIR}/lib/benstools)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/yaml-cpp)

find_library(LIB_WOLFSSL wolfssl
    REQUIRED
)

add_executable(gemcaps
    ${sources}
)

target_include_directories(gemcaps PUBLIC
    ./includes
    ${PROJECT_SOURCE_DIR}/lib/yaml-cpp/include
)

target_link_libraries(gemcaps PUBLIC
    pthread
    benstools
    yaml-cpp
    wolfssl
)
