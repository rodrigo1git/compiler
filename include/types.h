#ifndef TYPES_H
#define TYPES_H
#include "rpn.h"
#include "comptime_values.h"
typedef enum {
    TYPE_ERROR,
    TYPE_INTEGER,
    TYPE_SINGLEF,
    TYPE_CHAIN,
    TYPE_OBJECT,
    /* Candidate field types for a variable-position attribute. */
    TYPE_CANDIDATES,
    /* Depends on comptime arguments; also propagates through expressions. */
    TYPE_COMPTIME_PENDING
} type_kind_t;
typedef struct { type_kind_t kind; const map_node_t *class_symbol; } type_t;
type_t type_of_symbol(const map_node_t *symbol);
type_t type_of_attribute(const rpn_step_t *step);
const char *type_name(type_t type);
bool types_compatible(type_t destination, type_t source);
bool types_check_body(const rpn_t *body, const map_node_t *function);
bool types_check_discarded(const rpn_t *body, const map_node_t *function,
                           size_t begin, size_t end,
                           const comptime_environment_t *environment,
                           const map_node_t *receiver);
bool types_check_sources(const rpn_t *main_body);
bool types_check_specializations(void);
#endif
