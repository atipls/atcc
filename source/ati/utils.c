#include "utils.h"

void *vector_growf(void *arr, int increment, int itemsize) {
    int dbl_cur = arr ? 2 * vector_raw_cap(arr) : 0;
    int min_needed = vector_len(arr) + increment;
    int m = dbl_cur > min_needed ? dbl_cur : min_needed;
    int *p = (int *) realloc(arr ? vector_raw(arr) : 0, itemsize * m + sizeof(int) * 2);
    if (p) {
        if (!arr)
            p[1] = 0;
        p[0] = m;
        return p + 2;
    } else {
        return (void *) (2 * sizeof(int));// try to force a NULL pointer exception later
    }
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