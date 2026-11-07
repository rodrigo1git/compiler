#include <stdio.h>
#include <stdlib.h>
#include "../include/memory.h"

void *memory_resize(void *pointer, size_t bytes) {
    void *result = realloc(pointer, bytes ? bytes : 1);
    if (!result) { fprintf(stderr, "Cannot allocate compiler memory\n"); exit(1); }
    return result;
}

void *memory_zero(size_t count, size_t size) {
    void *result = calloc(count ? count : 1, size ? size : 1);
    if (!result) { fprintf(stderr, "Cannot allocate compiler memory\n"); exit(1); }
    return result;
}
