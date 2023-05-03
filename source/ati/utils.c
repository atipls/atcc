#include "utils.h"

void *vector_growf(void *arr, int increment, int itemsize) {
    int natural_capacity = arr ? 2 * vector_raw_cap(arr) : 0;
    int space_needed = vector_len(arr) + increment;
    int new_capacity = natural_capacity > space_needed ? natural_capacity : space_needed;
    int *reallocated_data = (int *) realloc(arr ? vector_raw(arr) : 0, itemsize * new_capacity + sizeof(int) * 2);
    if (!reallocated_data) return null;
    if (!arr)
        reallocated_data[1] = 0;
    reallocated_data[0] = new_capacity;
    return reallocated_data + 2;
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
