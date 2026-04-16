cmake_minimum_required(VERSION 3.14)
project(tBMP VERSION 1.1.0 LANGUAGES C)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wshadow -Wcast-align -Wstrict-prototypes
        -Wmissing-prototypes -Wdouble-promotion
        -Wformat=2 -Wundef
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-O0 -g3)
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-O2 -g)
    else()
        add_compile_options(-O2)
    endif()
endif()

# Library target
add_library(tbmp STATIC
    src/tbmp_reader.c
    src/tbmp_pixel.c
    src/tbmp_decode.c
    src/tbmp_rotate.c
    src/tbmp_write.c
    src/tbmp_meta.c
    src/tbmp_msgpack.c
)

target_include_directories(tbmp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(tbmp PUBLIC m)

# Options to opt into non-library targets
option(TBMP_BUILD_TESTS "Build the test suite" OFF)
option(TBMP_BUILD_TOOLS "Build the CLI converter tool" OFF)

# enable_testing() must be called at the top level so CTest can discover tests
enable_testing()

# Test executable
if(TBMP_BUILD_TESTS)
    add_executable(tbmp_test
        tests/tbmp_test_main.c
        tests/test_reader.c
        tests/test_pixel.c
        tests/test_decode.c
        tests/test_rotate.c
        tests/test_write.c
        tests/test_edge_cases.c
        tests/test_meta.c
    )
    target_link_libraries(tbmp_test PRIVATE tbmp)

    add_test(NAME tbmp_test COMMAND tbmp_test)
endif()

# CLI tool
if(TBMP_BUILD_TOOLS)
    add_executable(tbmp_cli
        tools/tbmp.c
        tools/src/tbmp_imgio.c
        tools/src/tbmp_meta_cli.c
        tools/src/tbmp_ui.c
    )
    set_target_properties(tbmp_cli PROPERTIES OUTPUT_NAME tbmp)
    target_include_directories(tbmp_cli PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tools
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/vendor
    )
    target_link_libraries(tbmp_cli PRIVATE tbmp)
endif()

# Install rules
install(TARGETS tbmp ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
