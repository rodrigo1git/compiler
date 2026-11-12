#ifndef WASM_INTERNAL_H
#define WASM_INTERNAL_H

#include <stdio.h>
#include "layout.h"
#include "rpn.h"

#define FRAME_STATIC_LINK_OFFSET 0
#define FRAME_RECEIVER_OFFSET 4
#define FRAME_HEADER_SIZE 8

#define INT16_MIN_WASM (-32768)
#define INT16_MAX_WASM 32767
#define INT16_SIGN_EXTEND_SHIFT 16
#define FLOAT32_MAX_ABS_STR "3.40282347e38"

typedef enum {
    WASM_ERR_FLOAT_OVERFLOW     = 1,
    WASM_ERR_INT_OVERFLOW       = 2,
    WASM_ERR_FRACTIONAL_TRUNC   = 3,
    WASM_ERR_FLOAT_TO_INT_RANGE = 4,
    WASM_ERR_INVALID_FIELD      = 5,
    WASM_ERR_RECURSIVE_CALL     = 6,
    WASM_ERR_OUT_OF_MEMORY      = 8
} wasm_runtime_error_t;

typedef struct {
    const rpn_t *code;
    map_node_t *symbol;
    layout_size_t size;
    layout_size_t *scratch;
    size_t *controls;
} body_t;

typedef struct {
    FILE *out;
    layout_t layout;
    body_t *bodies;
    size_t count;
} wasm_module_t;

void wasm_emit_line(FILE *out, int indent, const char *format, ...);
void wasm_emit_block(FILE *out, int indent, const char *format, ...)
    __attribute__((format(printf, 3, 4)));
void wasm_emit_runtime(wasm_module_t *mod);
void wasm_prepare_bodies(wasm_module_t *mod);
void wasm_write_body(wasm_module_t *mod, body_t *body);

#endif /* WASM_INTERNAL_H */
