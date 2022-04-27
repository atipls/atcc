cmake_minimum_required(VERSION 3.21)
project(atcc C)

set(CMAKE_C_STANDARD 99)

add_executable(atcc
        source/main.c
        source/lexer.c
        source/ati/basic.h
        source/ati/string.c
        source/ati/string.h
        source/ati/utils.c
        source/ati/utils.h
        source/parser.c source/dot.c source/sema.c source/ati/table.c source/ati/table.h source/type.c)


