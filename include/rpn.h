#ifndef RPN_H
#define RPN_H

#include <stddef.h>
#include <stdio.h>
#include "symbol_table.h"

typedef enum {
    RPN_ADD,
    RPN_SUBTRACT,
    RPN_MULTIPLY,
    RPN_DIVIDE,
    RPN_ASSIGN,
    RPN_LESS,
    RPN_GREATER,
    RPN_LESS_EQUAL,
    RPN_GREATER_EQUAL,
    RPN_EQUAL,
    RPN_NOT_EQUAL,
    RPN_BRANCH_FALSE,
    RPN_BRANCH,
    RPN_DUPLICATE,
    RPN_TO_INTEGER,
    RPN_OUTPUT,
    RPN_RETURN
} rpn_operator_t;

typedef enum {
    RPN_SYMBOL,
    RPN_OPERATOR,
    RPN_TARGET,
    RPN_DESTINATION
} rpn_step_kind_t;

typedef struct {
    rpn_step_kind_t kind;
    union {
        const map_node_t *symbol;
        rpn_operator_t operator;
        size_t target;
    } value;
    int source_line;
} rpn_step_t;

typedef struct {
    rpn_step_t *steps;
    size_t count;
    size_t capacity;
} rpn_t;

/* Symbols are borrowed and must outlive the RPN that references them. */
void rpn_init(rpn_t *rpn);
void rpn_free(rpn_t *rpn);

/* Addresses are one-based. Append operations return zero for invalid input. */
size_t rpn_append_symbol(rpn_t *rpn, const map_node_t *symbol);
size_t rpn_append_destination(rpn_t *rpn, const map_node_t *symbol);
size_t rpn_append_operator(rpn_t *rpn, rpn_operator_t operator);
size_t rpn_reserve_target(rpn_t *rpn);
/* Copy a separate sequence, relocating its internal jump targets. */
bool rpn_append_sequence(rpn_t *rpn, const rpn_t *source);

/* A target may point to the first step after the sequence, count + 1. */
bool rpn_patch_target(rpn_t *rpn, size_t address, size_t target);
bool rpn_is_complete(const rpn_t *rpn);

/* Incomplete targets are rejected before writing any output. */
bool rpn_print(FILE *stream, const rpn_t *rpn);

#endif
