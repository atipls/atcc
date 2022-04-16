#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef i32 bool;

#define false (0)
#define true (1)

#define null (NULL)

#define make(T) ((T *) calloc(sizeof(T), 1))

typedef struct Buffer {
    i8 *data;
    u64 length;
} Buffer;

#define buffer_free(buffer) free((buffer).data)