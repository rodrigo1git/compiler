#ifndef MEMORY_H
#define MEMORY_H
#include <stddef.h>
void *memory_resize(void *pointer, size_t bytes);
void *memory_zero(size_t count, size_t size);
#endif
