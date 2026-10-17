#ifndef CODEGEN_H
#define CODEGEN_H

#include "rpn.h"

typedef struct codegen_function codegen_function_t;

typedef struct {
    map_node_t *variable;
    map_node_t *step;
    size_t condition_target;
    size_t exit_target;
    rpn_operator_t update_operator;
    int source_line;
} codegen_loop_t;

void codegen_init(bool enabled);
void codegen_destroy(void);
bool codegen_print(FILE *stream);
void codegen_unsupported(int line, const char *feature);

/* Expression fragments are owned; combining or emitting consumes its inputs. */
rpn_t *codegen_value(map_node_t *symbol, int line);
rpn_t *codegen_unary(rpn_t *value, rpn_operator_t operator, int line);
rpn_t *codegen_binary(rpn_t *left, rpn_t *right, rpn_operator_t operator, int line);
rpn_t *codegen_target(rpn_t *targets, map_node_t *symbol, int line);
rpn_t *codegen_assignment_expression(rpn_t *targets, rpn_t *value);
void codegen_assignment(map_node_t *target, rpn_t *value, int line);
void codegen_output(rpn_t *value, int line);
void codegen_return(rpn_t *value, int line);
/* Function bodies are retained until destruction; guards restore the enclosing body. */
codegen_function_t *codegen_function_begin(map_node_t *symbol, int line);
void codegen_function_end(codegen_function_t *function);
void codegen_function_abort(codegen_function_t *function);
/* Pending jump addresses live on the parser stack until their IF is reduced. */
size_t codegen_if_begin(rpn_t *condition, int line);
size_t codegen_if_else(size_t false_target, int line);
void codegen_if_end(size_t target);
/* Loop contexts borrow symbols and remain on the parser stack while parsing bodies. */
codegen_loop_t codegen_for_begin(map_node_t *variable, map_node_t *initial,
                                map_node_t *final, map_node_t *step, int line);
void codegen_for_end(codegen_loop_t loop);
void codegen_free(rpn_t *fragment);

#endif
