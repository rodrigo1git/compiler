#ifndef SPECIALIZE_H
#define SPECIALIZE_H

#include "comptime_values.h"

void specialize_init(void);
void specialize_register(map_node_t *symbol, const rpn_t *body);
bool specialize_program(const rpn_t *program);
bool specialize_print(FILE *stream);
void specialize_destroy(void);
comptime_value_t specialize_evaluate_call(const map_node_t *symbol,
    comptime_environment_t *environment, const comptime_environment_t *arguments,
    const map_node_t *receiver, int line);
size_t specialize_source_count(void);
const rpn_t *specialize_source_body(size_t index, map_node_t **symbol);
size_t specialize_instance_count(void);
const rpn_t *specialize_instance_body(size_t index, map_node_t **symbol);
const rpn_t *specialize_main_body(void);

#endif
