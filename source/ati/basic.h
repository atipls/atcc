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
#define make_n(T, n) ((T *) calloc(sizeof(T), (n)))

typedef struct Buffer {
    i8 *data;
    u64 length;
} Buffer;

#define buffer_free(buffer) free((buffer).data)

typedef enum {
    VARIANT_NONE,
    VARIANT_I8,
    VARIANT_U8,
    VARIANT_I16,
    VARIANT_U16,
    VARIANT_I32,
    VARIANT_U32,
    VARIANT_I64,
    VARIANT_U64,
    VARIANT_F32,
    VARIANT_F64,
} VariantKind;

typedef struct {
    VariantKind kind;
    union {
        i8 i8;
        u8 u8;
        i16 i16;
        u16 u16;
        i32 i32;
        u32 u32;
        i64 i64;
        u64 u64;
        f32 f32;
        f64 f64;
    };
} Variant;