#pragma once

#include "basic.h"
#include "string.h"

#define vector_push(a, v) (vector_maybe_grow(a, 1), (a)[vector_raw_len(a)++] = (v))
#define vector_len(a) ((a) ? vector_raw_len(a) : 0)
#define vector_cap(a) ((a) ? vector_raw_len(a) : 0)
#define vector_add(a, n) (vector_maybe_grow(a, n), vector_raw_len(a) += (n), &(a)[vector_raw_len(a) - (n)])
#define vector_last(a) ((a)[vector_raw_len(a) - 1])
#define vector_at(a, i) ((&(a))[i])
#define vector_free(a) ((a) ? free(vector_raw(a)), 0 : 0)

#define vector_raw(a) ((i32 *) (void *) (a) -2)
#define vector_raw_cap(a) vector_raw(a)[0]
#define vector_raw_len(a) vector_raw(a)[1]

#define vector_needs_grow(a, n) ((a) == 0 || vector_raw_len(a) + (n) >= vector_raw_cap(a))
#define vector_maybe_grow(a, n) (vector_needs_grow(a, (n)) ? vector_grow(a, n) : 0)
#define vector_grow(a, n) (*((void **) &(a)) = vector_growf((a), (n), sizeof(*(a))))

#define vector_foreach(Type, name, vec) for (Type *name = (vec), *name##_end = ((vec) + vector_len(vec)); (size_t)name != (size_t)name##_end; ++name)
#define vector_foreach_ptr(Type, name, vec) for (Type **name = (vec), **name##_end = ((vec) + vector_len(vec)); (size_t)name != (size_t)name##_end; ++name)

#define Vec(Type) Type*

#define array_length(a) (sizeof(a) / sizeof(a[0]))

void *vector_growf(void *arr, i32 increment, i32 itemsize);

Buffer read_file(string path);
bool write_file(string path, Buffer *buffer);