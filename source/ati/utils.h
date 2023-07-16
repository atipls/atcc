#pragma once

#include "basic.h"
#include "string.h"

typedef struct VectorHeader {
    usize length;
    usize capacity;
} VectorHeader;

#define vector_header(vector) ((VectorHeader *) (vector) -1)

#define vector_create(Type) ((Type *) vector_create_sized(sizeof(Type), 32))
#define vector_create_n(Type, n) ((Type *) vector_create_sized(sizeof(Type), n))

#define vector_length(vector) (vector_header(vector)->length)
#define vector_capacity(vector) (vector_header(vector)->capacity)

#define vector_push(vector, value)                 \
    do {                                           \
        vector_ensure_length(vector, 1);           \
        (vector)[vector_length(vector)++] = value; \
    } while (0)

#define vector_add(vector, n) (vector_ensure_length(vector, n), vector_header(vector)->length += (n), &(vector)[vector_length(vector) - (n)])

#define vector_last(vector) ((vector)[vector_length(vector) - 1])

#define vector_free(vector) \
    do { free(vector_header(vector)) } while (0)

#define vector_needs_grow(vector, n) (vector_header(vector)->length + (n) >= vector_header(vector)->capacity)
#define vector_ensure_length(vector, length) (vector_needs_grow(vector, (length)) ? vector_grow(vector, length) : 0)

#define vector_grow(vector, n) (*((void **) &(vector)) = vector_grow_sized((vector), (n), sizeof(*(vector))))

#define vector_foreach(Type, name, vec) for (Type *name = (vec), *name##_end = ((vec) + vector_length(vec)); (size_t) name != (size_t) name##_end; ++name)
#define vector_foreach_ptr(Type, name, vec) for (Type **name = (vec), **name##_end = ((vec) + vector_length(vec)); (size_t) name != (size_t) name##_end; ++name)

void *vector_create_sized(u32 item_size, u32 capacity);
void *vector_grow_sized(void *vector, u32 increment, u32 item_size);

#define array_length(array) (sizeof(array) / sizeof(array[0]))

Buffer read_file(string path);
bool write_file(string path, Buffer *buffer);
