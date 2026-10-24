#ifndef SCOPE_H
#define SCOPE_H

#include "symbol_table.h"

typedef struct scope scope_t;
typedef struct scope_names scope_names_t;

typedef struct {
    const char *name;
    map_node_t *class_symbol;
} scope_type_t;

typedef enum {
    SCOPE_VALUE,
    SCOPE_CALLABLE,
    SCOPE_CLASS
} scope_reference_t;

void scope_init(void);
void scope_destroy(void);
void scope_program(map_node_t *name, int line);
scope_t *scope_begin(map_node_t *name, symbol_usage_t usage, scope_type_t type, int line);
void scope_end(scope_t *scope);
map_node_t *scope_symbol(const scope_t *scope);
map_node_t *scope_lookup_local(scope_t *scope, const char *name);
bool scope_in_function(void);
scope_type_t scope_named_type(map_node_t *name, int line);
scope_names_t *scope_names_create(map_node_t *name, int line);
scope_names_t *scope_names_append(scope_names_t *names, map_node_t *name, int line);
void scope_names_free(scope_names_t *names);
void scope_declare_names(scope_names_t *names, scope_type_t type, bool is_comptime);
void scope_base(map_node_t *name, int line);
map_node_t *scope_object(map_node_t *name, int line);
map_node_t *scope_parameter(map_node_t *name, scope_type_t type, bool is_comptime, int line);
map_node_t *scope_resolve(map_node_t *name, scope_reference_t reference, int line);

#endif
