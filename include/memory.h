#ifndef MEMORY_H
#define MEMORY_H
#include <stddef.h>
size_t memory_add(size_t left, size_t right);
size_t memory_multiply(size_t count, size_t size);
void *memory_resize(void *pointer, size_t bytes);
void *memory_zero(size_t count, size_t size);
#endif
