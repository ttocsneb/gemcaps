
FILE(GLOB_RECURSE test_sources
    ${PROJECT_SOURCE_DIR}/test/*
)

add_subdirectory(${PROJECT_SOURCE_DIR}/lib/googletest
    ${PROJECT_BINARY_DIR}/googletest
)

add_executable(test_gemcaps
    ${test_sources}
)

target_link_libraries(test_gemcaps
    gemcaps_src
    gtest
    gtest_main
)

set_target_properties(test_gemcaps PROPERTIES RUNTIME_OUTPUT_DIRECTORY
	${PROJECT_BINARY_DIR}/bin
)

add_test(NAME test_gemcaps
    COMMAND ${PROJECT_BINARY_DIR}/bin/test_gemcaps
)
