cmake_minimum_required(VERSION 3.21)
project(atcc C)

set(CMAKE_C_STANDARD 99)

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
        source/debug.c
        source/emit/amd64.c
        source/emit/arm64.c
        source/emit/bcdump.c
        source/emit/bytecode.c
        source/emit/bytecode.h
        source/emit/source.c
        source/lexer.c
        source/lower.c
        source/main.c
        source/parser.c
        source/sema.c
        source/type.c
        source/eval.c
        source/emit/tests.c source/ati/tests.c)

target_include_directories(atcc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/source)

