#ifndef CLASSES_H
#define CLASSES_H

#include "symbol_table.h"

typedef enum {
    CLASS_ALL_MEMBERS,
    CLASS_INHERITED_MEMBERS
} class_search_t;

typedef struct {
    map_node_t *symbol;
    bool ambiguous;
    bool cyclic;
} class_member_t;

class_member_t class_lookup_member(map_node_t *class_symbol, const char *name, class_search_t search);
void class_set_code(map_node_t *class_symbol, map_node_t *code);
void class_add_base(map_node_t *class_symbol, map_node_t *base);
class_member_t class_inherited_member(map_node_t *class_symbol, const char *name, int line);
map_node_t *class_resolve_method(map_node_t *object, map_node_t *name, int line);
bool class_build_layout(map_node_t *class_symbol, int line);
bool class_same_layout(const map_node_t *left, const map_node_t *right);
void class_complete(map_node_t *class_symbol, int line);
map_node_t *class_attribute_at(map_node_t *class_symbol, size_t position, int line);
bool class_check_copy(const map_node_t *destination, const map_node_t *source, int line);

bool class_has_comptime(const map_node_t *class_symbol);

#endif
