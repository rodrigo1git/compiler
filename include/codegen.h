#ifndef CODEGEN_H
#define CODEGEN_H

#include "rpn.h"

typedef struct codegen_function codegen_function_t;
typedef struct codegen_arguments codegen_arguments_t;
typedef struct codegen_order codegen_order_t;
typedef struct codegen_call codegen_call_t;
typedef struct {
    map_node_t *object;
    map_node_t *index;
    int line;
} codegen_attribute_t;

typedef struct {
    map_node_t *variable;
    map_node_t *step;
    size_t condition_target;
    size_t exit_target;
    rpn_operator_t update_operator;
    int source_line;
} codegen_loop_t;

void codegen_init(void);
void codegen_destroy(void);
bool codegen_print(FILE *stream);
bool codegen_finalize(void);

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
codegen_arguments_t *codegen_arguments_append(codegen_arguments_t *arguments, rpn_t *value);
void codegen_arguments_free(codegen_arguments_t *arguments);
codegen_order_t *codegen_order_append(codegen_order_t *order, map_node_t *constant);
void codegen_order_free(codegen_order_t *order);
codegen_call_t *codegen_call_begin(map_node_t *function, map_node_t *receiver,
                                 codegen_arguments_t *arguments, int line);
void codegen_call_free(codegen_call_t *call);
rpn_t *codegen_call_end(codegen_call_t *call, codegen_order_t *order);
codegen_attribute_t codegen_attribute(map_node_t *object, map_node_t *index, int line);
rpn_t *codegen_attribute_value(codegen_attribute_t attribute);
void codegen_attribute_assignment(codegen_attribute_t attribute, rpn_t *value);

#endif
