#ifndef COMPTIME_VALUES_H
#define COMPTIME_VALUES_H

#include "rpn.h"

typedef struct {
    bool known;
    double number;
    bool floating;
    const map_node_t *constant;
    const map_node_t *object_type;
    const map_node_t *object;
    const map_node_t *object_snapshot;
} comptime_value_t;

typedef struct {
    const map_node_t *symbol;
    const map_node_t *object;
    comptime_value_t value;
} comptime_binding_t;

typedef struct {
    comptime_binding_t *items;
    size_t count;
} comptime_environment_t;

bool comptime_values_equal(comptime_value_t left, comptime_value_t right);
comptime_value_t comptime_get(const comptime_environment_t *environment, const map_node_t *symbol,
                             const map_node_t *object);
void comptime_set(comptime_environment_t *environment, const map_node_t *symbol,
                  const map_node_t *object, comptime_value_t value);
comptime_environment_t comptime_copy_environment(const comptime_environment_t *source);
void comptime_forget_runtime(comptime_environment_t *environment);
comptime_environment_t comptime_join_environments(const comptime_environment_t *left,
                                                  const comptime_environment_t *right);
const map_node_t *comptime_make_constant(comptime_value_t value, int line);
comptime_value_t comptime_read_constant(const map_node_t *symbol);
comptime_value_t comptime_binary(comptime_value_t left, comptime_value_t right, rpn_operator_t operator);

comptime_value_t comptime_to_integer(comptime_value_t value);

#endif
