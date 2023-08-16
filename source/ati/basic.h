#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ANSI_COLORED
#ifdef ANSI_COLORED
#define ANSI_RESET "\033[0m"
#define ANSI_MESSAGE "\033[31m"
#define ANSI_ERROR "\033[36m"
#define ANSI_FILE_PATH "\033[94m"
#else
#define ANSI_RESET ""
#define ANSI_MESSAGE ""
#define ANSI_ERROR ""
#define ANSI_FILE_PATH ""
#endif

#define POINTER_SIZE 8

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
typedef u64 usize;
typedef i64 isize;
#else
typedef u32 usize;
typedef i32 isize;
#endif

typedef float f32;
typedef double f64;

typedef i32 bool;

#define false (0)
#define true (1)

#define null (NULL)

#define make(T) ((T *) calloc(sizeof(T), 1))
#define make_n(T, n) ((T *) calloc(sizeof(T), (n)))

#define panic(...) panicf(__FILE__, __LINE__, __VA_ARGS__)
void __attribute__((noreturn)) panicf(const char *file, int line, const char *format, ...);

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
    VARIANT_STRING,
    VARIANT_ARRAY,
} VariantKind;

typedef struct Variant {
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

        struct {
            i8 *data;
            u64 length;
        } string;

        struct {
            struct Variant *data;
            u64 length;
        } array;
    };
} Variant;

static inline Variant variant_none() { return (Variant){VARIANT_NONE}; }
static inline Variant variant_i8(i8 i) { return (Variant){VARIANT_I8, {.i8 = i}}; }
static inline Variant variant_u8(u8 u) { return (Variant){VARIANT_U8, {.u8 = u}}; }
static inline Variant variant_i16(i16 i) { return (Variant){VARIANT_I16, {.i16 = i}}; }
static inline Variant variant_u16(u16 u) { return (Variant){VARIANT_U16, {.u16 = u}}; }
static inline Variant variant_i32(i32 i) { return (Variant){VARIANT_I32, {.i32 = i}}; }
static inline Variant variant_u32(u32 u) { return (Variant){VARIANT_U32, {.u32 = u}}; }
static inline Variant variant_i64(i64 i) { return (Variant){VARIANT_I64, {.i64 = i}}; }
static inline Variant variant_u64(u64 u) { return (Variant){VARIANT_U64, {.u64 = u}}; }
static inline Variant variant_f32(f32 f) { return (Variant){VARIANT_F32, {.f32 = f}}; }
static inline Variant variant_f64(f64 f) { return (Variant){VARIANT_F64, {.f64 = f}}; }
static inline Variant variant_string(i8 *s, u64 length) { return (Variant){VARIANT_STRING, {.string = {.data = s, .length = length}}}; }
static inline Variant variant_array(Variant *a, u64 length) { return (Variant){VARIANT_ARRAY, {.array = {.data = a, .length = length}}}; }

bool variant_equals(Variant *a, Variant *b);

void variant_print(Variant variant);

void ati_register_utest(void);
