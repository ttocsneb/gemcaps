cmake_minimum_required(VERSION 3.12)

project(GemCaps
    VERSION 0.1.0
)

# file(GLOB_RECURSE
#     sources ${PROJECT_SOURCE_DIR}/sources/*
# )

set(sources
    ${PROJECT_SOURCE_DIR}/sources/server.cpp
    ${PROJECT_SOURCE_DIR}/sources/util.cpp
    ${PROJECT_SOURCE_DIR}/sources/uvutils.cpp
)

SET(CMAKE_CXX_STANDARD 17)

add_subdirectory(${PROJECT_SOURCE_DIR}/lib/yaml-cpp)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/libuv)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/wolfssl)

add_executable(gemcaps
    ${sources}
)

target_include_directories(gemcaps PRIVATE
    ${PROJECT_SOURCE_DIR}/includes
)

target_include_directories(gemcaps PUBLIC
    ${PROJECT_SOURCE_DIR}/shared
    ${PROJECT_SOURCE_DIR}/lib/yaml-cpp/include
    ${PROJECT_SOURCE_DIR}/lib/libuv/include
    ${PROJECT_SOURCE_DIR}/lib/wolfssl
    ${PROJECT_SOURCE_DIR}/lib/rll/include/
    ${PROJECT_SOURCE_DIR}/lib/parallel-hashmap/
)

target_link_libraries(gemcaps PUBLIC
    yaml-cpp
    wolfssl
    uv
)

set_target_properties(uv PROPERTIES RUNTIME_OUTPUT_DIRECTORY
	${PROJECT_BINARY_DIR}/bin
)
set_target_properties(yaml-cpp PROPERTIES RUNTIME_OUTPUT_DIRECTORY
	${PROJECT_BINARY_DIR}/bin
)
set_target_properties(wolfssl PROPERTIES RUNTIME_OUTPUT_DIRECTORY
	${PROJECT_BINARY_DIR}/bin
)
set_target_properties(gemcaps PROPERTIES RUNTIME_OUTPUT_DIRECTORY
	${PROJECT_BINARY_DIR}/bin
)
