#ifndef WASM_EMIT_H
#define WASM_EMIT_H
#include <stdbool.h>
/* Called after the shared source and specialization type checks. */
bool wasm_write(const char *path);
#endif
