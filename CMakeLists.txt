cmake_minimum_required(VERSION 3.21)
project(atcc C CXX)

set(CMAKE_C_STANDARD 99)

if (APPLE)
    set(LLVM_DIR /opt/homebrew/opt/llvm/lib/cmake/llvm)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(WITH_UTESTS)
    set(TEST_SOURCES source/tests/tests-ati.c source/tests/tests-bc.c)
endif ()

add_executable(atcc
        source/ati/basic.h
        source/ati/config.c
        source/ati/config.h
        source/ati/string.c
        source/ati/string.h
        source/ati/table.c
        source/ati/table.h
        source/ati/utest.c
        source/ati/utest.h
        source/ati/utils.c
        source/ati/utils.h
        source/emit/amd64.c
        source/emit/arm64.c
        source/emit/bcdump.c
        source/emit/bytecode.c
        source/emit/bytecode.h
        source/emit/llvm.c
        source/emit/source.c
        source/debug.c
        source/eval.c
        source/lexer.c
        source/lower.c
        source/main.c
        source/parser.c
        source/sema.c
        source/type.c

        ${TEST_SOURCES}
        )


find_package(LLVM CONFIG)

if (LLVM_FOUND)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

    target_include_directories(atcc PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(atcc PRIVATE ${LLVM_DEFINITIONS})
    target_link_directories(atcc PRIVATE ${LLVM_LIBRARY_DIRS})

    target_compile_definitions(atcc PRIVATE -DUSE_LLVM)
    target_link_libraries(atcc LLVM)
else ()
    message(STATUS "LLVM not found")
endif ()

target_include_directories(atcc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/source)

