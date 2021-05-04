cmake_minimum_required(VERSION 3.12)

project(GemCaps
    VERSION 0.1.0
)

file(GLOB_RECURSE
    sources ${PROJECT_SOURCE_DIR}/sources/*
)

SET(CMAKE_CXX_STANDARD 17)

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
    yaml-cpp
    wolfssl
    uv
)

set_target_properties(uv PROPERTIES RUNTIME_OUTPUT_DIRECTORY
	${PROJECT_BINARY_DIR}/bin
)
set_target_properties(gemcaps PROPERTIES RUNTIME_OUTPUT_DIRECTORY
	${PROJECT_BINARY_DIR}/bin
)



include(CTest)

if (WIN32)
else()
if(CMAKE_TESTING_ENABLED)
    list(FILTER sources EXCLUDE REGEX
        ".*main.cpp$"
    )
    file(GLOB_RECURSE
        tests ${PROJECT_SOURCE_DIR}/test/*
    )

    add_subdirectory(${PROJECT_SOURCE_DIR}/lib/googletest)

    add_executable(test_gemcaps
        ${sources}
        ${tests}
    )
    target_include_directories(test_gemcaps PUBLIC
        ${PROJECT_SOURCE_DIR}/includes
        ${PROJECT_SOURCE_DIR}/lib/yaml-cpp/include
        ${PROJECT_SOURCE_DIR}/lib/libuv/include
        ${PROJECT_SOURCE_DIR}/lib/wolfssl
        ${PROJECT_SOURCE_DIR}/lib/googletest/include
    )
    target_link_libraries(test_gemcaps PUBLIC
        yaml-cpp
        wolfssl
        uv
        gtest
        gtest_main
    )

    add_test(NAME test_gemcaps
        COMMAND ${PROJECT_BINARY_DIR}/test_gemcaps
    )
endif()
endif()
