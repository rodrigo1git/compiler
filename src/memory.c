#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../include/memory.h"

size_t memory_add(size_t left, size_t right) {
    if (right > SIZE_MAX - left) {
        fprintf(stderr, "Compiler allocation size overflow\n"); exit(1);
    }
    return left + right;
}

size_t memory_multiply(size_t count, size_t size) {
    if (size && count > SIZE_MAX / size) {
        fprintf(stderr, "Compiler allocation size overflow\n"); exit(1);
    }
    return count * size;
}

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
