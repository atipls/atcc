#include "utils.h"
#include <execinfo.h>
#include <stdarg.h>
#include <unistd.h>

void *vector_create_sized(u32 item_size, u32 capacity) {
    usize size = sizeof(VectorHeader) + item_size * capacity;
    VectorHeader *header = (VectorHeader *) calloc(1, size);
    if (!header) return null;

    header->length = 0;
    header->capacity = capacity;
    return header + 1;
}

void *vector_grow_sized(void *vector, u32 increment, u32 item_size) {
    VectorHeader *header = vector_header(vector);

    usize growth = header->capacity * 2;
    usize needed = header->length + increment;
    usize capacity = growth > needed ? growth : needed;

    usize size = sizeof(VectorHeader) + item_size * capacity;
    header = (VectorHeader *) realloc(header, size);

    if (!header) return null;

    header->capacity = capacity;
    return header + 1;
}

Buffer read_file(string path) {
    Buffer buffer = {0};
    cstring cpath = string_to_cstring(path);
    FILE *handle = fopen(cpath, "rb");
    free(cpath);

    if (handle == NULL)
        return buffer;

    fseek(handle, 0, SEEK_END);
    buffer.length = ftell(handle);
    fseek(handle, 0, SEEK_SET);

    buffer.data = (i8 *) malloc(buffer.length);
    fread(buffer.data, 1, buffer.length, handle);
    fclose(handle);

    return buffer;
}

bool write_file(string path, Buffer *buffer) {
    cstring cpath = string_to_cstring(path);
    FILE *handle = fopen(cpath, "wb");
    free(cpath);

    if (handle == NULL)
        return false;

    fwrite(buffer->data, 1, buffer->length, handle);
    fclose(handle);

    return true;
}

void variant_print(Variant variant) {
    switch (variant.kind) {
        case VARIANT_NONE:
            printf("none");
            break;
        case VARIANT_I8:
            printf("%d", variant.i8);
            break;
        case VARIANT_U8:
            printf("%u", variant.u8);
            break;
        case VARIANT_I16:
            printf("%d", variant.i16);
            break;
        case VARIANT_U16:
            printf("%u", variant.u16);
            break;
        case VARIANT_I32:
            printf("%d", variant.i32);
            break;
        case VARIANT_U32:
            printf("%u", variant.u32);
            break;
        case VARIANT_I64:
            printf("%lld", variant.i64);
            break;
        case VARIANT_U64:
            printf("%llu", variant.u64);
            break;
        case VARIANT_F32:
            printf("%f", variant.f32);
            break;
        case VARIANT_F64:
            printf("%f", variant.f64);
            break;
        case VARIANT_STRING:
            printf("%.*s", (int) variant.string.length, variant.string.data);
            break;
        case VARIANT_ARRAY: {
            printf("[");
            for (int i = 0; i < variant.array.length; i++) {
                variant_print(variant.array.data[i]);
                if (i != variant.array.length - 1)
                    printf(", ");
            }
            printf("]");
            break;
        }
    }
}

bool variant_equals(Variant *a, Variant *b) {
    if (!a || !b) return false;

    if (a->kind != b->kind)
        return false;

    switch (a->kind) {
        case VARIANT_NONE: return true;
        case VARIANT_I8: return a->i8 == b->i8;
        case VARIANT_U8: return a->u8 == b->u8;
        case VARIANT_I16: return a->i16 == b->i16;
        case VARIANT_U16: return a->u16 == b->u16;
        case VARIANT_I32: return a->i32 == b->i32;
        case VARIANT_U32: return a->u32 == b->u32;
        case VARIANT_I64: return a->i64 == b->i64;
        case VARIANT_U64: return a->u64 == b->u64;
        case VARIANT_F32: return a->f32 == b->f32;
        case VARIANT_F64: return a->f64 == b->f64;
        case VARIANT_STRING: {
            if (a->string.length != b->string.length)
                return false;

            string s1 = rawstr(a->string.data, a->string.length);
            string s2 = rawstr(b->string.data, b->string.length);
            return string_match(s1, s2);
        }
        case VARIANT_ARRAY: {
            if (a->array.length != b->array.length)
                return false;

            for (int i = 0; i < a->array.length; i++) {
                if (!variant_equals(&a->array.data[i], &b->array.data[i]))
                    return false;
            }

            return true;
        }
    }

    return false;
}

void panicf(const char *file, int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, ANSI_ERROR "PANIC" ANSI_RESET " at %s:%d: ", file, line);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    fflush(stderr);

    // Print the stack trace
    void *array[10];
    size_t size = backtrace(array, 10);
    cstring *symbols = backtrace_symbols(array, size);
    if (symbols) {
        for (int i = 0; i < size; i++)
            fprintf(stderr, "[SYM] %s\n", symbols[i]);
        free(symbols);
    }

    va_end(args);
    exit(1);
}