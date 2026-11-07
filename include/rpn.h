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
    RPN_RETURN,
    RPN_DROP
} rpn_operator_t;

typedef enum {
    RPN_SYMBOL,
    RPN_OPERATOR,
    RPN_TARGET,
    RPN_DESTINATION,
    RPN_CALL_FRAME,
    RPN_ARGUMENT_DESTINATION,
    RPN_CALL,
    RPN_ATTRIBUTE,
    RPN_ATTRIBUTE_DESTINATION,
    RPN_OBJECT_VALUE,
    RPN_INDEX_CHECK
} rpn_step_kind_t;

typedef struct {
    rpn_step_kind_t kind;
    union {
        const map_node_t *symbol;
        rpn_operator_t operator;
        size_t target;
    } value;
    int source_line;
    const map_node_t *receiver;
    const map_node_t *index;
    size_t control_start;
    bool loop;
    const map_node_t *loop_variable;
    const map_node_t *capture;
} rpn_step_t;

typedef struct {
    rpn_step_t *steps;
    size_t count;
    size_t capacity;
} rpn_t;

typedef struct {
    size_t start;
    size_t branch;
    size_t false_start;
    size_t body_end;
    size_t end;
    bool loop;
    bool has_else;
} rpn_control_t;

/* Build after completing a body; caller owns the returned position index. */
size_t *rpn_index_controls(const rpn_t *body);
rpn_control_t rpn_describe_control(const rpn_t *input, size_t start, size_t branch);

void rpn_init(rpn_t *rpn);
void rpn_free(rpn_t *rpn);

size_t rpn_append_symbol(rpn_t *rpn, const map_node_t *symbol);
size_t rpn_append_destination(rpn_t *rpn, const map_node_t *symbol);
size_t rpn_append_operator(rpn_t *rpn, rpn_operator_t operator);
size_t rpn_append_step(rpn_t *rpn, rpn_step_t step);
size_t rpn_reserve_target(rpn_t *rpn);
bool rpn_append_sequence(rpn_t *rpn, const rpn_t *source);

bool rpn_patch_target(rpn_t *rpn, size_t address, size_t target);
bool rpn_is_complete(const rpn_t *rpn);

bool rpn_print(FILE *stream, const rpn_t *rpn);

#endif
