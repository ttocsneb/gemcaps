cmake_minimum_required(VERSION 3.12)

project(GemCaps
    VERSION 0.1.0
)

SET(CMAKE_CXX_STANDARD 17)

FILE(GLOB_RECURSE
    sources ${PROJECT_SOURCE_DIR}/sources/*
)
LIST(FILTER sources EXCLUDE REGEX ".*main.cpp$")
SET(main_sources
    ${PROJECT_SOURCE_DIR}/sources/main.cpp
)

SET(private_includes
    ${PROJECT_SOURCE_DIR}/includes
)
SET(public_includes
    ${PROJECT_SOURCE_DIR}/shared
    ${PROJECT_SOURCE_DIR}/lib/yaml-cpp/include
    ${PROJECT_SOURCE_DIR}/lib/libuv/include
    ${PROJECT_SOURCE_DIR}/lib/wolfssl
    ${PROJECT_SOURCE_DIR}/lib/rll/include/
    ${PROJECT_SOURCE_DIR}/lib/parallel-hashmap/
)

add_subdirectory(${PROJECT_SOURCE_DIR}/lib/yaml-cpp)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/libuv)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/wolfssl)

add_library(gemcaps_src STATIC
    ${sources}
)

add_executable(gemcaps
    ${main_sources}
)

target_include_directories(gemcaps_src PRIVATE ${private_includes})
target_include_directories(gemcaps PRIVATE ${private_includes})
target_include_directories(gemcaps_src PUBLIC ${public_includes})

target_link_libraries(gemcaps_src PUBLIC
    yaml-cpp
    wolfssl
    uv
)

target_link_libraries(gemcaps PRIVATE
    gemcaps_src
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
set_target_properties(gemcaps_src PROPERTIES RUNTIME_OUTPUT_DIRECTORY
	${PROJECT_BINARY_DIR}/bin
)
set_target_properties(gemcaps PROPERTIES RUNTIME_OUTPUT_DIRECTORY
	${PROJECT_BINARY_DIR}/bin
)


include(CTest)

if(CMAKE_TESTING_ENABLED)
    add_subdirectory(${PROJECT_SOURCE_DIR}/test)
endif()
